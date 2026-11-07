/**
 * @file rd_helper.h
 * @brief RD 模块辅助函数（分页、状态枚举映射、JSON 构建等）
 * @author Christins
 * @date 2026-06-03
 * @copyright Apache 2.0
 */
#pragma once

#include "../../manager/bug_manager.h"
#include "../../manager/organization_manager.h"
#include "../../manager/requirement_manager.h"
#include "../../manager/task_manager.h"
#include "../../manager/user_manager.h"
#include "blog/data/bug_info.h"
#include "blog/data/requirement_info.h"
#include "blog/data/task_info.h"

#include <chen/http/http.h>
#include <chen/util/time_util.h>
#include <json/json.h>

#include <ctime>
#include <string>

namespace blog {
namespace servlet {
namespace rd {

// ============================================================================
// 分页: page/page_size → offset/size
// ============================================================================
inline void ParsePagination(uint64_t page, uint64_t page_size, uint64_t& offset, uint64_t& size)
{
    if (page < 1) {
        page = 1;
    }
    offset = (page - 1) * page_size;
    size = page_size;
}

// ============================================================================
// 日期/时间桶
// ============================================================================
inline time_t ParseDate(const std::string& date_str)
{
    return chen::TimeUtil::Str2Time(date_str.c_str(), "%Y-%m-%d");
}

inline std::string FormatDate(time_t t)
{
    return chen::TimeUtil::Time2StrSec(t, "%Y-%m-%d");
}

inline std::string FormatGroupKey(time_t t, const std::string& granularity)
{
    if (granularity == "week") {
        return FormatDate(chen::TimeUtil::GetWeekStart(t));
    }
    if (granularity == "month") {
        return chen::TimeUtil::Time2StrSec(chen::TimeUtil::GetMonthStart(t), "%Y-%m");
    }
    return FormatDate(t);
}

// ============================================================================
// 需求状态枚举映射
// ============================================================================
inline const char* RequirementStatusToString(int32_t status)
{
    switch (status) {
    case RequirementManager::STATUS_DRAFT:
        return "new";
    case RequirementManager::STATUS_PENDING:
        return "developing";
    case RequirementManager::STATUS_INPROGRESS:
        return "testing";
    case RequirementManager::STATUS_DONE:
        return "done";
    case RequirementManager::STATUS_CLOSED:
        return "closed";
    default:
        return "new";
    }
}

inline int32_t StringToRequirementStatus(const std::string& s)
{
    if (s == "new") {
        return RequirementManager::STATUS_DRAFT;
    }
    if (s == "developing") {
        return RequirementManager::STATUS_PENDING;
    }
    if (s == "testing") {
        return RequirementManager::STATUS_INPROGRESS;
    }
    if (s == "done") {
        return RequirementManager::STATUS_DONE;
    }
    if (s == "closed") {
        return RequirementManager::STATUS_CLOSED;
    }
    return -1;
}

// ============================================================================
// 缺陷状态枚举映射
// ============================================================================
inline const char* BugStatusToString(int32_t status)
{
    switch (status) {
    case BugManager::STATUS_PENDING:
        return "new";
    case BugManager::STATUS_INPROGRESS:
        return "processing";
    case BugManager::STATUS_FIXED:
        return "verified";
    case BugManager::STATUS_CLOSED:
        return "closed";
    case BugManager::STATUS_REOPENED:
        return "reopened";
    default:
        return "new";
    }
}

inline int32_t StringToBugStatus(const std::string& s)
{
    if (s == "new") {
        return BugManager::STATUS_PENDING;
    }
    if (s == "processing") {
        return BugManager::STATUS_INPROGRESS;
    }
    if (s == "verified") {
        return BugManager::STATUS_FIXED;
    }
    if (s == "closed") {
        return BugManager::STATUS_CLOSED;
    }
    if (s == "reopened") {
        return BugManager::STATUS_REOPENED;
    }
    return -1;
}

// ============================================================================
// 任务状态枚举映射
// ============================================================================
inline const char* TaskStatusToString(int32_t status)
{
    switch (status) {
    case TaskManager::STATUS_TODO:
        return "todo";
    case TaskManager::STATUS_INPROGRESS:
        return "doing";
    case TaskManager::STATUS_DONE:
        return "done";
    case TaskManager::STATUS_CLOSED:
        return "closed";
    default:
        return "todo";
    }
}

inline int32_t StringToTaskStatus(const std::string& s)
{
    if (s == "todo") {
        return TaskManager::STATUS_TODO;
    }
    if (s == "doing") {
        return TaskManager::STATUS_INPROGRESS;
    }
    if (s == "done") {
        return TaskManager::STATUS_DONE;
    }
    if (s == "closed") {
        return TaskManager::STATUS_CLOSED;
    }
    return -1;
}

// ============================================================================
// 优先级枚举映射
// ============================================================================
inline const char* PriorityToString(int32_t priority)
{
    switch (priority) {
    case 1:
        return "low";
    case 2:
        return "medium";
    case 3:
        return "high";
    case 4:
        return "urgent";
    default:
        return "medium";
    }
}

inline int32_t StringToPriority(const std::string& s)
{
    if (s == "low") {
        return 1;
    }
    if (s == "medium") {
        return 2;
    }
    if (s == "high") {
        return 3;
    }
    if (s == "urgent") {
        return 4;
    }
    return -1;
}

// ============================================================================
// 严重程度枚举映射
// ============================================================================
inline const char* SeverityToString(int32_t severity)
{
    switch (severity) {
    case 1:
        return "minor";
    case 2:
        return "normal";
    case 3:
        return "serious";
    case 4:
        return "fatal";
    default:
        return "normal";
    }
}

inline int32_t StringToSeverity(const std::string& s)
{
    if (s == "minor") {
        return 1;
    }
    if (s == "normal") {
        return 2;
    }
    if (s == "serious") {
        return 3;
    }
    if (s == "fatal") {
        return 4;
    }
    return -1;
}

// ============================================================================
// 组织角色枚举映射（用于前端展示）
// ============================================================================
inline int32_t OrgRoleToFrontend(int32_t role)
{
    // 前端期望: 1=报告者, 2=开发者, 3=研发主管, 4=组织管理员
    switch (role) {
    case OrganizationManager::Role::MEMBER:
        return 0;
    case OrganizationManager::Role::REPORTER:
        return 1;
    case OrganizationManager::Role::DEVELOPER:
        return 2;
    case OrganizationManager::Role::DEV_LEAD:
        return 3;
    case OrganizationManager::Role::ORG_ADMIN:
        return 4;
    default:
        return 0;
    }
}

// ============================================================================
// 构建公共响应字段
// ============================================================================

inline std::string GetOrgName(int64_t orgId)
{
    auto org = OrganizationMgr::GetInstance()->get(orgId);
    return org ? org->getName() : "";
}

inline std::string GetUserName(int64_t userId)
{
    auto user = UserMgr::GetInstance()->get(userId);
    return user ? user->getName() : "";
}

inline std::string GetUserAvatar(int64_t userId)
{
    auto user = UserMgr::GetInstance()->get(userId);
    return user ? user->getAvatar() : "";
}

// ============================================================================
// 构建 JSON 响应对象
// ============================================================================
inline void BuildRequirementJson(Json::Value& item, data::RequirementInfo::ptr info)
{
    item["id"] = info->getId();
    item["title"] = info->getTitle();
    item["description"] = info->getDescription();
    item["priority"] = PriorityToString(info->getPriority());
    item["status"] = RequirementStatusToString(info->getStatus());
    item["creator"] = GetUserName(info->getCreatorId());
    item["creator_id"] = info->getCreatorId();
    item["assignee"] = GetUserName(info->getAssigneeId());
    item["assignee_avatar"] = GetUserAvatar(info->getAssigneeId());
    item["assignee_id"] = info->getAssigneeId();
    item["org_id"] = info->getOrgId();
    item["org_name"] = GetOrgName(info->getOrgId());
    item["iteration"] = info->getIteration();
    item["category"] = info->getCategory();
    item["source"] = info->getSource();
    item["deadline"] = info->getDeadline();
    item["created_at"] = info->getCreateTime();
    item["updated_at"] = info->getUpdateTime();
}

inline void BuildBugJson(Json::Value& item, data::BugInfo::ptr info)
{
    item["id"] = info->getId();
    item["title"] = info->getTitle();
    item["description"] = info->getDescription();
    item["severity"] = SeverityToString(info->getSeverity());
    item["priority"] = PriorityToString(info->getPriority());
    item["status"] = BugStatusToString(info->getStatus());
    item["creator"] = GetUserName(info->getCreatorId());
    item["creator_id"] = info->getCreatorId();
    item["assignee"] = GetUserName(info->getAssigneeId());
    item["assignee_avatar"] = GetUserAvatar(info->getAssigneeId());
    item["assignee_id"] = info->getAssigneeId();
    item["org_id"] = info->getOrgId();
    item["org_name"] = GetOrgName(info->getOrgId());
    item["related_requirement_id"] = info->getRequirementId();
    item["module"] = info->getModule();
    item["steps_to_reproduce"] = info->getStepsToReproduce();
    item["environment"] = info->getEnvironment();
    item["created_at"] = info->getCreateTime();
    item["updated_at"] = info->getUpdateTime();
}

inline void BuildTaskJson(Json::Value& item, data::TaskInfo::ptr info)
{
    item["id"] = info->getId();
    item["title"] = info->getTitle();
    item["description"] = info->getDescription();
    item["status"] = TaskStatusToString(info->getStatus());
    item["priority"] = PriorityToString(info->getPriority());
    item["creator"] = GetUserName(info->getCreatorId());
    item["creator_id"] = info->getCreatorId();
    item["assignee"] = GetUserName(info->getAssigneeId());
    item["assignee_avatar"] = GetUserAvatar(info->getAssigneeId());
    item["assignee_id"] = info->getAssigneeId();
    item["org_id"] = info->getOrgId();
    item["org_name"] = GetOrgName(info->getOrgId());
    item["requirement_id"] = info->getRequirementId();
    item["bug_id"] = info->getBugId();
    item["deadline"] = info->getDeadline();
    item["estimated_hours"] = info->getEstimatedHours();
    item["created_at"] = info->getCreateTime();
    item["updated_at"] = info->getUpdateTime();
}

} // namespace rd
} // namespace servlet
} // namespace blog
