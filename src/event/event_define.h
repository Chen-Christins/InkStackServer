/**
 * @file event_define.h
 * @brief 事件定义
 * @author Christins (chen.christins@qq.com)
 * @date 2026-07-25
 * @copyright Apache 2.0
 */
#pragma once

#include <map>
#include <string>
#include <vector>

#include <chen/ds/event_bus.h>
#include <chen/email/email.h>

namespace blog {

// 事件类型
enum EVENT_ID {
    EVENT_ID_USER_SEND_CODE = 1,        // 用户发送验证码
    EVENT_ID_COMMENT = 2,               // 评论审核事件
    EVENT_ID_ARTICLE_REVIEW = 3,        // 文章审核
    EVENT_ID_ARTICLE_STATE_CHANGED = 4, // 文章状态变更
    EVENT_ID_ARTICLE_PRAISE = 5,        // 文章点赞
    EVENT_ID_COMMENT_CREATED = 6,       // 评论创建
    EVENT_ID_COMMENT_PRAISE = 7,        // 评论点赞
    EVENT_ID_USER_FOLLOW = 8,           // 用户关注
    EVENT_ID_USER_ADMIN = 9,            // 管理员操作用户
    EVENT_ID_ORG_DELETED = 10,          // 组织删除
    EVENT_ID_ORG_MEMBER = 11,           // 组织成员变更
    EVENT_ID_ORG_APPLY = 12,            // 组织申请
    EVENT_ID_ASSIGNMENT = 13,           // 作业
    EVENT_ID_ASSIGNMENT_SUBMITTED = 14, // 作业提交
    EVENT_ID_RD_ASSIGN = 15,            // RD 指派
    EVENT_ID_DATABASE_BACKUP = 16,      // 数据库备份
};

struct EventUserSendCodeData {
    chen::EMail::ptr email;
    std::string smtp_host;
    uint16_t port = 0;
    /// 是否使用隐式 TLS（465）；由 ResolveSmtpTls 根据 smtp_encryption 解析
    bool use_ssl = false;
};

struct EventCommentData {
    int64_t comment_id;
    int64_t author_id;
    std::string type;
};

struct EventArticleReviewData {
    std::string type;
    int64_t article_id;
    std::string article_title;
    int64_t author_id;
    std::string author_name;
    int64_t reviewer_id;
};

struct EventArticleStateChangedData {
    int64_t author_id;
    int64_t article_id;
    std::string article_title;
    int32_t new_state;
};

struct EventArticlePraiseData {
    int64_t author_id;
    int64_t liker_id;
    std::string liker_name;
    int64_t article_id;
    std::string article_title;
};

struct EventCommentCreatedData {
    int64_t commenter_id;
    std::string commenter_name;
    int64_t article_id;
    std::string article_title;
    int64_t author_id;
    int64_t parent_comment_id;
    int64_t parent_comment_author_id;
};

struct EventCommentPraiseData {
    int64_t comment_author_id;
    int64_t liker_id;
    std::string liker_name;
    int64_t article_id;
    int64_t comment_id;
};

struct EventUserFollowData {
    int64_t follower_id;
    std::string follower_name;
    int64_t following_id;
};

struct EventUserAdminData {
    std::string type;
    int64_t user_id;
};

struct EventOrgDeletedData {
    int64_t member_id;
    int64_t org_id;
    std::string org_name;
};

struct EventOrgMemberData {
    std::string type;
    int64_t org_id;
    std::string org_name;
    int64_t operator_id;
    std::string operator_name;
    int64_t target_user_id;
    std::string new_role_name;
    int64_t applicant_id;
    std::string applicant_name;
    int64_t kicked_user_id;
    std::string kicked_user_name;
    bool approved;
};

struct EventOrgApplyData {
    std::string type;
    int64_t applicant_id;
    std::string applicant_name;
    std::string org_name;
    int64_t apply_id;
    int64_t org_id;
    std::string reason;
};

struct EventAssignmentData {
    std::string type;
    int64_t assignment_id;
    std::string assignment_name;
    int64_t submitter_id;
    std::string subject_name;
    std::vector<int64_t> member_user_ids;
};

struct EventAssignmentSubmittedData {
    int64_t submitter_id;
    std::string submitter_name;
    int64_t assignment_id;
    std::string assignment_name;
    std::vector<int64_t> admin_user_ids;
};

struct EventRdAssignData {
    std::string type;
    int64_t assignee_id;
    int64_t item_id;
    std::string item_title;
    int64_t creator_id;
};

struct EventDatabaseBackupData {
    int64_t backup_id;
    std::string type;
    std::string name;
    std::map<std::string, std::map<std::string, std::string>> mysql_dbs;
    std::string work_path;
};

} // namespace blog
