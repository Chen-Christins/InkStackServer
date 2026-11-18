#include "platform/manager/OrganizationRepoPrManager.hpp"

#include "../../common/util/Util.hpp"
#include "common/manager/CacheUtil.hpp"
#include "platform/manager/OrganizationRepoManager.hpp"
#include "platform/manager/OrganizationUserRelManager.hpp"
#include "protocol_ss_github.h"

#include <chen/http/http_connection.h>
#include <chen/http/uri.h>
#include <chen/iomanager/worker.h>
#include <chen/log/log.h>
#include <chen/util/json_util.h>
#include <json/json.h>
#include <json/reader.h>

#include <mutex>
#include <set>

namespace blog {

static chen::Logger::ptr logger = LOG_ROOT();

static const int kCacheTTL = 300;
static const char* kCachePrefix = "cache:obj:org_pr:";

static std::string orgRepoPrCacheKey(int64_t id)
{
    return std::string(kCachePrefix) + std::to_string(id);
}

static data::OrganizationRepoPrsInfo::ptr parseOrgRepoPrCache(const Json::Value& v)
{
    auto info = std::make_shared<data::OrganizationRepoPrsInfo>();
    info->setId(cacheJsonInt64(v, "id"));
    info->setRepoId(cacheJsonInt64(v, "repo_id"));
    info->setGithubPrId(cacheJsonInt32(v, "github_pr_id"));
    info->setTitle(cacheJsonString(v, "title"));
    info->setDescription(cacheJsonString(v, "description"));
    info->setState(cacheJsonString(v, "state"));
    info->setPriority(cacheJsonString(v, "priority"));
    info->setAuthor(cacheJsonString(v, "author"));
    info->setHeadBranch(cacheJsonString(v, "head_branch"));
    info->setBaseBranch(cacheJsonString(v, "base_branch"));
    info->setCommitSha(cacheJsonString(v, "commit_sha"));
    info->setChangedFiles(cacheJsonInt32(v, "changed_files"));
    info->setAdditions(cacheJsonInt32(v, "additions"));
    info->setDeletions(cacheJsonInt32(v, "deletions"));
    info->setReviewers(cacheJsonString(v, "reviewers"));
    info->setReviewStatus(cacheJsonString(v, "review_status"));
    info->setClosedAt(cacheJsonTime(v, "closed_at"));
    info->setMergedAt(cacheJsonTime(v, "merged_at"));
    info->setCreateTime(cacheJsonTime(v, "create_time"));
    info->setUpdateTime(cacheJsonTime(v, "update_time"));
    info->markClean();
    return info;
}

static data::OrganizationRepoPrsInfo::ptr getOrgRepoPrCache(int64_t id)
{
    std::string key = orgRepoPrCacheKey(id);
    std::string data = redisCacheGet(key);
    if (data.empty()) {
        return nullptr;
    }
    Json::Value v;
    if (!chen::JsonUtil::FromString(v, data) || !v.isObject()) {
        redisCacheDel(key);
        return nullptr;
    }
    return parseOrgRepoPrCache(v);
}

static void setOrgRepoPrCache(const data::OrganizationRepoPrsInfo::ptr& info)
{
    if (!info) {
        return;
    }
    redisCacheSetex(orgRepoPrCacheKey(info->getId()), info->toJsonString(), kCacheTTL);
}

static void delOrgRepoPrCache(int64_t id)
{
    redisCacheDel(orgRepoPrCacheKey(id));
}

/// 单条 PR 同步互斥锁与进行中集合，避免同一 PR 并发重复请求 GitHub
static std::mutex s_sync_pr_mutex;
static std::set<int64_t> s_syncing_pr_ids;

data::OrganizationRepoPrsInfo::ptr OrganizationRepoPrManager::parseRow(chen::ISQLData::ptr rt)
{
    return data::OrganizationRepoPrsInfoDao::ParseRow(rt);
}

void OrganizationRepoPrManager::add(data::OrganizationRepoPrsInfo::ptr info)
{
    setOrgRepoPrCache(info);
}

void OrganizationRepoPrManager::del(int64_t id)
{
    delOrgRepoPrCache(id);
}

void OrganizationRepoPrManager::onTick()
{
    std::vector<data::OrganizationReposInfo::ptr> repos;
    OrganizationRepoMgr::GetInstance()->getAllWithToken(repos);

    constexpr int kMaxPrCount = 15;
    for (const auto& repo : repos) {
        if (!repo) {
            continue;
        }
        int64_t id = repo->getId();
        std::string url = repo->getUrl();
        std::string token = repo->getToken();

        SyncFromGitHub(id, url, token, kMaxPrCount);
    }
}

data::OrganizationRepoPrsInfo::ptr OrganizationRepoPrManager::get(int64_t id)
{
    auto v = getOrgRepoPrCache(id);
    if (v) {
        return v;
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    v = data::OrganizationRepoPrsInfoDao::Query(id, db);
    if (v) {
        setOrgRepoPrCache(v);
    }
    return v;
}

data::OrganizationRepoPrsInfo::ptr OrganizationRepoPrManager::getByRepoAndPrId(int64_t repo_id, int32_t github_pr_id)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    return data::OrganizationRepoPrsInfoDao::QueryByRepoIdGithubPrId(repo_id, github_pr_id, db);
}

int64_t OrganizationRepoPrManager::listByRepoPages(
    std::vector<data::OrganizationRepoPrsInfo::ptr>& prs,
    int64_t repo_id,
    const std::string& state,
    uint64_t offset,
    uint64_t limit)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }

    auto qb = data::OrganizationRepoPrsInfoDao::newQuery();
    qb->where("repo_id", "=", repo_id);
    qb->whereIf(!state.empty(), "state", "=", state);
    qb->orderBy("create_time", "DESC");

    int64_t total = 0;
    if (data::OrganizationRepoPrsInfoDao::QueryByBuilderPages(prs, total, qb, (int32_t)offset, (int32_t)limit, db)) {
        ERROR(logger) << "listByRepoPages QueryByBuilderPages failed";
        return 0;
    }
    return total;
}

