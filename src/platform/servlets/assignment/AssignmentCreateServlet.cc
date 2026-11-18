#include "platform/servlets/assignment/AssignmentCreateServlet.hpp"

#include <chen/log/log.h>

#include "../../../common/util/Util.hpp"
#include "platform/manager/AssignmentManager.hpp"
#include "user/manager/UserManager.hpp"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

AssignmentCreateServlet::AssignmentCreateServlet()
    : BlogLoginedServlet("AssignmentCreateServlet")
{
}

int32_t AssignmentCreateServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_STRING(result, name, "name");
        DEFINE_AND_CHECK_STRING(result, subject_name, "subject_name");
        DEFINE_AND_CHECK_TYPE(result, uint64_t, end_time, "end_time");
        DEFINE_AND_CHECK_TYPE(result, int32_t, file_size, "file_size");
        DEFINE_AND_CHECK_TYPE(result, int32_t, priority, "priority");
        DEFINE_AND_CHECK_TYPE(result, int32_t, status, "status");
        DEFINE_AND_CHECK_STRING(result, description, "description");
        DEFINE_AND_CHECK_STRING(result, file_type, "file_type");
        int64_t aid = request->getParamAs<int64_t>("id", 0);

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
        if (current_user->getRole() != UserManager::Role::ADMIN) {
            result->setErrno(errcode::ACCESS_DENIED);
            break;
        }

        if (file_size > 96) {
            result->setErrno(errcode::ASSIGNMENT_SIZE_EXCEED);
            break;
        }

        data::AssignmentInfo::ptr info;
        if (aid) {
            info = AssignmentMgr::GetInstance()->get(aid);
            if (!info) {
                result->setErrno(errcode::ASSIGNMENT_NOT_FOUND);
                break;
            }
            info->setName(name);
            info->setSubjectName(subject_name);
        } else {
            info = AssignmentMgr::GetInstance()->getByName(subject_name, name);
            if (!info) {
                info.reset(new data::AssignmentInfo);
                info->setName(name);
                info->setSubjectName(subject_name);
                info->setCreateTime(time(0));
            } else if (info->getIsDeleted()) {
                info->setCreateTime(time(0));
            }
        }
        info->setDescription(description);
        info->setFileType(file_type);
        info->setDeadline(end_time);
        info->setMaxSize(file_size);
        info->setStatus(status);
        info->setPriority(priority);
        info->setIsDeleted(0);
        info->setUpdateTime(time(0));

        auto db = getDB();
        if (!db) {
            result->setErrno(errcode::DB_OPERATION_FAILED);
            break;
        }

        if (data::AssignmentInfoDao::InsertOrUpdate(info, db)) {
            result->setErrno(errcode::ASSIGNMENT_UPDATE_FAILED);
            ERROR(logger) << "db error, errno=" << db->getErrno() << " errstr=" << db->getErrStr();
            break;
        }

        AssignmentMgr::GetInstance()->add(info);

        result->set("id", info->getId());
        result->set("name", info->getName());
        result->set("subject_name", info->getSubjectName());
        result->set("status", info->getStatus());
        result->set("priority", info->getPriority());
        result->set("end_time", info->getDeadline());
        result->set("create_time", info->getCreateTime());
        result->set("file_size", info->getMaxSize());
        result->set("file_type", info->getFileType());
        result->set("description", info->getDescription());
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
