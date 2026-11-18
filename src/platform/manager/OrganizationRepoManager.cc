#include "platform/manager/OrganizationRepoManager.hpp"

#include <chen/config/config.h>
#include <chen/db/redis.h>
#include <chen/http/http_connection.h>
#include <chen/http/uri.h>
#include <chen/log/log.h>
#include <chen/util/json_util.h>
#include <json/json.h>

#include "../../common/util/Util.hpp"
#include "common/manager/CacheUtil.hpp"

namespace blog {

static chen::Logger::ptr logger = LOG_ROOT();

static const int kCacheTTL = 300;
static const char* kCachePrefix = "cache:obj:org_repo:";

static std::string orgRepoCacheKey(int64_t id)
{
    return std::string(kCachePrefix) + std::to_string(id);
}

static data::OrganizationReposInfo::ptr parseOrgRepoCache(const Json::Value& v)
{
    auto info = std::make_shared<data::OrganizationReposInfo>();
    info->setId(cacheJsonInt64(v, "id"));
    info->setOrgId(cacheJsonInt64(v, "org_id"));
    info->setName(cacheJsonString(v, "name"));
    info->setDescription(cacheJsonString(v, "description"));
    info->setUrl(cacheJsonString(v, "url"));
    info->setLanguage(cacheJsonString(v, "language"));
    info->setStarsCount(cacheJsonInt32(v, "stars_count"));
    info->setSortOrder(cacheJsonInt32(v, "sort_order"));
    info->setCreateTime(cacheJsonTime(v, "create_time"));
    info->setUpdateTime(cacheJsonTime(v, "update_time"));
    info->setToken(cacheJsonString(v, "token"));
    info->setSyncStatus(cacheJsonString(v, "sync_status"));
    info->setPrSyncStatus(cacheJsonString(v, "pr_sync_status"));
    info->setPrSyncedAt(cacheJsonInt64(v, "pr_synced_at"));
    info->setGithubFullName(cacheJsonString(v, "github_full_name"));
    info->markClean();
    return info;
}

static data::OrganizationReposInfo::ptr getOrgRepoCache(int64_t id)
{
    std::string key = orgRepoCacheKey(id);
    std::string data = redisCacheGet(key);
    if (data.empty()) {
        return nullptr;
    }
    Json::Value v;
    if (!chen::JsonUtil::FromString(v, data) || !v.isObject()) {
        redisCacheDel(key);
        return nullptr;
    }
    return parseOrgRepoCache(v);
}

static void setOrgRepoCache(const data::OrganizationReposInfo::ptr& info)
{
    if (!info) {
        return;
    }
    redisCacheSetex(orgRepoCacheKey(info->getId()), info->toJsonString(), kCacheTTL);
}

static void delOrgRepoCache(int64_t id)
{
    redisCacheDel(orgRepoCacheKey(id));
}

static chen::ConfigVar<std::string>::ptr g_redis_pool_name = chen::Config::Lookup("redis.name", std::string("blog"), "Redis connection pool name");

data::OrganizationReposInfo::ptr OrganizationRepoManager::parseRow(chen::ISQLData::ptr rt)
{
    return data::OrganizationReposInfoDao::ParseRow(rt);
}

void OrganizationRepoManager::add(data::OrganizationReposInfo::ptr info)
{
    setOrgRepoCache(info);
    invalidateCountCache(info->getOrgId());
}

void OrganizationRepoManager::del(int64_t id)
{
    auto info = getOrgRepoCache(id);
    if (info) {
        invalidateCountCache(info->getOrgId());
    }
    delOrgRepoCache(id);
}

void OrganizationRepoManager::onTick()
{
    std::vector<data::OrganizationReposInfo::ptr> repos;
    getAllWithToken(repos);

    for (const auto& repo : repos) {
        if (!repo) {
            continue;
        }
        int64_t id = repo->getId();
        std::string url = repo->getUrl();
        std::string token = repo->getToken();

        SyncFromGitHub(id, url, token);
    }
}

data::OrganizationReposInfo::ptr OrganizationRepoManager::get(int64_t id)
{
    auto v = getOrgRepoCache(id);
    if (v) {
        return v;
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    v = data::OrganizationReposInfoDao::Query(id, db);
    if (v) {
        setOrgRepoCache(v);
    }
    return v;
}

data::OrganizationReposInfo::ptr OrganizationRepoManager::getByOrgAndName(int64_t org_id, const std::string& name)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    auto info = data::OrganizationReposInfoDao::QueryByOrgIdName(org_id, name, db);
    if (info) {
        setOrgRepoCache(info);
    }
    return info;
}

int64_t OrganizationRepoManager::listByOrgPages(std::vector<data::OrganizationReposInfo::ptr>& repos, int64_t org_id, uint64_t offset, uint64_t limit)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }

    auto qb = data::OrganizationReposInfoDao::newQuery();
    qb->where("org_id", "=", org_id);
    qb->orderBy("sort_order DESC, id", "DESC");

    int64_t total = 0;
    if (data::OrganizationReposInfoDao::QueryByBuilderPages(repos, total, qb, (int32_t)offset, (int32_t)limit, db)) {
        ERROR(logger) << "QueryByBuilderPages failed";
        return 0;
    }
    return total;
}

