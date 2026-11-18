#include "common/event/Events.hpp"

#include <chen/config/config.h>
#include <chen/ds/event_bus.h>
#include <chen/email/smtp.h>
#include <chen/log/log.h>
#include <chen/util/fs_util.h>
#include <json/json.h>

#include "article/manager/ArticleManager.hpp"
#include "platform/manager/BackupRecordManager.hpp"
#include "comment/manager/CommentManager.hpp"
#include "platform/manager/NotificationManager.hpp"
#include "platform/manager/OrganizationManager.hpp"
#include "platform/manager/OrganizationUserRelManager.hpp"
#include "user/manager/UserManager.hpp"
#include "common/Util.hpp"
#include "common/event/EventDefine.hpp"

namespace blog {

static chen::Logger::ptr logger = LOG_ROOT();

static void EventUserSendCode(const std::any& d)
{
    auto data = std::any_cast<const EventUserSendCodeData&>(d);

    auto client = chen::SmtpClient::Create(data.smtp_host, data.port, true);
    if (!client) {
        ERROR(logger) << "connect email server fail";
        return;
    }

    if (!data.email) {
        ERROR(logger) << "email is null";
        return;
    }

    auto r = client->send(data.email, 5000);
    if (r->result != 0) {
        ERROR(logger) << "send email fail: " << r->result << " " << r->msg;
    }
}

static void EventComment(const std::any& d)
{
    auto data = std::any_cast<const EventCommentData&>(d);

    auto comment = CommentMgr::GetInstance()->get(data.comment_id);
    if (!comment) {
        ERROR(logger) << "comment not found, id=" << data.comment_id;
        return;
    }

    auto& type = data.type;
    std::string title, content;
    if (type == "comment_approved") {
        title = "评论审核通过";
        content = "你的评论「" + comment->getContent() + "」已通过审核";
    } else if (type == "comment_admin_deleted") {
        title = "评论被删除";
        content = "你的评论「" + comment->getContent() + "」已被管理员删除";
    } else if (type == "comment_rejected") {
        title = "评论未通过审核";
        content = "你的评论「" + comment->getContent() + "」未通过审核";
    } else if (type == "comment_spam") {
        title = "评论被标记为垃圾";
        content = "你的评论「" + comment->getContent() + "」已被标记为垃圾评论";
    } else {
        return;
    }

    NotificationData notif_data{};
    notif_data.user_id = data.author_id;
    notif_data.title = title;
    notif_data.content = content;
    notif_data.type = type;
    notif_data.sender_id = 0;
    notif_data.article_id = comment->getArticleId();
    notif_data.comment_id = data.comment_id;

    auto notif = NotificationMgr::GetInstance()->addNotification(notif_data);
    if (notif) {
        Json::Value wsMsg;
        wsMsg["id"] = notif->getId();
        wsMsg["title"] = title;
        wsMsg["content"] = content;
        wsMsg["type"] = type;
        wsMsg["article_id"] = comment->getArticleId();
        wsMsg["comment_id"] = data.comment_id;
        wsMsg["is_read"] = false;
        wsMsg["create_time"] = notif->getCreateTime();
        NotificationMgr::GetInstance()->sendToUser(data.author_id, chen::JsonUtil::ToString(wsMsg));
    }
}

static void EventArticleReview(const std::any& d)
{
    auto data = std::any_cast<const EventArticleReviewData&>(d);

    if (data.type == "request") {
        // Notify admins and checkers about new article pending review
        std::string title = "新的文章待审核";
        std::string content = "「" + data.author_name + "」提交了文章「" + data.article_title + "」等待审核";

        std::vector<int64_t> userIds;
        UserMgr::GetInstance()->getAllIds(userIds, true);
        for (auto targetId : userIds) {
            auto u = UserMgr::GetInstance()->get(targetId);
            if (u && (u->getRole() == UserManager::Role::ADMIN || u->getRole() == UserManager::Role::CHECKER)) {
                NotificationData notif_data{};
                notif_data.user_id = targetId;
                notif_data.title = title;
                notif_data.content = content;
                notif_data.type = "article_review_request";
                notif_data.sender_id = data.author_id;
                notif_data.article_id = data.article_id;

                auto notif_info = NotificationMgr::GetInstance()->addNotification(notif_data);
                if (notif_info) {
                    Json::Value wsMsg;
                    wsMsg["id"] = notif_info->getId();
                    wsMsg["title"] = title;
                    wsMsg["content"] = content;
                    wsMsg["type"] = "article_review_request";
                    wsMsg["article_id"] = data.article_id;
                    wsMsg["is_read"] = false;
                    wsMsg["create_time"] = notif_info->getCreateTime();
                    NotificationMgr::GetInstance()->sendToUser(targetId, chen::JsonUtil::ToString(wsMsg));
                }
            }
        }
    } else {
        bool approved = (data.type == "approved");
        const char* notif_type = approved ? "article_review_approved" : "article_review_rejected";
        std::string title = approved ? "文章审核通过" : "文章审核未通过";
        std::string content = "您的文章「" + data.article_title + "」" + (approved ? "已通过审核" : "未通过审核");

        NotificationData notif_data{};
        notif_data.user_id = data.author_id;
        notif_data.title = title;
        notif_data.content = content;
        notif_data.type = notif_type;
        notif_data.sender_id = data.reviewer_id;
        notif_data.article_id = data.article_id;

        auto notif_info = NotificationMgr::GetInstance()->addNotification(notif_data);
        if (notif_info) {
            Json::Value wsMsg;
            wsMsg["id"] = notif_info->getId();
            wsMsg["title"] = title;
            wsMsg["content"] = content;
            wsMsg["type"] = notif_type;
            wsMsg["article_id"] = data.article_id;
            wsMsg["is_read"] = false;
            wsMsg["create_time"] = notif_info->getCreateTime();
            NotificationMgr::GetInstance()->sendToUser(data.author_id, chen::JsonUtil::ToString(wsMsg));
        }

        // Mark other admins/checkers' article_review_request as read
        std::vector<int64_t> userIds;
        UserMgr::GetInstance()->getAllIds(userIds, true);
        for (auto targetId : userIds) {
            auto u = UserMgr::GetInstance()->get(targetId);
            if (u && (u->getRole() == UserManager::Role::ADMIN || u->getRole() == UserManager::Role::CHECKER)) {
                NotificationMgr::GetInstance()->markReadByType(targetId, "article_review_request");
            }
        }
    }
}

static void EventArticleStateChanged(const std::any& d)
{
    auto data = std::any_cast<const EventArticleStateChangedData&>(d);

    std::string title = "文章状态变更";
    std::string content =
        "你的文章《" + data.article_title + "》状态已被管理员变更为「" + (data.new_state == ArticleManager::PUBLISHED ? "已发布" : "私密") + "」";

    NotificationData notif_data{};
    notif_data.user_id = data.author_id;
    notif_data.title = title;
    notif_data.content = content;
    notif_data.type = "article_state_changed";
    notif_data.sender_id = 0;
    notif_data.article_id = data.article_id;

    auto notif = NotificationMgr::GetInstance()->addNotification(notif_data);
    if (notif) {
        Json::Value wsMsg;
        wsMsg["id"] = notif->getId();
        wsMsg["title"] = title;
        wsMsg["content"] = content;
        wsMsg["type"] = "article_state_changed";
        wsMsg["article_id"] = data.article_id;
        wsMsg["is_read"] = false;
        wsMsg["create_time"] = notif->getCreateTime();
        NotificationMgr::GetInstance()->sendToUser(data.author_id, chen::JsonUtil::ToString(wsMsg));
    }
}

static void EventArticlePraise(const std::any& d)
{
    auto data = std::any_cast<const EventArticlePraiseData&>(d);

    std::string notify_title = "文章点赞";
    std::string notify_content = data.liker_name + " 赞了你的文章《" + data.article_title + "》";

    NotificationData notif_data{};
    notif_data.user_id = data.author_id;
    notif_data.title = notify_title;
    notif_data.content = notify_content;
    notif_data.type = "praise";
    notif_data.sender_id = data.liker_id;
    notif_data.article_id = data.article_id;

    auto notif_info = NotificationMgr::GetInstance()->addNotification(notif_data);
    if (notif_info) {
        Json::Value wsMsg;
        wsMsg["id"] = notif_info->getId();
        wsMsg["title"] = notify_title;
        wsMsg["content"] = notify_content;
        wsMsg["type"] = "praise";
        wsMsg["article_id"] = data.article_id;
        wsMsg["is_read"] = false;
        wsMsg["create_time"] = notif_info->getCreateTime();
        NotificationMgr::GetInstance()->sendToUser(data.author_id, chen::JsonUtil::ToString(wsMsg));
    }
}

static void EventCommentCreated(const std::any& d)
{
    auto data = std::any_cast<const EventCommentCreatedData&>(d);

    auto send_notify = [&](int64_t targetUid, const std::string& title, const std::string& content) {
        if (targetUid == data.commenter_id) {
            return;
        }

        NotificationData notif_data{};
        notif_data.user_id = targetUid;
        notif_data.title = title;
        notif_data.content = content;
        notif_data.type = "comment";
        notif_data.sender_id = data.commenter_id;
        notif_data.article_id = data.article_id;

        auto notif_info = NotificationMgr::GetInstance()->addNotification(notif_data);
        if (notif_info) {
            Json::Value wsMsg;
            wsMsg["id"] = notif_info->getId();
            wsMsg["title"] = title;
            wsMsg["content"] = content;
            wsMsg["type"] = "comment";
            wsMsg["article_id"] = data.article_id;
            wsMsg["is_read"] = false;
            wsMsg["create_time"] = notif_info->getCreateTime();
            NotificationMgr::GetInstance()->sendToUser(targetUid, chen::JsonUtil::ToString(wsMsg));
        }
    };

    // notify article author
    send_notify(data.author_id, "文章评论", data.commenter_name + " 评论了你的文章《" + data.article_title + "》");

    // notify parent comment author on reply
    if (data.parent_comment_id > 0 && data.parent_comment_author_id > 0 && data.parent_comment_author_id != data.author_id) {
        send_notify(data.parent_comment_author_id, "评论回复", data.commenter_name + " 回复了你的评论");
    }
}

static void EventCommentPraise(const std::any& d)
{
    auto data = std::any_cast<const EventCommentPraiseData&>(d);

    std::string notify_title = "评论点赞";
    std::string notify_content = data.liker_name + " 赞了你的评论";

    NotificationData notif_data{};
    notif_data.user_id = data.comment_author_id;
    notif_data.title = notify_title;
    notif_data.content = notify_content;
    notif_data.type = "comment_praise";
    notif_data.sender_id = data.liker_id;
    notif_data.article_id = data.article_id;
    notif_data.comment_id = data.comment_id;

    auto notif_info = NotificationMgr::GetInstance()->addNotification(notif_data);
    if (notif_info) {
        Json::Value wsMsg;
        wsMsg["id"] = notif_info->getId();
        wsMsg["title"] = notify_title;
        wsMsg["content"] = notify_content;
        wsMsg["type"] = "comment_praise";
        wsMsg["article_id"] = data.article_id;
        wsMsg["comment_id"] = data.comment_id;
        wsMsg["is_read"] = false;
        wsMsg["create_time"] = notif_info->getCreateTime();
        NotificationMgr::GetInstance()->sendToUser(data.comment_author_id, chen::JsonUtil::ToString(wsMsg));
    }
}

static void EventUserFollow(const std::any& d)
{
    auto data = std::any_cast<const EventUserFollowData&>(d);

    std::string notify_title = "新关注";
    std::string notify_content = data.follower_name + " 关注了你";

    NotificationData notif_data{};
    notif_data.user_id = data.following_id;
    notif_data.title = notify_title;
    notif_data.content = notify_content;
    notif_data.type = "follow";
    notif_data.sender_id = data.follower_id;

    auto notif_info = NotificationMgr::GetInstance()->addNotification(notif_data);
    if (notif_info) {
        Json::Value wsMsg;
        wsMsg["id"] = notif_info->getId();
        wsMsg["title"] = notify_title;
        wsMsg["content"] = notify_content;
        wsMsg["type"] = "follow";
        wsMsg["is_read"] = false;
        wsMsg["create_time"] = notif_info->getCreateTime();
        NotificationMgr::GetInstance()->sendToUser(data.following_id, chen::JsonUtil::ToString(wsMsg));
    }
}

static void EventUserAdminAction(const std::any& d)
{
    auto data = std::any_cast<const EventUserAdminData&>(d);

    std::string title, content;
    if (data.type == "password_reset") {
        title = "密码已被重置";
        content = "你的账户密码已被管理员重置，请尽快修改密码";
    } else if (data.type == "account_recovered") {
        title = "账户已恢复";
        content = "你的账户已被管理员恢复，现在可以正常使用";
    } else if (data.type == "account_deleted") {
        title = "账户已被禁用";
        content = "你的账户已被管理员禁用，如有疑问请联系管理员";
    } else {
        return;
    }

    NotificationData notif_data{};
    notif_data.user_id = data.user_id;
    notif_data.title = title;
    notif_data.content = content;
    notif_data.type = data.type;
    notif_data.sender_id = 0;

    auto notif = NotificationMgr::GetInstance()->addNotification(notif_data);
    if (notif) {
        Json::Value wsMsg;
        wsMsg["id"] = notif->getId();
        wsMsg["title"] = title;
        wsMsg["content"] = content;
        wsMsg["type"] = data.type;
        wsMsg["is_read"] = false;
        wsMsg["create_time"] = notif->getCreateTime();
        NotificationMgr::GetInstance()->sendToUser(data.user_id, chen::JsonUtil::ToString(wsMsg));
    }
}

static void EventOrgDeleted(const std::any& d)
{
    auto data = std::any_cast<const EventOrgDeletedData&>(d);

    std::string title = "组织已删除";
    std::string content = "组织「" + data.org_name + "」已被管理员删除";

    NotificationData notif_data{};
    notif_data.user_id = data.member_id;
    notif_data.title = title;
    notif_data.content = content;
    notif_data.type = "org_deleted";
    notif_data.sender_id = 0;

    auto notif = NotificationMgr::GetInstance()->addNotification(notif_data);
    if (notif) {
        Json::Value wsMsg;
        wsMsg["id"] = notif->getId();
        wsMsg["title"] = title;
        wsMsg["content"] = content;
        wsMsg["type"] = "org_deleted";
        wsMsg["is_read"] = false;
        wsMsg["create_time"] = notif->getCreateTime();
        NotificationMgr::GetInstance()->sendToUser(data.member_id, chen::JsonUtil::ToString(wsMsg));
    }
}

static void EventOrgMember(const std::any& d)
{
    auto data = std::any_cast<const EventOrgMemberData&>(d);

    if (data.type == "role_change") {
        std::string title = "组织角色变更";
        std::string content = "您在组织「" + data.org_name + "」中的角色已被更新为" + data.new_role_name;

        NotificationData notif_data{};
        notif_data.user_id = data.target_user_id;
        notif_data.title = title;
        notif_data.content = content;
        notif_data.type = "org_role_change";
        notif_data.sender_id = data.operator_id;

        auto notif_info = NotificationMgr::GetInstance()->addNotification(notif_data);
        if (notif_info) {
            Json::Value wsMsg;
            wsMsg["id"] = notif_info->getId();
            wsMsg["title"] = title;
            wsMsg["content"] = content;
            wsMsg["type"] = "org_role_change";
            wsMsg["is_read"] = false;
            wsMsg["create_time"] = notif_info->getCreateTime();
            NotificationMgr::GetInstance()->sendToUser(data.target_user_id, chen::JsonUtil::ToString(wsMsg));
        }
    } else if (data.type == "kicked") {
        std::string title = "成员被移出组织";
        std::string content = "「" + data.operator_name + "」将「" + data.applicant_name + "」移出了组织「" + data.org_name + "」";
        // Notify all org admins
        std::vector<data::OrganizationUserRelInfo::ptr> members;
        OrganizationUserRelMgr::GetInstance()->getByPages(members, data.org_id, 0, 10000, -1, true);
        for (auto& m : members) {
            if (m->getRole() == OrganizationManager::Role::ORG_ADMIN) {
                NotificationData notif_data{};
                notif_data.user_id = m->getUserId();
                notif_data.title = title;
                notif_data.content = content;
                notif_data.type = "org_member_kicked";
                notif_data.sender_id = data.operator_id;

                auto notif_info = NotificationMgr::GetInstance()->addNotification(notif_data);
                if (notif_info) {
                    Json::Value wsMsg;
                    wsMsg["id"] = notif_info->getId();
                    wsMsg["title"] = title;
                    wsMsg["content"] = content;
                    wsMsg["type"] = "org_member_kicked";
                    wsMsg["is_read"] = false;
                    wsMsg["create_time"] = notif_info->getCreateTime();
                    NotificationMgr::GetInstance()->sendToUser(m->getUserId(), chen::JsonUtil::ToString(wsMsg));
                }
            }
        }
        // Notify kicked user
        {
            std::string kicked_title = "您已被移出组织";
            std::string kicked_content = "您已被移出组织「" + data.org_name + "」";

            NotificationData notif_data{};
            notif_data.user_id = data.kicked_user_id;
            notif_data.title = kicked_title;
            notif_data.content = kicked_content;
            notif_data.type = "org_member_kicked";
            notif_data.sender_id = data.operator_id;

            auto notif_info = NotificationMgr::GetInstance()->addNotification(notif_data);
            if (notif_info) {
                Json::Value wsMsg;
                wsMsg["id"] = notif_info->getId();
                wsMsg["title"] = kicked_title;
                wsMsg["content"] = kicked_content;
                wsMsg["type"] = "org_member_kicked";
                wsMsg["is_read"] = false;
                wsMsg["create_time"] = notif_info->getCreateTime();
                NotificationMgr::GetInstance()->sendToUser(data.kicked_user_id, chen::JsonUtil::ToString(wsMsg));
            }
        }
    } else if (data.type == "join_request") {
        auto applicant = UserMgr::GetInstance()->get(data.applicant_id);
        std::string applicant_name = applicant ? applicant->getName() : std::to_string(data.applicant_id);
        std::string title = "新的加入申请";
        std::string content = "用户「" + applicant_name + "」申请加入组织「" + data.org_name + "」";
        std::vector<data::OrganizationUserRelInfo::ptr> members;
        OrganizationUserRelMgr::GetInstance()->getByPages(members, data.org_id, 0, 10000, -1, true);
        for (auto& m : members) {
            if (m->getRole() == OrganizationManager::Role::ORG_ADMIN) {
                NotificationData notif_data{};
                notif_data.user_id = m->getUserId();
                notif_data.title = title;
                notif_data.content = content;
                notif_data.type = "org_join_request";
                notif_data.sender_id = data.applicant_id;

                auto notif_info = NotificationMgr::GetInstance()->addNotification(notif_data);
                if (notif_info) {
                    Json::Value wsMsg;
                    wsMsg["id"] = notif_info->getId();
                    wsMsg["title"] = title;
                    wsMsg["content"] = content;
                    wsMsg["type"] = "org_join_request";
                    wsMsg["is_read"] = false;
                    wsMsg["create_time"] = notif_info->getCreateTime();
                    NotificationMgr::GetInstance()->sendToUser(m->getUserId(), chen::JsonUtil::ToString(wsMsg));
                }
            }
        }
    } else if (data.type == "join_approved" || data.type == "join_rejected") {
        bool approved = (data.type == "join_approved");
        const char* notif_type = approved ? "org_join_approved" : "org_join_rejected";
        std::string title = approved ? "加入申请已通过" : "加入申请被拒绝";
        std::string content =
            approved ? "您申请加入组织「" + data.org_name + "」的请求已通过" : "您申请加入组织「" + data.org_name + "」的请求已被拒绝";

        NotificationData notif_data{};
        notif_data.user_id = data.applicant_id;
        notif_data.title = title;
        notif_data.content = content;
        notif_data.type = notif_type;
        notif_data.sender_id = data.operator_id;

        auto notif_info = NotificationMgr::GetInstance()->addNotification(notif_data);
        if (notif_info) {
            Json::Value wsMsg;
            wsMsg["id"] = notif_info->getId();
            wsMsg["title"] = title;
            wsMsg["content"] = content;
            wsMsg["type"] = notif_type;
            wsMsg["is_read"] = false;
            wsMsg["create_time"] = notif_info->getCreateTime();
            NotificationMgr::GetInstance()->sendToUser(data.applicant_id, chen::JsonUtil::ToString(wsMsg));
        }
        // Mark other admins' org_join_request as read
        std::vector<data::OrganizationUserRelInfo::ptr> members;
        OrganizationUserRelMgr::GetInstance()->getByPages(members, data.org_id, 0, 10000, -1, true);
        for (auto& m : members) {
            if (m->getRole() == OrganizationManager::Role::ORG_ADMIN) {
                NotificationMgr::GetInstance()->markReadByType(m->getUserId(), "org_join_request");
            }
        }
    }
}

static void EventOrgApply(const std::any& d)
{
    auto data = std::any_cast<const EventOrgApplyData&>(d);

    if (data.type == "created") {
        std::string title = "新的组织申请";
        std::string content = "用户「" + data.applicant_name + "」申请创建组织「" + data.org_name + "」";
        std::vector<data::UserInfo::ptr> admins;
        UserMgr::GetInstance()->listByPages(admins, 0, 10000, UserManager::Role::ADMIN, -1, -1, true);
        for (auto& admin : admins) {
            NotificationData notif_data{};
            notif_data.user_id = admin->getId();
            notif_data.title = title;
            notif_data.content = content;
            notif_data.type = "org_apply_request";
            notif_data.sender_id = 0;
            notif_data.article_id = data.apply_id;

            auto notif = NotificationMgr::GetInstance()->addNotification(notif_data);
            if (notif) {
                Json::Value wsMsg;
                wsMsg["id"] = notif->getId();
                wsMsg["title"] = title;
                wsMsg["content"] = content;
                wsMsg["type"] = "org_apply_request";
                wsMsg["apply_id"] = data.apply_id;
                wsMsg["is_read"] = false;
                wsMsg["create_time"] = notif->getCreateTime();
                NotificationMgr::GetInstance()->sendToUser(admin->getId(), chen::JsonUtil::ToString(wsMsg));
            }
        }
    } else if (data.type == "approved") {
        std::string title = "组织申请已通过";
        std::string content = "你申请创建的组织「" + data.org_name + "」已通过审核";

        NotificationData notif_data{};
        notif_data.user_id = data.applicant_id;
        notif_data.title = title;
        notif_data.content = content;
        notif_data.type = "org_apply_approved";
        notif_data.sender_id = 0;
        notif_data.article_id = data.org_id;

        auto notif = NotificationMgr::GetInstance()->addNotification(notif_data);
        if (notif) {
            Json::Value wsMsg;
            wsMsg["id"] = notif->getId();
            wsMsg["title"] = title;
            wsMsg["content"] = content;
            wsMsg["type"] = "org_apply_approved";
            wsMsg["org_id"] = data.org_id;
            wsMsg["is_read"] = false;
            wsMsg["create_time"] = notif->getCreateTime();
            NotificationMgr::GetInstance()->sendToUser(data.applicant_id, chen::JsonUtil::ToString(wsMsg));
        }
    } else if (data.type == "rejected") {
        std::string title = "组织申请被拒绝";
        std::string reject_reason = data.reason.empty() ? "" : "，原因：" + data.reason;
        std::string content = "你申请创建的组织「" + data.org_name + "」未通过审核" + reject_reason;

        NotificationData notif_data{};
        notif_data.user_id = data.applicant_id;
        notif_data.title = title;
        notif_data.content = content;
        notif_data.type = "org_apply_rejected";
        notif_data.sender_id = 0;

        auto notif = NotificationMgr::GetInstance()->addNotification(notif_data);
        if (notif) {
            Json::Value wsMsg;
            wsMsg["id"] = notif->getId();
            wsMsg["title"] = title;
            wsMsg["content"] = content;
            wsMsg["type"] = "org_apply_rejected";
            wsMsg["is_read"] = false;
            wsMsg["create_time"] = notif->getCreateTime();
            NotificationMgr::GetInstance()->sendToUser(data.applicant_id, chen::JsonUtil::ToString(wsMsg));
        }
    }
}

static void EventAssignment(const std::any& d)
{
    auto data = std::any_cast<const EventAssignmentData&>(d);

    if (data.type == "deleted") {
        std::string title = "作业已删除";
        std::string content = "作业「" + data.assignment_name + "」已被管理员删除";

        NotificationData notif_data{};
        notif_data.user_id = data.submitter_id;
        notif_data.title = title;
        notif_data.content = content;
        notif_data.type = "assignment_deleted";
        notif_data.sender_id = 0;

        auto notif = NotificationMgr::GetInstance()->addNotification(notif_data);
        if (notif) {
            Json::Value wsMsg;
            wsMsg["id"] = notif->getId();
            wsMsg["title"] = title;
            wsMsg["content"] = content;
            wsMsg["type"] = "assignment_deleted";
            wsMsg["is_read"] = false;
            wsMsg["create_time"] = notif->getCreateTime();
            NotificationMgr::GetInstance()->sendToUser(data.submitter_id, chen::JsonUtil::ToString(wsMsg));
        }
    } else if (data.type == "created") {
        std::string title = "新作业发布";
        std::string content = "组织发布了新作业「" + data.assignment_name + "」（" + data.subject_name + "），请及时完成";
        for (auto uid : data.member_user_ids) {
            NotificationData notif_data{};
            notif_data.user_id = uid;
            notif_data.title = title;
            notif_data.content = content;
            notif_data.type = "assignment_created";
            notif_data.sender_id = 0;
            notif_data.article_id = data.assignment_id;

            auto notif = NotificationMgr::GetInstance()->addNotification(notif_data);
            if (notif) {
                Json::Value wsMsg;
                wsMsg["id"] = notif->getId();
                wsMsg["title"] = title;
                wsMsg["content"] = content;
                wsMsg["type"] = "assignment_created";
                wsMsg["assignment_id"] = data.assignment_id;
                wsMsg["is_read"] = false;
                wsMsg["create_time"] = notif->getCreateTime();
                NotificationMgr::GetInstance()->sendToUser(uid, chen::JsonUtil::ToString(wsMsg));
            }
        }
    }
}

static void EventAssignmentSubmitted(const std::any& d)
{
    auto data = std::any_cast<const EventAssignmentSubmittedData&>(d);

    std::string title = "作业提交通知";
    std::string content = "用户「" + data.submitter_name + "」提交了作业「" + data.assignment_name + "」";
    for (auto admin_id : data.admin_user_ids) {
        NotificationData notif_data{};
        notif_data.user_id = admin_id;
        notif_data.title = title;
        notif_data.content = content;
        notif_data.type = "assignment_submitted";
        notif_data.sender_id = 0;
        notif_data.article_id = data.assignment_id;

        auto notif = NotificationMgr::GetInstance()->addNotification(notif_data);
        if (notif) {
            Json::Value wsMsg;
            wsMsg["id"] = notif->getId();
            wsMsg["title"] = title;
            wsMsg["content"] = content;
            wsMsg["type"] = "assignment_submitted";
            wsMsg["assignment_id"] = data.assignment_id;
            wsMsg["is_read"] = false;
            wsMsg["create_time"] = notif->getCreateTime();
            NotificationMgr::GetInstance()->sendToUser(admin_id, chen::JsonUtil::ToString(wsMsg));
        }
    }
}

static void EventRdAssign(const std::any& d)
{
    auto data = std::any_cast<const EventRdAssignData&>(d);

    std::string title, notif_type;
    if (data.type == "task") {
        title = "你有新的 Task 待处理";
        notif_type = "task_assigned";
    } else if (data.type == "requirement") {
        title = "你有新的 Requirement 待处理";
        notif_type = "requirement_assigned";
    } else if (data.type == "bug") {
        title = "你有新的 Bug 待处理";
        notif_type = "bug_assigned";
    } else {
        return;
    }
    std::string content = title.substr(0, title.find("待处理") - 1) + "「" + data.item_title + "」被分配给了你，请尽快处理";

    NotificationData notif_data{};
    notif_data.user_id = data.assignee_id;
    notif_data.title = title;
    notif_data.content = content;
    notif_data.type = notif_type;
    notif_data.sender_id = data.creator_id;
    notif_data.article_id = data.item_id;

    auto notif_info = NotificationMgr::GetInstance()->addNotification(notif_data);
    if (notif_info) {
        Json::Value wsMsg;
        wsMsg["id"] = notif_info->getId();
        wsMsg["title"] = title;
        wsMsg["content"] = content;
        wsMsg["type"] = notif_type;
        if (data.type == "task") {
            wsMsg["task_id"] = data.item_id;
        } else if (data.type == "requirement") {
            wsMsg["requirement_id"] = data.item_id;
        } else if (data.type == "bug") {
            wsMsg["bug_id"] = data.item_id;
        }
        wsMsg["is_read"] = false;
        wsMsg["create_time"] = notif_info->getCreateTime();
        NotificationMgr::GetInstance()->sendToUser(data.assignee_id, chen::JsonUtil::ToString(wsMsg));
    }
}

static void EventDatabaseBackup(const std::any& d)
{
    auto data = std::any_cast<const EventDatabaseBackupData&>(d);

    if (data.mysql_dbs.empty()) {
        ERROR(logger) << "mysql config not found for backup " << data.backup_id;
        auto info = BackupRecordMgr::GetInstance()->get(data.backup_id);
        if (info) {
            info->setStatus("failed");
            info->setDescription("mysql config not found");
            auto db = GetDB();
            if (db) {
                data::BackupRecordInfoDao::Update(info, db);
            }
        }
        return;
    }
    auto& dbcfg = data.mysql_dbs.begin()->second;
    for (const char* key : {"host", "port", "user", "passwd", "dbname"}) {
        if (dbcfg.find(key) == dbcfg.end()) {
            ERROR(logger) << "mysql config missing key=" << key << " for backup " << data.backup_id;
            auto info = BackupRecordMgr::GetInstance()->get(data.backup_id);
            if (info) {
                info->setStatus("failed");
                info->setDescription(std::string("mysql config missing: ") + key);
                auto db = GetDB();
                if (db) {
                    data::BackupRecordInfoDao::Update(info, db);
                }
            }
            return;
        }
    }
    std::string host = dbcfg.at("host");
    std::string port = dbcfg.at("port");
    std::string user = dbcfg.at("user");
    std::string passwd = dbcfg.at("passwd");
    std::string dbname = dbcfg.at("dbname");

    std::string backup_dir = data.work_path + "/backups";
    chen::FSUtil::Mkdir(backup_dir.c_str());

    std::string filename = data.name + ".sql.gz";
    std::string filepath = backup_dir + "/" + filename;

    int64_t t0 = time(0);
    std::string cmd = "mysqldump -h " + host + " -P " + port + " -u " + user + " -p'" + passwd + "' " + dbname + " 2>/dev/null | gzip > " + filepath;

    INFO(logger) << "Backup " << data.backup_id << " starting, cmd=" << cmd;
    int rc = std::system(cmd.c_str());

    auto info = BackupRecordMgr::GetInstance()->get(data.backup_id);
    if (!info) {
        ERROR(logger) << "Backup record " << data.backup_id << " not found after execution";
        return;
    }

    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Backup " << data.backup_id << " failed: db is null";
        return;
    }

