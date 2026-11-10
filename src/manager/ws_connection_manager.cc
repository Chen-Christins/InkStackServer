#include "ws_connection_manager.h"

#include <chen/log/log.h>
#include <chen/util/json_util.h>

#include <json/json.h>

namespace blog {

static chen::Logger::ptr logger = LOG_ROOT();

std::string BuildChannelMessage(const std::string& message, const std::string& channel)
{
    if (message.empty() || channel.empty()) {
        return message;
    }

    Json::Value root;
    if (!chen::JsonUtil::FromString(root, message) || !root.isObject()) {
        return message;
    }
    root["channel"] = channel;
    return chen::JsonUtil::ToString(root);
}

WSConnectionManager::AddResult WSConnectionManager::addConnection(
    int64_t user_id,
    const std::string& device_id,
    chen::http::WSSession::ptr session,
    bool broadcast_presence)
{
    AddResult result;
    if (user_id <= 0 || device_id.empty() || !session) {
        ERROR(logger) << "WS addConnection invalid param: user_id=" << user_id << " device_id=" << device_id;
        return result;
    }

    bool user_became_online = false;
    bool old_user_became_offline = false;

    {
        std::unique_lock lock(m_mutex);

        auto dit = m_devices.find(device_id);
        if (dit != m_devices.end()) {
            auto old_conn = dit->second;
            if (old_conn && old_conn->session == session) {
                // 同一个连接重复注册，直接返回
                return result;
            }
            if (old_conn) {
                result.replaced_session = old_conn->session;
                // 暂不移除旧 session 映射，Gateway 还需要通过它发送 replaced 帧；
                // 旧连接 onClose 时 removeConnection 会清理。
                auto old_uit = m_userDevices.find(old_conn->user_id);
                if (old_uit != m_userDevices.end()) {
                    old_uit->second.erase(device_id);
                    if (old_uit->second.empty()) {
                        m_userDevices.erase(old_uit);
                        old_user_became_offline = true;
                    }
                }
            }
            m_devices.erase(dit);
        }

        auto conn = std::make_shared<Connection>();
        conn->user_id = user_id;
        conn->device_id = device_id;
        conn->session = session;

        auto& devices = m_userDevices[user_id];
        user_became_online = devices.empty();
        devices.insert(device_id);
        m_devices[device_id] = conn;
        m_sessionDevices[session.get()] = conn;
    }

    if (user_became_online || old_user_became_offline) {
        result.presence_changed = true;
        if (broadcast_presence) {
            broadcastPresence();
        }
    }

    result.user_became_online = user_became_online;
    if (result.replaced_session) {
        INFO(logger) << "WS replace: user_id=" << user_id << " device=" << device_id.substr(0, 12);
    } else {
        INFO(logger) << "WS connected: user_id=" << user_id << " device=" << device_id.substr(0, 12);
    }
    return result;
}

WSConnectionManager::RemoveResult WSConnectionManager::removeConnection(chen::http::WSSession::ptr session)
{
    RemoveResult result;
    if (!session) {
        return result;
    }

    {
        std::unique_lock lock(m_mutex);

        auto sit = m_sessionDevices.find(session.get());
        if (sit == m_sessionDevices.end()) {
            return result;
        }

        auto conn = sit->second;
        if (!conn) {
            m_sessionDevices.erase(sit);
            return result;
        }

        auto dit = m_devices.find(conn->device_id);
        if (dit == m_devices.end() || dit->second != conn) {
            // 该连接已被同设备的新连接顶掉，只清理旧 session 映射
            m_sessionDevices.erase(sit);
            return result;
        }

        m_sessionDevices.erase(sit);
        m_devices.erase(dit);

        result.removed = true;
        result.user_id = conn->user_id;

        auto uit = m_userDevices.find(conn->user_id);
        if (uit != m_userDevices.end()) {
            uit->second.erase(conn->device_id);
            if (uit->second.empty()) {
                m_userDevices.erase(uit);
                result.user_became_offline = true;
            }
        }
    }

    if (result.user_became_offline) {
        broadcastPresence();
    }

    INFO(logger) << "WS disconnected: user_id=" << result.user_id;
    return result;
}

bool WSConnectionManager::getIdentity(chen::http::WSSession::ptr session, int64_t& user_id, std::string& device_id) const
{
    if (!session) {
        return false;
    }

    std::shared_lock lock(m_mutex);
    auto sit = m_sessionDevices.find(session.get());
    if (sit == m_sessionDevices.end() || !sit->second) {
        return false;
    }

    auto conn = sit->second;
    auto dit = m_devices.find(conn->device_id);
    if (dit == m_devices.end() || dit->second != conn) {
        return false;
    }

    user_id = conn->user_id;
    device_id = conn->device_id;
    return true;
}

int32_t WSConnectionManager::sendToUser(int64_t user_id, const std::string& message)
{
    std::vector<Connection::ptr> conns;
    {
        std::shared_lock lock(m_mutex);
        auto uit = m_userDevices.find(user_id);
        if (uit == m_userDevices.end()) {
            return -1;
        }

        conns.reserve(uit->second.size());
        for (const auto& device_id : uit->second) {
            auto dit = m_devices.find(device_id);
            if (dit != m_devices.end() && dit->second) {
                conns.push_back(dit->second);
            }
        }
    }

    bool sent = false;
    for (const auto& conn : conns) {
        if (SendToConnection(conn, message) == 0) {
            sent = true;
        }
    }
    return sent ? 0 : -1;
}

int32_t WSConnectionManager::sendToSession(chen::http::WSSession::ptr session, const std::string& message)
{
    if (!session) {
        return -1;
    }

    Connection::ptr conn;
    {
        std::shared_lock lock(m_mutex);
        auto sit = m_sessionDevices.find(session.get());
        if (sit == m_sessionDevices.end() || !sit->second) {
            return -1;
        }
        conn = sit->second;
    }

    return SendToConnection(conn, message);
}

int32_t WSConnectionManager::sendToDevice(const std::string& device_id, const std::string& message)
{
    Connection::ptr conn;
    {
        std::shared_lock lock(m_mutex);
        auto dit = m_devices.find(device_id);
        if (dit == m_devices.end() || !dit->second) {
            return -1;
        }
        conn = dit->second;
    }

    return SendToConnection(conn, message);
}

void WSConnectionManager::broadcast(const std::string& message)
{
    std::vector<Connection::ptr> conns;
    {
        std::shared_lock lock(m_mutex);
        conns.reserve(m_devices.size());
        for (const auto& item : m_devices) {
            if (item.second) {
                conns.push_back(item.second);
            }
        }
    }

    for (const auto& conn : conns) {
        SendToConnection(conn, message);
    }
}

bool WSConnectionManager::isUserOnline(int64_t user_id) const
{
    if (user_id <= 0) {
        return false;
    }

    std::shared_lock lock(m_mutex);
    auto uit = m_userDevices.find(user_id);
    if (uit == m_userDevices.end()) {
        return false;
    }

    for (const auto& device_id : uit->second) {
        auto dit = m_devices.find(device_id);
        if (dit != m_devices.end() && dit->second && dit->second->session && dit->second->session->isConnected()) {
            return true;
        }
    }
    return false;
}

bool WSConnectionManager::isDeviceOnline(const std::string& device_id) const
{
    if (device_id.empty()) {
        return false;
    }

    std::shared_lock lock(m_mutex);
    auto dit = m_devices.find(device_id);
    return dit != m_devices.end() && dit->second && dit->second->session && dit->second->session->isConnected();
}

int32_t WSConnectionManager::onlineUserCount() const
{
    std::shared_lock lock(m_mutex);
    return static_cast<int32_t>(m_userDevices.size());
}

int32_t WSConnectionManager::connectionCount() const
{
    std::shared_lock lock(m_mutex);
    return static_cast<int32_t>(m_devices.size());
}

bool WSConnectionManager::closeSession(chen::http::WSSession::ptr session, const std::string& notify_message)
{
    if (!session) {
        return false;
    }

    Connection::ptr conn;
    {
        std::shared_lock lock(m_mutex);
        auto sit = m_sessionDevices.find(session.get());
        if (sit == m_sessionDevices.end() || !sit->second) {
            return false;
        }
        conn = sit->second;
    }

    CloseConnection(conn, notify_message);
    return true;
}

bool WSConnectionManager::closeDevice(const std::string& device_id, const std::string& notify_message)
{
    if (device_id.empty()) {
        return false;
    }

    Connection::ptr conn;
    bool user_became_offline = false;
    {
        std::unique_lock lock(m_mutex);
        auto dit = m_devices.find(device_id);
        if (dit == m_devices.end()) {
            return false;
        }

        conn = dit->second;
        m_devices.erase(dit);
        if (conn && conn->session) {
            m_sessionDevices.erase(conn->session.get());
        }
        if (conn) {
            auto uit = m_userDevices.find(conn->user_id);
            if (uit != m_userDevices.end()) {
                uit->second.erase(device_id);
                if (uit->second.empty()) {
                    m_userDevices.erase(uit);
                    user_became_offline = true;
                }
            }
        }
    }

    CloseConnection(conn, notify_message);

    if (user_became_offline) {
        broadcastPresence();
    }

    INFO(logger) << "WS close device: " << device_id.substr(0, 12);
    return true;
}

bool WSConnectionManager::closeUser(int64_t user_id, const std::string& notify_message)
{
    if (user_id <= 0) {
        return false;
    }

    std::vector<Connection::ptr> conns;
    {
        std::unique_lock lock(m_mutex);
        auto uit = m_userDevices.find(user_id);
        if (uit == m_userDevices.end()) {
            return false;
        }

        for (const auto& device_id : uit->second) {
            auto dit = m_devices.find(device_id);
            if (dit == m_devices.end() || !dit->second) {
                continue;
            }
            auto conn = dit->second;
            if (conn->session) {
                m_sessionDevices.erase(conn->session.get());
            }
            conns.push_back(conn);
            m_devices.erase(dit);
        }
        m_userDevices.erase(uit);
    }

    for (const auto& conn : conns) {
        CloseConnection(conn, notify_message);
    }

    if (!conns.empty()) {
        broadcastPresence();
    }

    INFO(logger) << "WS close user: user_id=" << user_id << " devices=" << conns.size();
    return !conns.empty();
}

int32_t WSConnectionManager::closeAllConnections(const std::string& notify_message)
{
    std::vector<Connection::ptr> conns;
    {
        std::unique_lock lock(m_mutex);
        conns.reserve(m_devices.size());
        for (const auto& item : m_devices) {
            if (item.second) {
                conns.push_back(item.second);
            }
        }
        m_devices.clear();
        m_userDevices.clear();
        m_sessionDevices.clear();
    }

    for (const auto& conn : conns) {
        CloseConnection(conn, notify_message);
    }

    INFO(logger) << "WS close all connections: " << conns.size();
    return static_cast<int32_t>(conns.size());
}

void WSConnectionManager::CloseConnection(const Connection::ptr& conn, const std::string& notify_message)
{
    if (!conn || !conn->session) {
        return;
    }

    std::lock_guard lock(conn->send_mutex);
    if (!notify_message.empty() && conn->session->isConnected()) {
        conn->session->sendMessage(notify_message);
    }
    conn->session->close();
}

int32_t WSConnectionManager::SendToConnection(const Connection::ptr& conn, const std::string& message)
{
    if (!conn || !conn->session || !conn->session->isConnected()) {
        return -1;
    }

    std::lock_guard lock(conn->send_mutex);
    if (!conn->session->isConnected()) {
        return -1;
    }
    // 框架返回写入字节数，统一归一化为 0 成功 / -1 失败
    return conn->session->sendMessage(message) < 0 ? -1 : 0;
}

void WSConnectionManager::broadcastPresence()
{
    Json::Value msg;
    msg["channel"] = "presence";
    msg["type"] = "online_count";
    msg["count"] = onlineUserCount();
    broadcast(chen::JsonUtil::ToString(msg));
}

} // namespace blog
