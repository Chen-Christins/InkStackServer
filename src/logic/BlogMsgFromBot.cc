#include "BlogMsgFromBot.h"

#include "../manager/organization_repo_pr_manager.h"
#include "../util.h"

#include <chen/log/log.h>

namespace blog {

static chen::Logger::ptr logger = LOG_ROOT();

static int32_t HandlePRWebhook(const tagGithubPRInfo& info)
{
    // 1. 查找本地仓库
    auto repo = OrganizationRepoPrManager::findRepoByOwnerAndName(info.RepoOwner, info.RepoName);
    if (!repo) {
        WARN(logger) << "HandlePRWebhook: repo not found for " << info.RepoOwner << "/" << info.RepoName << ", PR #" << info.Number << " \""
                     << info.Title << "\"";
        return GITHUB_REPO_NOT_FOUND;
    }
    int64_t repo_id = repo->getId();

    // 2. 解析时间
    time_t created_at = chen::TimeUtil::Str2Time(info.CreatedAt);
    time_t closed_at = chen::TimeUtil::Str2Time(info.ClosedAt);
    time_t merged_at = chen::TimeUtil::Str2Time(info.MergedAt);

    // 3. 获取 DB 连接
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "HandlePRWebhook: GetDB failed for repo " << repo_id << " PR #" << info.Number;
        return GITHUB_DB_CONNECTION_FAILED;
    }

    // 4. 查找已有记录（幂等去重）
    auto existing = OrganizationRepoPrMgr::GetInstance()->getByRepoAndPrId(repo_id, info.Number);

    data::OrganizationRepoPrsInfo::ptr pr;
    if (existing) {
        pr = existing;
    } else {
        pr.reset(new data::OrganizationRepoPrsInfo);
        pr->setRepoId(repo_id);
        pr->setGithubPrId(info.Number);
        if (created_at) {
            pr->setCreateTime(created_at);
        }
    }

    // 5. 更新字段
    pr->setTitle(info.Title);
    pr->setDescription(info.Body);
    pr->setAuthor(info.Author);
    pr->setHeadBranch(info.HeadBranch);
    pr->setBaseBranch(info.BaseBranch);
    pr->setCommitSha(info.HeadSha);
    pr->setChangedFiles(info.ChangedFiles);
    pr->setAdditions(info.Additions);
    pr->setDeletions(info.Deletions);

    // 状态：merged > open/closed
    if (info.Merged) {
        pr->setState("merged");
    } else {
        pr->setState(info.State);
    }

    // opened/reopened 时重置审查状态
    if (info.Action == "opened" || info.Action == "reopened") {
        pr->setReviewStatus("pending");
        pr->setReviewers("[]");
    }

    // synchronized(push) 时更新时间，但保留 review 状态
    if (info.Action == "synchronize") {
        pr->setCommitSha(info.HeadSha);
    }

    pr->setClosedAt(closed_at);
    pr->setMergedAt(merged_at);
    pr->setUpdateTime(time(0));

    // 6. 写入 DB
    if (data::OrganizationRepoPrsInfoDao::InsertOrUpdate(pr, db)) {
        ERROR(logger) << "HandlePRWebhook: Update failed for repo " << repo_id << " PR #" << info.Number << ", errstr=" << db->getErrStr()
                      << ", errno=" << db->getErrno();
        return GITHUB_DB_OPERATION_FAILED;
    }
    OrganizationRepoPrMgr::GetInstance()->add(pr);
    DEBUG(logger) << "HandlePRWebhook: updated PR #" << info.Number << " state=" << pr->getState() << " action=" << info.Action;

    return 0;
}

