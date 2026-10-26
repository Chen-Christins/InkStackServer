/**
 * @file permission.h
 * @brief RD 平台权限检查辅助函数
 * @author Christins
 * @date 2026-06-03
 * @copyright Apache 2.0
 */

#pragma once

#include "error_codes.h"
#include "manager/organization_manager.h"
#include "manager/user_manager.h"

namespace blog {
namespace permission {

using OrgRole = OrganizationManager::Role;

// ============================================================================
// 成员管理权限
// ============================================================================

/// 是否可以管理组织成员（审批加入、踢人、设角色）
inline bool CanManageMembers(int32_t system_role, int32_t org_role)
{
    if (system_role == UserManager::Role::ADMIN) {
        return true;
    }
    if (org_role == OrgRole::ORG_ADMIN || org_role == OrgRole::DEV_LEAD) {
        return true;
    }
    return false;
}

/// 是否可以将成员切换到目标角色
inline bool CanSwitchRole(int32_t system_role, int32_t org_role, int32_t target_role)
{
    if (system_role == UserManager::Role::ADMIN) {
        return true;
    }
    if (org_role == OrgRole::ORG_ADMIN) {
        return true;
    }
    // 研发主管可以将成员设置为: 普通成员、报告者、开发者
    if (org_role == OrgRole::DEV_LEAD && (target_role == OrgRole::MEMBER || target_role == OrgRole::REPORTER || target_role == OrgRole::DEVELOPER)) {
        return true;
    }
    return false;
}

// ============================================================================
// 平台访问权限
// ============================================================================

/// 是否可以进入研发平台: 系统管理员, 或在任一已批准组织中角色为报告者及以上
inline bool CanAccessPlatform(int32_t system_role, int32_t highest_org_role)
{
    if (system_role == UserManager::Role::ADMIN) {
        return true;
    }
    return highest_org_role >= OrgRole::REPORTER;
}

// ============================================================================
// 需求 (Requirement) 权限
// ============================================================================

/// 查看需求: 组织内成员可见全部
inline bool CanViewRequirement(int32_t org_role, int64_t user_id, int64_t creator_id)
{
    return org_role >= OrgRole::REPORTER;
}

/// 创建需求: 报告者及以上
inline bool CanCreateRequirement(int32_t org_role)
{
    return org_role >= OrgRole::REPORTER;
}

/// 编辑需求: 报告者及以上
inline bool CanEditRequirement(int32_t org_role)
{
    return org_role >= OrgRole::REPORTER;
}

/// 删除需求: 研发主管及以上
inline bool CanDeleteRequirement(int32_t org_role)
{
    return org_role >= OrgRole::DEV_LEAD;
}

/// 分配需求负责人: 研发主管及以上
inline bool CanAssignRequirement(int32_t org_role)
{
    return org_role >= OrgRole::REPORTER;
}

// ============================================================================
// 缺陷 (Bug) 权限
// ============================================================================

/// 查看缺陷: 组织内成员可见全部
inline bool CanViewBug(int32_t org_role, int64_t user_id, int64_t creator_id)
{
    return org_role >= OrgRole::REPORTER;
}

/// 创建缺陷: 报告者及以上
inline bool CanCreateBug(int32_t org_role)
{
    return org_role >= OrgRole::REPORTER;
}

/// 编辑缺陷: 开发者只能编辑自己创建的，报告者及以上编辑全部
inline bool CanEditBug(int32_t org_role, int64_t user_id, int64_t creator_id)
{
    if (org_role >= OrgRole::REPORTER) {
        return true;
    }
    return false;
}

/// 删除缺陷: 研发主管及以上
inline bool CanDeleteBug(int32_t org_role)
{
    return org_role >= OrgRole::DEV_LEAD;
}

/// 分配缺陷负责人: 研发主管及以上
inline bool CanAssignBug(int32_t org_role)
{
    return org_role >= OrgRole::REPORTER;
}

// ============================================================================
// 任务 (Task) 权限
// ============================================================================

/// 查看任务: 研发主管及以上看全部，其他人只看自己相关的
inline bool CanViewTask(int32_t org_role, bool is_related)
{
    if (org_role >= OrgRole::REPORTER) {
        return true;
    }
    return is_related;
}

/// 创建任务: 研发主管及以上
inline bool CanCreateTask(int32_t org_role)
{
    return org_role >= OrgRole::REPORTER;
}

/// 编辑任务: 开发者编辑自己负责的，研发主管及以上编辑全部
inline bool CanEditTask(int32_t org_role, int64_t user_id, int64_t assignee_id)
{
    if (org_role >= OrgRole::REPORTER) {
        return true;
    }
    return false;
}

/// 删除任务: 研发主管及以上
inline bool CanDeleteTask(int32_t org_role)
{
    return org_role >= OrgRole::DEV_LEAD;
}

/// 分配任务负责人: 研发主管及以上
inline bool CanAssignTask(int32_t org_role)
{
    return org_role >= OrgRole::REPORTER;
}

// ============================================================================
// 文章 (Article) 权限
// ============================================================================

/// 管理员和审核员发布的文章跳过审核，直接发布
inline bool CanBypassReview(int32_t system_role)
{
    return system_role == UserManager::Role::ADMIN || system_role == UserManager::Role::CHECKER;
}

// ============================================================================
// 管理端路由权限
// ============================================================================

/// 是否系统管理员
inline bool IsAdmin(int32_t system_role)
{
    return system_role == UserManager::Role::ADMIN;
}

/**
 * @brief 校验管理端路由访问权限
 * @param uid 当前登录用户 id
 * @param user 当前登录用户信息
 * @return 0 表示通过，否则返回需要下发的错误码
 */
inline int32_t CheckAdminRoute(int64_t uid, const data::UserInfo::ptr& user)
{
    if (!uid) {
        return errcode::NOT_LOGIN;
    }
    if (!user) {
        return errcode::USER_NOT_FOUND;
    }
    if (user->getState() != UserManager::Status::ACTIVE || user->getIsDeleted()) {
        return errcode::ACCOUNT_INVALID;
    }
    if (!IsAdmin(user->getRole())) {
        return errcode::ACCESS_DENIED;
    }
    return 0;
}

} // namespace permission
} // namespace blog