int64_t OrganizationRepoPrManager::getCountByRepo(int64_t repo_id)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }
    auto qb = data::OrganizationRepoPrsInfoDao::newQuery();
    qb->where("repo_id", "=", repo_id);
    int64_t total = 0;
    if (qb->executeCount(total, db)) {
        return 0;
    }
    return total;
}

int64_t OrganizationRepoPrManager::listByOrgPages(
    std::vector<data::OrganizationRepoPrsInfo::ptr>& prs,
    int64_t org_id,
    const std::string& state,
    uint64_t offset,
    uint64_t limit)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }

    auto qb = data::OrganizationRepoPrsInfoDao::newQuery("prs");
    qb->select("prs.*");
    qb->join("organization_repos repos", "prs.repo_id = repos.id");
    qb->where("repos.org_id", "=", org_id);
    qb->whereIf(!state.empty(), "prs.state", "=", state);
    qb->orderBy("prs.create_time", "DESC");

    int64_t total = 0;
    if (data::OrganizationRepoPrsInfoDao::QueryByBuilderPages(prs, total, qb, (int32_t)offset, (int32_t)limit, db)) {
        ERROR(logger) << "listByOrgPages QueryByBuilderPages failed";
        return 0;
    }
    return total;
}

int64_t OrganizationRepoPrManager::listByUserPages(
    std::vector<data::OrganizationRepoPrsInfo::ptr>& prs,
    int64_t uid,
    const std::string& state,
    uint64_t offset,
    uint64_t limit)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }

    auto qb = data::OrganizationRepoPrsInfoDao::newQuery("prs");
    qb->select("prs.*");
    qb->join("organization_repos repos", "prs.repo_id = repos.id");
    qb->join("INNER", "organization_user_rel rel", "repos.org_id = rel.org_id");
    qb->where("rel.user_id", "=", uid);
    qb->where("rel.status", "=", (int64_t)OrganizationUserRelManager::Status::APPROVED);
    qb->where("rel.is_deleted", "=", (int64_t)0);
    qb->whereIf(!state.empty(), "prs.state", "=", state);
    qb->orderBy("prs.create_time", "DESC");

    int64_t total = 0;
    if (data::OrganizationRepoPrsInfoDao::QueryByBuilderPages(prs, total, qb, (int32_t)offset, (int32_t)limit, db)) {
        ERROR(logger) << "listByUserPages QueryByBuilderPages failed";
        return 0;
    }
    return total;
}

/// 从 GitHub URL 提取 owner/repo
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

