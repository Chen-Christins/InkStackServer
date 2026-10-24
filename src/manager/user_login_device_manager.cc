#include "user_login_device_manager.h"

#include "../util.h"

#include <chen/config/config.h>
#include <chen/db/redis.h>
#include <chen/log/log.h>
#include <chen/util/time_util.h>

namespace blog {

static chen::Logger::ptr logger = LOG_ROOT();

/// Redis token 缓存 key 前缀
static const char* kTokenKeyPrefix = "login_token:";

static chen::ConfigVar<std::string>::ptr g_redis_pool_name = chen::Config::Lookup("redis.name", std::string("blog"), "Redis connection pool name");

UserLoginDeviceManager::UserLoginDeviceManager()
{
}

void UserLoginDeviceManager::cacheToken(const std::string& token, int64_t uid, int64_t token_time)
{
    if (token.empty()) {
        return;
    }
    int64_t ttl = token_time - chen::TimeUtil::GetCurrentSec();
    if (ttl <= 0) {
        ttl = 1;
    }
    std::string key = std::string(kTokenKeyPrefix) + token;
    chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "setex %s %lld %lld", key.c_str(), (long long)ttl, (long long)uid);
}

void UserLoginDeviceManager::removeTokenCache(const std::string& token)
{
    if (token.empty()) {
        return;
    }
    std::string key = std::string(kTokenKeyPrefix) + token;
    chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "del %s", key.c_str());
}

int UserLoginDeviceManager::recordLogin(const LoginParam& param)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "recordLogin: GetDB failed";
        return -1;
    }
    auto info = std::make_shared<data::UserLoginDeviceInfo>();
    info->setUserId(param.uid);
    info->setDeviceId(param.device_id);
    info->setPlatform(param.platform);
    info->setDeviceName(param.device_name);
    info->setUserAgent(param.user_agent);
    info->setIp(param.ip);
    info->setToken(param.token);
    info->setTokenTime(param.token_time);
    info->setIsActive(1);
    info->setLoginTime(chen::TimeUtil::GetCurrentSec());
    info->setLastActiveTime(chen::TimeUtil::GetCurrentSec());
    info->setLogoutTime(0);
    if (data::UserLoginDeviceInfoDao::Insert(info, db)) {
        ERROR(logger) << "recordLogin: Insert failed"
                      << ", errstr=" << db->getErrStr() << ", errno=" << db->getErrno();
        return -1;
    }
    cacheToken(param.token, param.uid, param.token_time);
    return 0;
}

data::UserLoginDeviceInfo::ptr UserLoginDeviceManager::getByToken(const std::string& token)
{
    if (token.empty()) {
        return nullptr;
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "getByToken: GetDB failed";
        return nullptr;
    }
    return data::UserLoginDeviceInfoDao::QueryByToken(token, db);
}

bool UserLoginDeviceManager::validateToken(int64_t uid, const std::string& token, int64_t now)
{
    if (!uid || token.empty()) {
        return false;
    }
    std::string key = std::string(kTokenKeyPrefix) + token;
    auto rpy = chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "get %s", key.c_str());
    if (rpy && rpy->type == REDIS_REPLY_STRING && rpy->str) {
        std::string val(rpy->str, rpy->len);
        return chen::TypeUtil::Atoi(val) == uid;
    }

    // Redis 未命中，DB 兜底
    auto info = getByToken(token);
    if (info && info->getIsActive() == 1 && info->getUserId() == uid && info->getTokenTime() > now) {
        cacheToken(token, uid, info->getTokenTime());
        return true;
    }
    return false;
}

data::UserLoginDeviceInfo::ptr UserLoginDeviceManager::getActiveByDevice(const std::string& device_id)
{
    if (device_id.empty()) {
        return nullptr;
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "getActiveByDevice: GetDB failed";
        return nullptr;
    }
    std::vector<data::UserLoginDeviceInfo::ptr> results;
    if (data::UserLoginDeviceInfoDao::QueryByDeviceId(results, device_id, db)) {
        return nullptr;
    }
    for (auto& info : results) {
        if (info->getIsActive() == 1) {
            return info;
        }
    }
    return nullptr;
}

