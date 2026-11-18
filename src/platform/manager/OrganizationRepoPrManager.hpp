/**
 * @file OrganizationRepoPrManager.hpp
 * @brief 组织仓库 PR 管理器
 * @author Christins
 * @date 2026-06-24
 * @copyright Apache 2.0
 */
#pragma once

#include "blog/data/organization_repo_prs_info.h"
#include "blog/data/organization_repos_info.h"

#include <chen/db/query_builder.h>
#include <chen/util/singleton.h>

// 前置声明 RPC 协议结构体，避免在 header 中引入 protocol 头文件
struct tagGithubPRInfo;
struct tagGithubPRReviewInfo;

namespace blog {

class OrganizationRepoPrManager {
public:
    typedef std::shared_ptr<OrganizationRepoPrManager> ptr;

    /// 单条 PR 同步结果
    enum class SyncPrStatus {
        /// 同步成功
        OK,
        /// 本地 PR 记录不存在
        PR_NOT_FOUND,
        /// 关联仓库不存在
        REPO_NOT_FOUND,
        /// 仓库未配置 GitHub Token
        NO_TOKEN,
        /// GitHub 上未找到该 PR (404)
        GITHUB_NOT_FOUND,
        /// GitHub API 触发限流
        RATE_LIMITED,
        /// 同一 PR 正在同步中
        BUSY,
        /// GitHub API 请求失败
        HTTP_ERROR,
        /// 数据库操作失败
        DB_ERROR,
    };

    void add(data::OrganizationRepoPrsInfo::ptr info);

    void del(int64_t id);

    void onTick();

    data::OrganizationRepoPrsInfo::ptr get(int64_t id);

    data::OrganizationRepoPrsInfo::ptr getByRepoAndPrId(int64_t repo_id, int32_t github_pr_id);

    int64_t listByRepoPages(
        std::vector<data::OrganizationRepoPrsInfo::ptr>& prs,
        int64_t repo_id,
        const std::string& state,
        uint64_t offset,
        uint64_t limit);

    int64_t getCountByRepo(int64_t repo_id);

    int64_t listByOrgPages(
        std::vector<data::OrganizationRepoPrsInfo::ptr>& prs,
        int64_t org_id,
        const std::string& state,
        uint64_t offset,
        uint64_t limit);

    int64_t listByUserPages(
        std::vector<data::OrganizationRepoPrsInfo::ptr>& prs,
        int64_t uid,
        const std::string& state,
        uint64_t offset,
        uint64_t limit);

    /// 从 GitHub API 同步某个仓库的 PR
    /// @param max_prs 最多同步的 PR 条数，0 表示全量同步
    static void SyncFromGitHub(int64_t repo_id, const std::string& repo_url, const std::string& token, int max_prs = 0);

    /// 从 GitHub API 同步单条 PR（按本地 PR 记录 id）
    /// @param pr_id 本地 organization_repo_prs 记录 id
    /// @param[out] out 同步成功时返回更新后的 PR 对象
    /// @return 同步结果状态
    static SyncPrStatus SyncPrFromGitHub(int64_t pr_id, data::OrganizationRepoPrsInfo::ptr& out);

    static data::OrganizationReposInfo::ptr findRepoByOwnerAndName(const std::string& owner, const std::string& name);

private:
    static data::OrganizationRepoPrsInfo::ptr parseRow(chen::ISQLData::ptr rt);
};

typedef chen::Singleton<OrganizationRepoPrManager> OrganizationRepoPrMgr;

} // namespace blog