/// 从 Link header 中提取 rel="next" 的 URL
static std::string getNextPageUrl(const std::string& linkHeader)
{
    // 格式: <https://...>; rel="next", <https://...>; rel="last"
    size_t pos = 0;
    while (pos < linkHeader.size()) {
        auto start = linkHeader.find('<', pos);
        if (start == std::string::npos) {
            break;
        }
        auto end = linkHeader.find('>', start);
        if (end == std::string::npos) {
            break;
        }
        std::string url = linkHeader.substr(start + 1, end - start - 1);
        auto relPos = linkHeader.find("rel=\"", end);
        if (relPos != std::string::npos) {
            auto quoteEnd = linkHeader.find('"', relPos + 5);
            std::string rel = linkHeader.substr(relPos + 5, quoteEnd - relPos - 5);
            if (rel == "next") {
                return url;
            }
        }
        pos = end + 1;
    }
    return "";
}

struct SyncPrItem {
    int number;
    std::string title;
    std::string description;
    std::string state;
    std::string author;
    std::string head_branch;
    std::string base_branch;
    std::string commit_sha;
    bool merged;
    int64_t created_at;
    int64_t closed_at;
    int64_t merged_at;
    std::string review_status;
    std::string reviewers;
};

/// 解析 GitHub reviews 数组，计算整体审查状态与 reviewers JSON
static void parseReviews(const Json::Value& arr, std::string& review_status, std::string& reviewers_json)
{
    bool has_approved = false;
    bool has_changes = false;
    Json::Value reviewerArr(Json::arrayValue);
    for (auto& rv : arr) {
        std::string state = rv["state"].asString();
        if (state == "APPROVED") {
            has_approved = true;
        } else if (state == "CHANGES_REQUESTED") {
            has_changes = true;
        }
        Json::Value reviewer;
        reviewer["reviewer"] = rv["user"]["login"].asString();
        reviewer["status"] = state;
        reviewerArr.append(reviewer);
    }
    if (has_approved) {
        review_status = "approved";
    } else if (has_changes) {
        review_status = "changes_requested";
    } else {
        review_status = "pending";
    }
    reviewers_json = chen::JsonUtil::ToString(reviewerArr);
}

/// 发起 GitHub GET 请求，仅对连接失败/超时重试，返回原始响应（含非 2xx 状态码）
static chen::http::HttpResult::ptr githubGetRaw(const std::string& url, const std::map<std::string, std::string>& headers, int max_retries)
{
    chen::http::HttpResult::ptr result;
    for (int retry = 0; retry <= max_retries; ++retry) {
        result = chen::http::HttpConnection::DoGet(url, 30000, headers);
        bool transient =
            (!result || result->result == (int)chen::http::HttpResult::Error::CONNECT_FAIL
             || result->result == (int)chen::http::HttpResult::Error::TIMEOUT);
        if (!transient) {
            return result;
        }
        if (retry < max_retries) {
            WARN(logger) << "githubGetRaw: retry " << (retry + 1) << "/" << max_retries << " for " << url;
        }
    }
    return result;
}

/// 判断 GitHub 响应是否因限流被拒绝
static bool isGithubRateLimited(const chen::http::HttpResponse::ptr& response)
{
    if (!response) {
        return false;
    }
    auto status = response->getStatus();
    if (status == chen::http::HttpStatus::TOO_MANY_REQUESTS) {
        return true;
    }
    if (status == chen::http::HttpStatus::FORBIDDEN) {
        if (response->getHeader("x-ratelimit-remaining") == "0") {
            return true;
        }
        if (!response->getHeader("retry-after").empty()) {
            return true;
        }
    }
    return false;
}