static int32_t HandlePRReviewWebhook(const tagGithubPRReviewInfo& info)
{
    // 1. 查找本地仓库
    auto repo = OrganizationRepoPrManager::findRepoByOwnerAndName(info.RepoOwner, info.RepoName);
    if (!repo) {
        WARN(logger) << "HandlePRReviewWebhook: repo not found for " << info.RepoOwner << "/" << info.RepoName << ", PR #" << info.PRNumber;
        return GITHUB_REPO_NOT_FOUND;
    }
    int64_t repo_id = repo->getId();

    // 2. 查找 PR
    auto pr = OrganizationRepoPrMgr::GetInstance()->getByRepoAndPrId(repo_id, info.PRNumber);
    if (!pr) {
        WARN(logger) << "HandlePRReviewWebhook: PR #" << info.PRNumber << " not found in repo " << repo_id << " (" << info.RepoOwner << "/"
                     << info.RepoName << ")";
        return GITHUB_PR_NOT_FOUND;
    }

    // 3. 获取 DB
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "HandlePRReviewWebhook: GetDB failed for repo " << repo_id << " PR #" << info.PRNumber;
        return GITHUB_DB_CONNECTION_FAILED;
    }

    // 4. 更新审查状态
    // 规则：一旦 approved 不再降级为 changes_requested（除非 dismissed）
    std::string current_status = pr->getReviewStatus();
    if (info.Action == "dismissed") {
        // 重新计算：保留其他 reviewer 的状态
        // 简化处理：设为 pending，后面的 reviewers 重建时会重新计算
        pr->setReviewStatus("pending");
    } else if (info.State == "approved") {
        pr->setReviewStatus("approved");
    } else if (info.State == "changes_requested") {
        if (current_status != "approved") {
            pr->setReviewStatus("changes_requested");
        }
    }
    // "commented" 类型的 review 不影响整体状态

    // 5. 更新审查者列表
    {
        Json::Value reviewerArr(Json::arrayValue);
        Json::Reader reader;

        if (!pr->getReviewers().empty() && reader.parse(pr->getReviewers(), reviewerArr) && reviewerArr.isArray()) {
            // 查找该 reviewer 是否已存在
            bool found = false;
            for (auto& r : reviewerArr) {
                if (r.isObject() && r["reviewer"].asString() == info.Reviewer) {
                    r["status"] = info.State;
                    if (!info.SubmittedAt.empty()) {
                        r["submitted_at"] = info.SubmittedAt;
                    }
                    found = true;
                    break;
                }
            }
            if (!found) {
                Json::Value entry;
                entry["reviewer"] = info.Reviewer;
                entry["status"] = info.State;
                if (!info.SubmittedAt.empty()) {
                    entry["submitted_at"] = info.SubmittedAt;
                }
                reviewerArr.append(entry);
            }
        } else {
            Json::Value entry;
            entry["reviewer"] = info.Reviewer;
            entry["status"] = info.State;
            if (!info.SubmittedAt.empty()) {
                entry["submitted_at"] = info.SubmittedAt;
            }
            reviewerArr.append(entry);
        }

        pr->setReviewers(chen::JsonUtil::ToString(reviewerArr));
    }

    pr->setUpdateTime(time(0));

    // 6. 写入 DB
    if (data::OrganizationRepoPrsInfoDao::InsertOrUpdate(pr, db)) {
        ERROR(logger) << "HandlePRReviewWebhook: Update failed for repo " << repo_id << " PR #" << info.PRNumber << ", errstr=" << db->getErrStr()
                      << ", errno=" << db->getErrno();
        return GITHUB_DB_OPERATION_FAILED;
    }
    OrganizationRepoPrMgr::GetInstance()->add(pr);

    DEBUG(logger) << "HandlePRReviewWebhook: updated review for PR #" << info.PRNumber << " reviewer=" << info.Reviewer << " state=" << info.State;
    return 0;
}

void BlogMsgFromBotInit(chen::rpc::RpcServer::ptr server)
{
    server->registerMethod(SS_CMD_GET_GITHUB_PR_INFO, HandlePRWebhook);
    server->registerMethod(SS_CMD_GET_GITHUB_PR_REVIEW_INFO, HandlePRReviewWebhook);
}

} // namespace blog