    if (rc != 0) {
        ERROR(logger) << "Backup " << data.backup_id << " failed, rc=" << rc;
        info->setStatus("failed");
        info->setDescription("mysqldump failed with exit code " + std::to_string(rc));
        data::BackupRecordInfoDao::Update(info, db);
        return;
    }

    int64_t fileSize = chen::FSUtil::FileSize(filepath);
    int64_t elapsed = time(0) - t0;

    info->setStatus("completed");
    info->setSize(fileSize);
    info->setFileCount(1);
    info->setFilePath("backups/" + filename);
    info->setCompletedAt(time(0));
    info->setDescription("completed in " + std::to_string(elapsed) + "s, size " + std::to_string(fileSize) + " bytes");
    data::BackupRecordInfoDao::Update(info, db);

    INFO(logger) << "Backup " << data.backup_id << " completed: " << filepath << " (" << fileSize << " bytes, " << elapsed << "s)";
}

bool EventMsgsInit()
{
    auto bus = chen::EventBusMgr::GetInstance();

    // Register event handlers
    bus->on(EVENT_ID_USER_SEND_CODE, EventUserSendCode);
    bus->on(EVENT_ID_COMMENT, EventComment);
    bus->on(EVENT_ID_ARTICLE_REVIEW, EventArticleReview);
    bus->on(EVENT_ID_ARTICLE_STATE_CHANGED, EventArticleStateChanged);
    bus->on(EVENT_ID_ARTICLE_PRAISE, EventArticlePraise);
    bus->on(EVENT_ID_COMMENT_CREATED, EventCommentCreated);
    bus->on(EVENT_ID_COMMENT_PRAISE, EventCommentPraise);
    bus->on(EVENT_ID_USER_FOLLOW, EventUserFollow);
    bus->on(EVENT_ID_USER_ADMIN, EventUserAdminAction);
    bus->on(EVENT_ID_ORG_DELETED, EventOrgDeleted);
    bus->on(EVENT_ID_ORG_MEMBER, EventOrgMember);
    bus->on(EVENT_ID_ORG_APPLY, EventOrgApply);
    bus->on(EVENT_ID_ASSIGNMENT, EventAssignment);
    bus->on(EVENT_ID_ASSIGNMENT_SUBMITTED, EventAssignmentSubmitted);
    bus->on(EVENT_ID_RD_ASSIGN, EventRdAssign);
    bus->on(EVENT_ID_DATABASE_BACKUP, EventDatabaseBackup);

    return true;
};

} // namespace blog
