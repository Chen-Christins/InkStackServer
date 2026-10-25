/**
 * @file session_store.h
 * @brief 服务端会话存储 — Redis 持久化，SESSIONID Cookie 只保存随机 ID
 * @author Christins
 * @date 2026-10-23
 * @copyright Apache 2.0
 */
#pragma once

#include <string>
#include <vector>

namespace blog {

/**
 * @brief 会话元数据
 */
struct SessionInfo {
    /// 对外暴露的会话标识（原始 SESSIONID 的 SHA-256）
    std::string id;
    int64_t uid = 0;
    std::string ip;
    std::string user_agent;
    int64_t created_at = 0;
    int64_t last_seen = 0;
    int64_t expires_at = 0;
};

/**
 * @brief Redis 会话存储
 * @details key 使用 SESSIONID 的 SHA-256，避免 Redis 数据泄露后直接复用 Cookie。
 */
class SessionStore {
public:
    /**
     * @brief 创建会话
     * @param uid 用户 ID
     * @param ip 登录 IP
     * @param user_agent User-Agent
     * @param idle_ttl 空闲过期时间（秒）
     * @param absolute_ttl 绝对过期时间（秒），<=0 表示不限制
     * @return 原始 SESSIONID，失败返回空串
     */
    static std::string Create(int64_t uid, const std::string& ip, const std::string& user_agent, int64_t idle_ttl, int64_t absolute_ttl);

    /**
     * @brief 查询会话
     * @param session_id 原始 SESSIONID
     * @param info 输出会话信息
     * @return 是否存在且未过期
     */
    static bool Get(const std::string& session_id, SessionInfo& info);

    /**
     * @brief 刷新会话活跃时间与过期时间
     * @return 会话是否仍然有效
     */
    static bool Touch(const std::string& session_id, const SessionInfo& info, int64_t idle_ttl, int64_t absolute_ttl);

    /**
     * @brief 删除单个会话
     */
    static void Destroy(const std::string& session_id);

    /**
     * @brief 删除用户的全部会话
     */
    static void DestroyAll(int64_t uid);

    /**
     * @brief 删除属于指定用户的单个会话（按存储标识）
     * @return 是否存在并删除成功
     */
    static bool DestroyForUser(int64_t uid, const std::string& hash);

    /**
     * @brief 列出用户当前有效会话
     */
    static void List(int64_t uid, std::vector<SessionInfo>& sessions);

    /**
     * @brief 计算原始 SESSIONID 的存储标识
     */
    static std::string HashId(const std::string& session_id);

private:
    static void DestroyByHash(const std::string& hash, int64_t uid);
};

} // namespace blog
