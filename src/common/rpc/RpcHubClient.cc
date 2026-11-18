#include "common/rpc/RpcHubClient.hpp"

#include <chen/iomanager/iomanager.h>
#include <chen/log/log.h>
#include <chen/socket/address.h>
#include <chen/util/time_util.h>

namespace blog {
namespace rpc {

static chen::Logger::ptr logger = LOG_NAME("system");

namespace {

/// 重连间隔（毫秒）
constexpr uint64_t kReconnectIntervalMs = 5000;

/// 每个服务功能类型下只跑一个实例，实例号固定为 1
constexpr uint32_t kInstanceId = 1;

} // namespace

bool RpcHubClient::start(uint32_t func_id)
{
    bool ok = false;
    {
        std::lock_guard lock(m_mutex);
        m_func_id = func_id;
        m_peer_id = MakePeerId(func_id);
        ok = connectLocked();
    }

    auto iom = chen::IOManager::GetThis();
    if (!iom) {
        WARN(logger) << "no iomanager in current thread, rpc hub reconnect timer disabled";
        return ok;
    }

    std::lock_guard lock(m_mutex);
    if (!m_reconnect_timer) {
        m_reconnect_timer = iom->addTimer(kReconnectIntervalMs, std::bind(&RpcHubClient::onReconnectTimer, this), true);
    }
    return ok;
}

void RpcHubClient::stop()
{
    std::lock_guard lock(m_mutex);
    if (m_reconnect_timer) {
        m_reconnect_timer->cancel();
        m_reconnect_timer = nullptr;
    }
    if (m_client) {
        m_client->close();
        m_client = nullptr;
    }
    m_registrars.clear();
    m_func_id = 0;
    m_peer_id = 0;
}

bool RpcHubClient::isConnected()
{
    std::lock_guard lock(m_mutex);
    return m_client && m_client->isConnected();
}

bool RpcHubClient::ensureConnected()
{
    std::lock_guard lock(m_mutex);
    if (!m_func_id) {
        return false;
    }
    if (m_client && m_client->isConnected()) {
        return true;
    }

    const uint64_t now = chen::TimeUtil::GetCurrentMs();
    if (now - m_last_connect_ms < kReconnectIntervalMs) {
        return false;
    }
    m_last_connect_ms = now;
    return connectLocked();
}

chen::rpc::RpcClient::ptr RpcHubClient::connectedClient()
{
    std::lock_guard lock(m_mutex);
    if (m_client && m_client->isConnected()) {
        return m_client;
    }
    return nullptr;
}

void RpcHubClient::onReconnectTimer()
{
    ensureConnected();
}

void RpcHubClient::logCallFailure(uint32_t func_id, uint32_t cmd, const std::string& why)
{
    WARN(logger) << "rpc call failed, func_id=" << func_id << " cmd=" << cmd << " reason=" << why;
}

bool RpcHubClient::connectLocked()
{
    const std::string& hub = GetHubAddress();
    auto address = chen::Address::LookupAny(hub);
    if (!address) {
        WARN(logger) << "resolve rpc hub failed, address=" << hub;
        return false;
    }

    auto client = std::make_shared<chen::rpc::RpcClient>();
    if (!client->connect(address)) {
        WARN(logger) << "connect rpc hub failed, address=" << hub;
        return false;
    }

    if (!client->registerService(m_peer_id, m_func_id, kInstanceId, {})) {
        WARN(logger) << "register rpc service failed, hub=" << hub << " func_id=" << m_func_id << " peer_id=" << m_peer_id;
        client->close();
        return false;
    }

    m_client = client;
    for (auto& item : m_registrars) {
        item.second(m_client);
    }

    INFO(logger) << "rpc hub connected, hub=" << hub << " func_id=" << m_func_id << " peer_id=" << m_peer_id;
    return true;
}

} // namespace rpc
} // namespace blog