data::UserLoginDeviceInfo::ptr UserLoginDeviceManager::getActiveByUserAndPlatform(int64_t uid, const std::string& platform)
{
    if (!uid || platform.empty()) {
        return nullptr;
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "getActiveByUserAndPlatform: GetDB failed";
        return nullptr;
    }
    std::vector<data::UserLoginDeviceInfo::ptr> results;
    if (data::UserLoginDeviceInfoDao::QueryByUserIdPlatform(results, uid, platform, db)) {
        return nullptr;
    }
    for (auto& info : results) {
        if (info->getIsActive() == 1) {
            return info;
        }
    }
    return nullptr;
}

int UserLoginDeviceManager::kick(data::UserLoginDeviceInfo::ptr info)
{
    if (!info) {
        return -1;
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "kick: GetDB failed";
        return -1;
    }
    info->setIsActive(0);
    info->setLogoutTime(chen::TimeUtil::GetCurrentSec());
    info->setUpdateTime(chen::TimeUtil::GetCurrentSec());
    if (data::UserLoginDeviceInfoDao::Update(info, db)) {
        ERROR(logger) << "kick: Update failed"
                      << ", errstr=" << db->getErrStr() << ", errno=" << db->getErrno();
        return -1;
    }
    removeTokenCache(info->getToken());
    return 0;
}

int UserLoginDeviceManager::logout(const std::string& token)
{
    auto info = getByToken(token);
    if (!info || info->getIsActive() != 1) {
        return -1;
    }
    return kick(info);
}

int UserLoginDeviceManager::revokeAllByUser(int64_t uid)
{
    if (!uid) {
        return -1;
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "revokeAllByUser: GetDB failed";
        return -1;
    }

    auto qb = data::UserLoginDeviceInfoDao::newQuery();
    qb->where("user_id", "=", uid);
    qb->where("is_active", "=", (int64_t)1);

    std::vector<data::UserLoginDeviceInfo::ptr> results;
    if (data::UserLoginDeviceInfoDao::QueryByBuilder(results, qb, db)) {
        ERROR(logger) << "revokeAllByUser: QueryByBuilder failed"
                      << ", errstr=" << db->getErrStr() << ", errno=" << db->getErrno();
        return -1;
    }

    for (auto& info : results) {
        if (info) {
            kick(info);
        }
    }
    return 0;
}

int UserLoginDeviceManager::updateToken(const std::string& old_token, const std::string& new_token, int64_t new_token_time)
{
    auto info = getByToken(old_token);
    if (!info || info->getIsActive() != 1) {
        return -1;
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "updateToken: GetDB failed";
        return -1;
    }
    info->setToken(new_token);
    info->setTokenTime(new_token_time);
    info->setLastActiveTime(chen::TimeUtil::GetCurrentSec());
    info->setUpdateTime(chen::TimeUtil::GetCurrentSec());
    if (data::UserLoginDeviceInfoDao::Update(info, db)) {
        ERROR(logger) << "updateToken: Update failed"
                      << ", errstr=" << db->getErrStr() << ", errno=" << db->getErrno();
        return -1;
    }
    removeTokenCache(old_token);
    cacheToken(new_token, info->getUserId(), new_token_time);
    return 0;
}

int UserLoginDeviceManager::listActiveByUser(std::vector<data::UserLoginDeviceInfo::ptr>& results, int64_t uid)
{
    if (!uid) {
        return 0;
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "listActiveByUser: GetDB failed";
        return 0;
    }
    auto qb = data::UserLoginDeviceInfoDao::newQuery();
    qb->where("user_id", "=", uid);
    qb->where("is_active", "=", (int64_t)1);
    qb->orderBy("last_active_time", "DESC");
    if (data::UserLoginDeviceInfoDao::QueryByBuilder(results, qb, db)) {
        ERROR(logger) << "listActiveByUser: QueryByBuilder failed"
                      << ", errstr=" << db->getErrStr() << ", errno=" << db->getErrno();
        return 0;
    }
    return (int)results.size();
}

} // namespace blog