static int syncPage(
    const Json::Value& arr,
    int64_t repo_id,
    const std::string& repo_path,
    const std::map<std::string, std::string>& headers,
    chen::IDB::ptr db)
{
    // 先解析列表数据
    std::vector<SyncPrItem> items;
    for (auto& item : arr) {
        SyncPrItem si;
        si.number = item["number"].asInt();
        si.title = item["title"].asString();
        si.description = item["body"].asString();
        si.state = item["state"].asString();
        si.author = item["user"]["login"].asString();
        si.head_branch = item["head"]["ref"].asString();
        si.base_branch = item["base"]["ref"].asString();
        si.commit_sha = item["head"]["sha"].asString();
        si.merged = item["merged"].asBool();
        si.created_at = 0;
        si.closed_at = 0;
        si.review_status = "pending";
        si.reviewers = "[]";
        si.merged_at = 0;
        if (item.isMember("created_at") && !item["created_at"].isNull()) {
            si.created_at = chen::TimeUtil::Str2Time(item["created_at"].asCString());
        }
        if (item.isMember("closed_at") && !item["closed_at"].isNull()) {
            si.closed_at = chen::TimeUtil::Str2Time(item["closed_at"].asCString());
        }
        if (item.isMember("merged_at") && !item["merged_at"].isNull()) {
            si.merged_at = chen::TimeUtil::Str2Time(item["merged_at"].asCString());
        }
        items.push_back(si);
    }

    // 并行拉详情 + 审查数据
    std::map<int, int> additions, deletions, changed_files;
    std::map<int, std::string> review_statuses, reviewers_json;
    {
        auto wg = chen::WorkerGroup::Create(5);
        for (auto& si : items) {
            int num = si.number;
            wg->schedule([repo_path, &headers, num, &additions, &deletions, &changed_files, &review_statuses, &reviewers_json]() {
                // 1. 拉详情补全 additions/deletions/changed_files
                {
                    std::string detail_url = "https://api.github.com/repos/" + repo_path + "/pulls/" + std::to_string(num);
                    chen::http::HttpResult::ptr detail;
                    for (int retry = 0; retry <= 3; ++retry) {
                        detail = chen::http::HttpConnection::DoGet(detail_url, 15000, headers);
                        if (detail && detail->result == 0 && detail->response && detail->response->getStatus() == chen::http::HttpStatus::OK) {
                            break;
                        }
                    }
                    if (detail && detail->result == 0 && detail->response && detail->response->getStatus() == chen::http::HttpStatus::OK) {
                        Json::Value detailJson;
                        Json::Reader reader;
                        if (reader.parse(detail->response->getBody(), detailJson)) {
                            if (detailJson.isMember("additions")) {
                                additions[num] = detailJson["additions"].asInt();
                            }
                            if (detailJson.isMember("deletions")) {
                                deletions[num] = detailJson["deletions"].asInt();
                            }
                            if (detailJson.isMember("changed_files")) {
                                changed_files[num] = detailJson["changed_files"].asInt();
                            }
                        }
                    }
                }
                // 2. 拉审查数据
                {
                    std::string reviews_url = "https://api.github.com/repos/" + repo_path + "/pulls/" + std::to_string(num) + "/reviews";
                    chen::http::HttpResult::ptr reviews;
                    for (int retry = 0; retry <= 3; ++retry) {
                        reviews = chen::http::HttpConnection::DoGet(reviews_url, 15000, headers);
                        if (reviews && reviews->result == 0 && reviews->response && reviews->response->getStatus() == chen::http::HttpStatus::OK) {
                            break;
                        }
                    }
                    if (reviews && reviews->result == 0 && reviews->response && reviews->response->getStatus() == chen::http::HttpStatus::OK) {
                        Json::Value arr;
                        Json::Reader reader;
                        if (reader.parse(reviews->response->getBody(), arr) && arr.isArray()) {
                            std::string status;
                            std::string reviewers;
                            parseReviews(arr, status, reviewers);
                            review_statuses[num] = status;
                            reviewers_json[num] = reviewers;
                        }
                    }
                }
            });
        }
        wg->waitAll();
    }

    // 写入 DB
    int synced = 0;
    for (auto& si : items) {
        auto existing = OrganizationRepoPrMgr::GetInstance()->getByRepoAndPrId(repo_id, si.number);
        data::OrganizationRepoPrsInfo::ptr info;
        if (existing) {
            info = existing;
        } else {
            info.reset(new data::OrganizationRepoPrsInfo);
            info->setRepoId(repo_id);
            info->setGithubPrId(si.number);
        }
        if (si.created_at) {
            info->setCreateTime(si.created_at);
        }

        info->setTitle(si.title);
        info->setDescription(si.description);
        if (si.merged || si.merged_at > 0) {
            info->setState("merged");
        } else {
            info->setState(si.state);
        }
        info->setAuthor(si.author);
        info->setHeadBranch(si.head_branch);
        info->setBaseBranch(si.base_branch);
        info->setCommitSha(si.commit_sha);

        auto it_a = additions.find(si.number);
        if (it_a != additions.end()) {
            info->setAdditions(it_a->second);
        }
        auto it_d = deletions.find(si.number);
        if (it_d != deletions.end()) {
            info->setDeletions(it_d->second);
        }
        auto it_c = changed_files.find(si.number);
        if (it_c != changed_files.end()) {
            info->setChangedFiles(it_c->second);
        }

        {
            auto it = review_statuses.find(si.number);
            info->setReviewStatus(it != review_statuses.end() ? it->second : "pending");
        }
        {
            auto it = reviewers_json.find(si.number);
            info->setReviewers(it != reviewers_json.end() ? it->second : "[]");
        }
        info->setClosedAt(si.closed_at);
        info->setMergedAt(si.merged_at);
        info->setUpdateTime(time(0));

        if (existing) {
            if (data::OrganizationRepoPrsInfoDao::Update(info, db)) {
                ERROR(logger) << "SyncPrFromGitHub: Update failed for PR #" << info->getGithubPrId() << ", errstr=" << db->getErrStr()
                              << ", errno=" << db->getErrno();
                continue;
            }
        } else {
            if (data::OrganizationRepoPrsInfoDao::Insert(info, db)) {
                ERROR(logger) << "SyncPrFromGitHub: Insert failed for PR #" << info->getGithubPrId() << ", errstr=" << db->getErrStr()
                              << ", errno=" << db->getErrno();
                continue;
            }
            OrganizationRepoPrMgr::GetInstance()->add(info);
        }
        ++synced;
    }
    return synced;
}