int64_t OrganizationRepoManager::getCountByOrg(int64_t org_id)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }

    auto qb = data::OrganizationReposInfoDao::newQuery();
    qb->where("org_id", "=", org_id);

    std::stringstream ck;
    ck << "org_repo:count:" << org_id;
    return executeCountCached(qb, db, ck.str());
}

void OrganizationRepoManager::getAllWithToken(std::vector<data::OrganizationReposInfo::ptr>& repos)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return;
    }
    auto qb = data::OrganizationReposInfoDao::newQuery();
    qb->where("token", "!=", "");
    if (data::OrganizationReposInfoDao::QueryByBuilder(repos, qb, db)) {
        ERROR(logger) << "QueryByBuilder failed";
    }
}

void OrganizationRepoManager::invalidateCountCache(int64_t org_id)
{
    {
        std::stringstream key;
        key << "cache:count:org_repo:count:" << org_id;
        chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "del %s", key.str().c_str());
    }
    {
        std::stringstream key;
        key << "cache:count:org_repo:list:" << org_id;
        chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "del %s", key.str().c_str());
    }
}

static std::string extractGithubRepo(const std::string& url)
{
    auto uri = chen::Uri::Create(url);
    if (!uri) {
        return "";
    }
    std::string path = uri->getPath();
    if (!path.empty() && path[0] == '/') {
        path = path.substr(1);
    }
    if (path.size() > 4 && path.substr(path.size() - 4) == ".git") {
        path = path.substr(0, path.size() - 4);
    }
    return path;
}

static void markSyncFailed(int64_t repo_id)
{
    auto repo = OrganizationRepoMgr::GetInstance()->get(repo_id);
    if (!repo) {
        return;
    }
    auto db = GetDB();
    if (!db) {
        return;
    }
    repo->setSyncStatus("failed");
    repo->setUpdateTime(time(0));
    data::OrganizationReposInfoDao::Update(repo, db);
}

void OrganizationRepoManager::SyncFromGitHub(int64_t repo_id, const std::string& url, const std::string& token)
{
    std::string repo_path = extractGithubRepo(url);
    if (repo_path.empty()) {
        ERROR(logger) << "SyncFromGitHub: invalid repo url=" << url;
        markSyncFailed(repo_id);
        return;
    }

    std::string api_url = "https://api.github.com/repos/" + repo_path;
    std::map<std::string, std::string> headers;
    headers["Accept"] = "application/vnd.github.v3+json";
    headers["Accept-Encoding"] = "identity";
    headers["User-Agent"] = "Blog-Server/1.0";
    if (!token.empty()) {
        headers["Authorization"] = "Bearer " + token;
    }

    INFO(logger) << "SyncFromGitHub: calling " << api_url;
    auto result = chen::http::HttpConnection::DoGet(api_url, 15000, headers);
    if (!result || result->result != 0 || !result->response || result->response->getStatus() != chen::http::HttpStatus::OK) {
        ERROR(logger) << "SyncFromGitHub: HTTP request failed"
                      << " result=" << (result ? result->result : -1)
                      << " status=" << (result && result->response ? (int)result->response->getStatus() : 0)
                      << " body=" << (result && result->response ? result->response->getBody() : "");
        markSyncFailed(repo_id);
        return;
    }

    Json::Value json;
    Json::Reader reader;
    if (!reader.parse(result->response->getBody(), json)) {
        ERROR(logger) << "SyncFromGitHub: parse JSON failed";
        markSyncFailed(repo_id);
        return;
    }

    auto repo = OrganizationRepoMgr::GetInstance()->get(repo_id);
    if (!repo) {
        ERROR(logger) << "SyncFromGitHub: repo " << repo_id << " not found";
        return;
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "SyncFromGitHub: GetDB failed";
        return;
    }

    if (json.isMember("stargazers_count")) {
        repo->setStarsCount(json["stargazers_count"].asInt());
    }
    if (json.isMember("language") && !json["language"].isNull()) {
        repo->setLanguage(json["language"].asString());
    }
    if (json.isMember("description") && !json["description"].isNull()) {
        repo->setDescription(json["description"].asString());
    }
    // 从 API 响应设置 github_full_name（如果尚未设置或与 URL 提取一致）
    if (json.isMember("full_name") && !json["full_name"].isNull()) {
        repo->setGithubFullName(json["full_name"].asString());
    }
    repo->setSyncStatus("success");
    repo->setUpdateTime(time(0));

    if (data::OrganizationReposInfoDao::Update(repo, db)) {
        ERROR(logger) << "SyncFromGitHub: DB update failed";
        repo->setSyncStatus("failed");
        data::OrganizationReposInfoDao::Update(repo, db);
        return;
    }

    INFO(logger) << "SyncFromGitHub: success for repo " << repo_id;
}

} // namespace blog
