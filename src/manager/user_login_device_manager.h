/**
 * @file user_login_device_manager.h
 * @brief 用户登录设备管理器 — 设备登录记录 + 双槽位（移动端/PC端）单登录
 * @author Christins
 * @date 2026-08-27
 * @copyright Apache 2.0
 */
#pragma once

#include "blog/data/user_login_device_info.h"

#include <chen/db/query_builder.h>
#include <chen/util/singleton.h>

namespace blog {

struct LoginParam {
    int64_t uid;
    std::string device_id;
    std::string platform;
    std::string device_name;
    std::string user_agent;
    std::string ip;
    std::string token;
    int64_t token_time;
};

class UserLoginDeviceManager {
public:
    UserLoginDeviceManager();

    /**
     * @brief 记录一次登录（插入一条活跃设备记录并缓存 token）
     * @return 0 成功，负数失败
     */
    int recordLogin(const LoginParam& param);

    /**
     * @brief 校验 token 是否属于 uid 且未过期（Redis 优先，DB 兜底）
     * @return true 有效，false 无效
     */
    bool validateToken(int64_t uid, const std::string& token, int64_t now);

    /**
     * @brief 按 token 查询设备记录（不校验活跃状态）
     */
    data::UserLoginDeviceInfo::ptr getByToken(const std::string& token);

    /**
     * @brief 查询某设备当前活跃记录（任意用户）
     */
    data::UserLoginDeviceInfo::ptr getActiveByDevice(const std::string& device_id);

    /**
     * @brief 查询某用户某平台当前活跃记录
     */
    data::UserLoginDeviceInfo::ptr getActiveByUserAndPlatform(int64_t uid, const std::string& platform);

    /**
     * @brief 顶掉某条设备记录（标记 inactive + 删除 Redis token 缓存）
     * @return 0 成功，负数失败
     */
    int kick(data::UserLoginDeviceInfo::ptr info);

    /**
     * @brief 退出当前设备（按 token 定位并顶掉）
     * @return 0 成功，负数失败
     */
    int logout(const std::string& token);

    /**
     * @brief 撤销某用户全部活跃设备会话（改密、封禁、删除等场景）
     * @return 0 成功，负数失败
     */
    int revokeAllByUser(int64_t uid);

    /**
     * @brief 刷新设备会话 token
     * @return 0 成功，负数失败
     */
    int updateToken(const std::string& old_token, const std::string& new_token, int64_t new_token_time);

    /**
     * @brief 列出用户当前活跃设备（按最近活跃时间倒序）
     */
    int listActiveByUser(std::vector<data::UserLoginDeviceInfo::ptr>& results, int64_t uid);

private:
    void cacheToken(const std::string& token, int64_t uid, int64_t token_time);
    void removeTokenCache(const std::string& token);
};

typedef chen::Singleton<UserLoginDeviceManager> UserLoginDeviceMgr;

} // namespace blog