void OrganizationRepoPrManager::SyncFromGitHub(int64_t repo_id, const std::string& repo_url, const std::string& token, int max_prs)
{
    std::string repo_path = extractGithubRepo(repo_url);
    if (repo_path.empty()) {
        ERROR(logger) << "SyncPrFromGitHub: invalid repo url=" << repo_url;
        return;
    }

    std::map<std::string, std::string> headers;
    headers["Accept"] = "application/vnd.github.v3+json";
    headers["Accept-Encoding"] = "identity";
    headers["User-Agent"] = "Blog-Server/1.0";
    if (!token.empty()) {
        headers["Authorization"] = "Bearer " + token;
    }

    {
        auto repo = OrganizationRepoMgr::GetInstance()->get(repo_id);
        if (repo) {
            auto db2 = GetDB();
            if (db2) {
                repo->setPrSyncStatus("syncing");
                repo->setGithubFullName(repo_path);
                data::OrganizationReposInfoDao::Update(repo, db2);
            }
        }
    }

    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "SyncPrFromGitHub: GetDB failed";
        return;
    }

    const int kMaxPages = 10;
    int page_no = 0;
    int total_synced = 0;
    int per_page = max_prs > 0 ? max_prs : 50;
    std::string api_url = "https://api.github.com/repos/" + repo_path + "/pulls?state=all&per_page=" + std::to_string(per_page);

    auto doGetWithRetry = [&](const std::string& url, int max_retries) -> chen::http::HttpResult::ptr {
        for (int retry = 0; retry <= max_retries; ++retry) {
            auto r = chen::http::HttpConnection::DoGet(url, 30000, headers);
            if (r && r->result == 0 && r->response && r->response->getStatus() == chen::http::HttpStatus::OK) {
                return r;
            }
            if (retry < max_retries) {
                WARN(logger) << "SyncPrFromGitHub: retry " << (retry + 1) << "/" << max_retries << " for " << url;
            }
        }
        return nullptr;
    };

    while (!api_url.empty() && page_no < kMaxPages) {
        ++page_no;
        INFO(logger) << "SyncPrFromGitHub: page " << page_no << " " << api_url;

        auto result = doGetWithRetry(api_url, 3);
        if (!result) {
            ERROR(logger) << "SyncPrFromGitHub: HTTP request failed on page " << page_no;
            break;
        }

        Json::Value arr;
        Json::Reader reader;
        if (!reader.parse(result->response->getBody(), arr) || !arr.isArray()) {
            ERROR(logger) << "SyncPrFromGitHub: parse JSON failed on page " << page_no;
            break;
        }

        total_synced += syncPage(arr, repo_id, repo_path, headers, db);

        // 限制条数时只取第一页，不再翻页
        if (max_prs > 0) {
            break;
        }

        // 检查下一页
        std::string link = result->response->getHeader("link");
        if (link.empty()) {
            break;
        }
        api_url = getNextPageUrl(link);
    }

    // 更新仓库的 PR 同步状态
    {
        auto repo = OrganizationRepoMgr::GetInstance()->get(repo_id);
        if (repo) {
            auto repoDb = GetDB();
            if (repoDb) {
                repo->setPrSyncStatus(total_synced >= 0 ? "success" : "failed");
                repo->setPrSyncedAt(time(0));
                data::OrganizationReposInfoDao::Update(repo, repoDb);
            }
        }
    }

    INFO(logger) << "SyncPrFromGitHub: synced " << total_synced << " PRs across " << page_no << " pages for repo " << repo_id;
}

