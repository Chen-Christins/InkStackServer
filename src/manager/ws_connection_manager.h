/**
 * @file ws_connection_manager.h
 * @brief 统一 WebSocket 连接管理（一设备一连接）
 * @author Christins
 * @date 2026-11-11
 * @copyright Apache 2.0
 */
#pragma once

#include <chen/http/ws_session.h>
#include <chen/util/singleton.h>

#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace blog {

/**
 * @brief 统一 WebSocket 连接管理器
 * @details 以 device_id 为主键管理连接，一个设备只保留一条长连接。
 *          支持同一用户多设备、按用户/设备/会话发送、在线人数统计和连接关闭。
 */
class WSConnectionManager {
public:
    /**
     * @brief 添加连接结果
     */
    struct AddResult {
        /// 用户是否从离线变为在线（首个设备接入）
        bool user_became_online = false;
        /// 在线人数是否发生变化（首个设备上线或最后一个设备下线）
        bool presence_changed = false;
        /// 被新连接替换的旧连接（同一 device_id 已存在时）
        chen::http::WSSession::ptr replaced_session;
    };

    /**
     * @brief 移除连接结果
     */
    struct RemoveResult {
        /// 是否确实移除了连接
        bool removed = false;
        /// 连接所属用户
        int64_t user_id = 0;
        /// 用户是否从在线变为离线（最后一个设备断开）
        bool user_became_offline = false;
    };

    /**
     * @brief 注册设备连接
     * @param user_id 用户 ID
     * @param device_id 设备标识（SESSIONID 的 SHA-256）
     * @param session WebSocket 会话
     * @param broadcast_presence 是否立即广播在线人数（Gateway 需先发送 connected 时可传 false）
     * @return 添加结果；同一 device_id 重复连接时返回旧 session
     */
    AddResult addConnection(int64_t user_id, const std::string& device_id, chen::http::WSSession::ptr session, bool broadcast_presence = true);

    /**
     * @brief 按会话移除连接
     * @details 只有当前注册的 session 与传入 session 一致时才移除，
     *          避免被替换的旧连接 onClose 误删新连接。
     */
    RemoveResult removeConnection(chen::http::WSSession::ptr session);

    /**
     * @brief 通过连接反查身份
     * @return 是否找到连接
     */
    bool getIdentity(chen::http::WSSession::ptr session, int64_t& user_id, std::string& device_id) const;

    /**
     * @brief 推送消息给用户的所有设备
     * @return 0 表示至少一个设备发送成功，-1 表示用户不在线或全部失败
     */
    int32_t sendToUser(int64_t user_id, const std::string& message);

    /**
     * @brief 推送消息给指定连接
     * @return 0 成功，-1 连接不存在或发送失败
     */
    int32_t sendToSession(chen::http::WSSession::ptr session, const std::string& message);

    /**
     * @brief 推送消息给指定设备
     * @return 0 成功，-1 设备不在线或发送失败
     */
    int32_t sendToDevice(const std::string& device_id, const std::string& message);

    /**
     * @brief 广播消息给所有设备
     */
    void broadcast(const std::string& message);

    /**
     * @brief 用户是否有任意设备在线
     */
    bool isUserOnline(int64_t user_id) const;

    /**
     * @brief 指定设备是否在线
     */
    bool isDeviceOnline(const std::string& device_id) const;

    /**
     * @brief 在线用户数（按用户去重）
     */
    int32_t onlineUserCount() const;

    /**
     * @brief 在线设备连接数
     */
    int32_t connectionCount() const;

    /**
     * @brief 关闭指定设备连接
     * @param notify_message 关闭前发送的消息，为空则不发送
     * @return 是否存在并关闭
     */
    bool closeDevice(const std::string& device_id, const std::string& notify_message = "");

    /**
     * @brief 关闭指定连接（发送关闭原因帧后关闭 socket，不修改连接表）
     * @param session 目标连接
     * @param notify_message 关闭前发送的消息，为空则不发送
     * @return 连接是否存在并关闭
     */
    bool closeSession(chen::http::WSSession::ptr session, const std::string& notify_message = "");

    /**
     * @brief 关闭指定用户的所有设备连接
     * @param notify_message 关闭前发送的消息，为空则不发送
     * @return 是否关闭了至少一条连接
     */
    bool closeUser(int64_t user_id, const std::string& notify_message = "");

    /**
     * @brief 广播当前在线人数
     */
    void broadcastPresence();

    /**
     * @brief 关闭所有连接（模块停用/热重载）
     * @param notify_message 关闭前发送的消息，为空则不发送
     * @return 关闭的连接数
     */
    int32_t closeAllConnections(const std::string& notify_message = "");

private:
    /**
     * @brief 单设备连接
     */
    struct Connection {
        typedef std::shared_ptr<Connection> ptr;

        int64_t user_id = 0;
        std::string device_id;
        chen::http::WSSession::ptr session;
        /// 同一 socket 上的通知/聊天发送串行化
        std::mutex send_mutex;
    };

    /**
     * @brief 发送消息到单条连接
     */
    static int32_t SendToConnection(const Connection::ptr& conn, const std::string& message);

    /**
     * @brief 发送关闭原因帧然后关闭连接（串行化发送与关闭）
     */
    static void CloseConnection(const Connection::ptr& conn, const std::string& notify_message);

private:
    /// device_id -> connection
    std::unordered_map<std::string, Connection::ptr> m_devices;
    /// user_id -> device_ids
    std::unordered_map<int64_t, std::unordered_set<std::string>> m_userDevices;
    /// session -> connection（被顶掉的旧连接保留到 onClose，便于发送 replaced 帧）
    std::unordered_map<const chen::http::WSSession*, Connection::ptr> m_sessionDevices;

    mutable std::shared_mutex m_mutex;
};

/**
 * @brief 为下行 JSON 添加 channel 字段
 * @param message 原始 JSON 字符串
 * @param channel system/notification/presence/chat
 * @return 添加 channel 后的 JSON；解析失败时原样返回
 */
std::string BuildChannelMessage(const std::string& message, const std::string& channel);

typedef chen::Singleton<WSConnectionManager> WSMgr;

} // namespace blog
