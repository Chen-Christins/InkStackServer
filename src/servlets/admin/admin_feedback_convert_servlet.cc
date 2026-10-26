#include "admin_feedback_convert_servlet.h"

#include "../../include/managers.h"
#include "../../util.h"

#include <chen/log/log.h>

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

AdminFeedbackConvertServlet::AdminFeedbackConvertServlet()
    : BlogLoginedServlet("AdminFeedbackConvertServlet")
{
}

int32_t AdminFeedbackConvertServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(errcode::NOT_LOGIN);
            break;
        }

        auto current_user = UserMgr::GetInstance()->get(uid);
        if (!current_user) {
            result->setErrno(errcode::USER_NOT_FOUND);
            break;
        }

        int32_t role = current_user->getRole();
        if (role != UserManager::Role::ADMIN) {
            result->setErrno(errcode::ACCESS_DENIED);
            break;
        }

        DEFINE_AND_CHECK_STRING(result, id_str, "id");
        DEFINE_AND_CHECK_STRING(result, target, "target");
        DEFINE_AND_CHECK_STRING(result, title, "title");

        // 校验 target 值
        if (target != "faq" && target != "requirement" && target != "bug") {
            result->setErrno(errcode::PARAM_INVALID, "target must be faq, requirement or bug");
            break;
        }

        int64_t id = 0;
        try {
            id = chen::TypeUtil::Atoi(id_str);
        } catch (...) {
            result->setErrno(errcode::PARAM_INVALID, "invalid id");
            break;
        }

        // 获取反馈
        auto feedback = FeedbackMgr::GetInstance()->get(id);
        if (!feedback) {
            result->setErrno(errcode::FEEDBACK_NOT_FOUND);
            break;
        }

        // content 不传则复用原反馈内容
        std::string content = request->getParam("content");
        if (content.empty()) {
            content = feedback->getContent();
        }

        auto db = getDB();
        if (!db) {
            result->setErrno(errcode::DB_CONNECTION_FAILED);
            break;
        }

        int64_t converted_id = 0;

        if (target == "faq") {
            // 转换为常见问题
            std::string cat = request->getParam("cat");

            data::HelpFaqsInfo::ptr faq(new data::HelpFaqsInfo);
            faq->setCat(cat);
            faq->setQ(title);
            faq->setA(content);
            faq->setSortOrder(0);
            faq->setIsDeleted(0);
            faq->setCreateTime(time(0));
            faq->setUpdateTime(time(0));

            if (data::HelpFaqsInfoDao::Insert(faq, db)) {
                result->setErrno(errcode::DB_OPERATION_FAILED, "insert faq failed");
                ERROR(logger) << "insert faq failed, errno=" << db->getErrno() << " errstr=" << db->getErrStr();
                break;
            }

            converted_id = faq->getId();
        } else if (target == "requirement") {
            // 转换为需求
            std::string org_id_str = request->getParam("org_id");
            int64_t org_id = 0;
            if (!org_id_str.empty()) {
                try {
                    org_id = chen::TypeUtil::Atoi(org_id_str);
                } catch (...) {
                    result->setErrno(errcode::PARAM_INVALID, "invalid orgId");
                    break;
                }
            }

            data::RequirementInfo::ptr req(new data::RequirementInfo);
            req->setOrgId(org_id);
            req->setTitle(title);
            req->setDescription(content);
            req->setPriority(1); // 默认低优先级
            req->setStatus(0);   // 草稿
            req->setCreatorId(uid);
            req->setAssigneeId(0);
            req->setIsDeleted(0);
            req->setCreateTime(time(0));
            req->setUpdateTime(time(0));

            if (data::RequirementInfoDao::Insert(req, db)) {
                result->setErrno(errcode::DB_OPERATION_FAILED, "insert requirement failed");
                ERROR(logger) << "insert requirement failed, errno=" << db->getErrno() << " errstr=" << db->getErrStr();
                break;
            }

            converted_id = req->getId();
        } else if (target == "bug") {
            // 转换为缺陷
            std::string org_id_str = request->getParam("org_id");
            int64_t org_id = 0;
            if (!org_id_str.empty()) {
                try {
                    org_id = chen::TypeUtil::Atoi(org_id_str);
                } catch (...) {
                    result->setErrno(errcode::PARAM_INVALID, "invalid orgId");
                    break;
                }
            }

            data::BugInfo::ptr bug(new data::BugInfo);
            bug->setOrgId(org_id);
            bug->setTitle(title);
            bug->setDescription(content);
            bug->setSeverity(1); // 默认轻微
            bug->setPriority(1); // 默认低优先级
            bug->setStatus(0);   // 待处理
            bug->setCreatorId(uid);
            bug->setAssigneeId(0);
            bug->setIsDeleted(0);
            bug->setCreateTime(time(0));
            bug->setUpdateTime(time(0));

            if (data::BugInfoDao::Insert(bug, db)) {
                result->setErrno(errcode::DB_OPERATION_FAILED, "insert bug failed");
                ERROR(logger) << "insert bug failed, errno=" << db->getErrno() << " errstr=" << db->getErrStr();
                break;
            }

            converted_id = bug->getId();
        }

        // 转换成功后删除原反馈
        if (converted_id > 0) {
            if (!FeedbackMgr::GetInstance()->remove(id)) {
                WARN(logger) << "failed to delete feedback after convert, id=" << id;
            }
        }

        result->set("id", std::to_string(converted_id));
        result->setErrno(errcode::SUCCESS);
    } while (0);

    DEBUG(logger) << "AdminFeedbackConvertServlet handle result: " << result->toJsonString();
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