OrganizationRepoPrManager::SyncPrStatus OrganizationRepoPrManager::SyncPrFromGitHub(int64_t pr_id, data::OrganizationRepoPrsInfo::ptr& out)
{
    auto pr = OrganizationRepoPrMgr::GetInstance()->get(pr_id);
    if (!pr) {
        return SyncPrStatus::PR_NOT_FOUND;
    }

    auto repo = OrganizationRepoMgr::GetInstance()->get(pr->getRepoId());
    if (!repo) {
        return SyncPrStatus::REPO_NOT_FOUND;
    }

    std::string token = repo->getToken();
    if (token.empty()) {
        return SyncPrStatus::NO_TOKEN;
    }

    std::string repo_path = extractGithubRepo(repo->getUrl());
    if (repo_path.empty()) {
        ERROR(logger) << "SyncPrFromGitHub: invalid repo url=" << repo->getUrl();
        return SyncPrStatus::HTTP_ERROR;
    }

    int32_t number = pr->getGithubPrId();
    if (number <= 0) {
        ERROR(logger) << "SyncPrFromGitHub: invalid github_pr_id for pr_id=" << pr_id;
        return SyncPrStatus::HTTP_ERROR;
    }

    // 同一 PR 加锁，重复触发直接返回 BUSY
    {
        std::lock_guard<std::mutex> lock(s_sync_pr_mutex);
        if (s_syncing_pr_ids.count(pr_id)) {
            return SyncPrStatus::BUSY;
        }
        s_syncing_pr_ids.insert(pr_id);
    }

    // RAII 保证任一出口都移除进行中标记
    struct SyncGuard {
        int64_t id;
        explicit SyncGuard(int64_t v)
            : id(v)
        {
        }
        ~SyncGuard()
        {
            std::lock_guard<std::mutex> lock(s_sync_pr_mutex);
            s_syncing_pr_ids.erase(id);
        }
    } guard(pr_id);

    std::map<std::string, std::string> headers;
    headers["Accept"] = "application/vnd.github.v3+json";
    headers["Accept-Encoding"] = "identity";
    headers["User-Agent"] = "Blog-Server/1.0";
    headers["Authorization"] = "Bearer " + token;

    // 1. 拉取 PR 详情
    std::string detail_url = "https://api.github.com/repos/" + repo_path + "/pulls/" + std::to_string(number);
    auto detail = githubGetRaw(detail_url, headers, 3);
    if (!detail || detail->result != 0 || !detail->response) {
        ERROR(logger) << "SyncPrFromGitHub: detail request failed pr_id=" << pr_id << " url=" << detail_url;
        return SyncPrStatus::HTTP_ERROR;
    }
    auto detail_status = detail->response->getStatus();
    if (detail_status == chen::http::HttpStatus::NOT_FOUND) {
        return SyncPrStatus::GITHUB_NOT_FOUND;
    }
    if (isGithubRateLimited(detail->response)) {
        return SyncPrStatus::RATE_LIMITED;
    }
    if (detail_status != chen::http::HttpStatus::OK) {
        ERROR(logger) << "SyncPrFromGitHub: detail unexpected status=" << (int)detail_status << " pr_id=" << pr_id;
        return SyncPrStatus::HTTP_ERROR;
    }

    Json::Value detail_json;
    Json::Reader detail_reader;
    if (!detail_reader.parse(detail->response->getBody(), detail_json) || !detail_json.isObject()) {
        ERROR(logger) << "SyncPrFromGitHub: parse detail failed pr_id=" << pr_id;
        return SyncPrStatus::HTTP_ERROR;
    }

    // 2. 回写主字段
    int64_t created_at = 0;
    if (detail_json.isMember("created_at") && !detail_json["created_at"].isNull()) {
        created_at = chen::TimeUtil::Str2Time(detail_json["created_at"].asCString());
    }
    if (created_at) {
        pr->setCreateTime(created_at);
    }
    pr->setGithubPrId(detail_json["number"].asInt());
    pr->setTitle(detail_json["title"].asString());
    pr->setDescription(detail_json["body"].asString());
    pr->setAuthor(detail_json["user"]["login"].asString());
    pr->setHeadBranch(detail_json["head"]["ref"].asString());
    pr->setBaseBranch(detail_json["base"]["ref"].asString());
    pr->setCommitSha(detail_json["head"]["sha"].asString());
    pr->setChangedFiles(detail_json["changed_files"].asInt());
    pr->setAdditions(detail_json["additions"].asInt());
    pr->setDeletions(detail_json["deletions"].asInt());

    bool merged = detail_json["merged"].asBool();
    int64_t merged_at = 0;
    int64_t closed_at = 0;
    if (detail_json.isMember("merged_at") && !detail_json["merged_at"].isNull()) {
        merged_at = chen::TimeUtil::Str2Time(detail_json["merged_at"].asCString());
    }
    if (detail_json.isMember("closed_at") && !detail_json["closed_at"].isNull()) {
        closed_at = chen::TimeUtil::Str2Time(detail_json["closed_at"].asCString());
    }
    if (merged || merged_at > 0) {
        pr->setState("merged");
    } else {
        pr->setState(detail_json["state"].asString());
    }
    pr->setClosedAt(closed_at);
    pr->setMergedAt(merged_at);

    // 3. 拉取 reviews；失败时保留原有评审字段，整体仍视为成功
    std::string reviews_url = detail_url + "/reviews";
    auto reviews = githubGetRaw(reviews_url, headers, 3);
    if (reviews && reviews->result == 0 && reviews->response) {
        auto reviews_status = reviews->response->getStatus();
        if (reviews_status == chen::http::HttpStatus::OK) {
            Json::Value arr;
            Json::Reader reviews_reader;
            if (reviews_reader.parse(reviews->response->getBody(), arr) && arr.isArray()) {
                std::string review_status;
                std::string reviewers_json;
                parseReviews(arr, review_status, reviewers_json);
                pr->setReviewStatus(review_status);
                pr->setReviewers(reviewers_json);
            }
        } else if (isGithubRateLimited(reviews->response)) {
            WARN(logger) << "SyncPrFromGitHub: reviews rate limited pr_id=" << pr_id;
        }
    }

    pr->setUpdateTime(time(0));

    // 4. 写回 DB
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "SyncPrFromGitHub: GetDB failed pr_id=" << pr_id;
        return SyncPrStatus::DB_ERROR;
    }
    if (data::OrganizationRepoPrsInfoDao::Update(pr, db)) {
        ERROR(logger) << "SyncPrFromGitHub: Update failed for PR #" << pr->getGithubPrId() << ", errstr=" << db->getErrStr()
                      << ", errno=" << db->getErrno();
        return SyncPrStatus::DB_ERROR;
    }
    OrganizationRepoPrMgr::GetInstance()->add(pr);
    out = pr;

    INFO(logger) << "SyncPrFromGitHub: synced pr_id=" << pr_id << " PR #" << pr->getGithubPrId() << " state=" << pr->getState();
    return SyncPrStatus::OK;
}

data::OrganizationReposInfo::ptr OrganizationRepoPrManager::findRepoByOwnerAndName(const std::string& owner, const std::string& name)
{
    if (owner.empty() || name.empty()) {
        return nullptr;
    }
    std::string full_name = owner + "/" + name;

    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "findRepoByOwnerAndName: GetDB failed for " << full_name;
        return nullptr;
    }

    // 用 github_full_name 字段精确匹配
    auto qb = data::OrganizationReposInfoDao::newQuery();
    qb->select("id");
    qb->where("github_full_name", "=", full_name);
    qb->limit(1);
    int64_t repoId = 0;
    if (qb->queryScalarInt64(repoId, db)) {
        DEBUG(logger) << "findRepoByOwnerAndName: no repo found for " << full_name;
        return nullptr;
    }
    return OrganizationRepoMgr::GetInstance()->get(repoId);
}

} // namespace blog
