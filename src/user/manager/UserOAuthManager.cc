#include "user/manager/UserOAuthManager.hpp"

#include "../../common/util/Util.hpp"

#include <chen/log/log.h>

namespace blog {

static chen::Logger::ptr logger = LOG_ROOT();

UserOAuthManager::UserOAuthManager()
{
}

data::UserOauthInfo::ptr UserOAuthManager::getByOpenId(const std::string& provider, const std::string& open_id)
{
    if (provider.empty() || open_id.empty()) {
        return nullptr;
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "getByOpenId: GetDB failed";
        return nullptr;
    }
    std::vector<data::UserOauthInfo::ptr> results;
    auto qb = data::UserOauthInfoDao::newQuery();
    qb->where("provider", "=", provider);
    qb->where("open_id", "=", open_id);
    if (data::UserOauthInfoDao::QueryByBuilder(results, qb, db)) {
        ERROR(logger) << "getByOpenId: query failed"
                      << ", errstr=" << db->getErrStr() << ", errno=" << db->getErrno();
        return nullptr;
    }
    if (results.empty()) {
        return nullptr;
    }
    return results[0];
}

data::UserOauthInfo::ptr UserOAuthManager::getByUser(int64_t user_id, const std::string& provider)
{
    if (!user_id || provider.empty()) {
        return nullptr;
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "getByUser: GetDB failed";
        return nullptr;
    }
    std::vector<data::UserOauthInfo::ptr> results;
    auto qb = data::UserOauthInfoDao::newQuery();
    qb->where("user_id", "=", user_id);
    qb->where("provider", "=", provider);
    if (data::UserOauthInfoDao::QueryByBuilder(results, qb, db)) {
        ERROR(logger) << "getByUser: query failed"
                      << ", errstr=" << db->getErrStr() << ", errno=" << db->getErrno();
        return nullptr;
    }
    if (results.empty()) {
        return nullptr;
    }
    return results[0];
}

int UserOAuthManager::bind(int64_t user_id, const std::string& provider, const std::string& open_id, const std::string& login_name)
{
    if (!user_id || provider.empty() || open_id.empty()) {
        return -1;
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "bind: GetDB failed";
        return -1;
    }
    auto info = std::make_shared<data::UserOauthInfo>();
    info->setUserId(user_id);
    info->setProvider(provider);
    info->setOpenId(open_id);
    info->setLoginName(login_name);
    info->setCreateTime(time(0));
    info->setUpdateTime(time(0));
    if (data::UserOauthInfoDao::Insert(info, db)) {
        ERROR(logger) << "bind: Insert failed"
                      << ", errstr=" << db->getErrStr() << ", errno=" << db->getErrno();
        return -1;
    }
    return 0;
}

} // namespace blog
