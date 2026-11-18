#include "article/servlets/label/LabelCreateServlet.hpp"

#include "../../../common/util/Util.hpp"
#include "article/manager/LabelManager.hpp"

#include <chen/log/log.h>

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

LabelCreateServlet::LabelCreateServlet()
    : BlogLoginedServlet("LabelCreateServlet")
{
}

int32_t LabelCreateServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_STRING(result, name, "name");
        DEFINE_AND_CHECK_STRING(result, color, "color");
        std::string desc = request->getParamAs<std::string>("desc");
        int64_t lid = request->getParamAs<int64_t>("id", 0);

        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(errcode::NOT_LOGIN);
            break;
        }

        data::LabelInfo::ptr info;
        if (lid) {
            info = LabelMgr::GetInstance()->get(lid);
            if (!info) {
                result->setErrno(errcode::PARAM_INVALID, "invalid lid");
                break;
            }
            if (info->getUserId() != uid) {
                result->setErrno(errcode::ACCESS_DENIED);
                break;
            }
            info->setName(name);
        } else {
            info = LabelMgr::GetInstance()->getByUserIdName(uid, name);
            if (!info) {
                info.reset(new data::LabelInfo);
                info->setUserId(uid);
                info->setName(name);
                info->setCreateTime(time(0));
            } else if (info->getIsDeleted()) {
                info->setIsDeleted(0);
            }
        }
        info->setDescription(desc);
        info->setColor(color);
        info->setIsDeleted(0);
        info->setUpdateTime(time(0));

        auto db = getDB();
        if (!db) {
            result->setErrno(errcode::DB_OPERATION_FAILED);
            break;
        }

        if (data::LabelInfoDao::InsertOrUpdate(info, db)) {
            result->setErrno(errcode::DB_OPERATION_FAILED, "label operation failed");
            ERROR(logger) << "db error, errno=" << db->getErrno() << " errstr=" << db->getErrStr();
            break;
        }

        LabelMgr::GetInstance()->add(info);

        result->set("id", info->getId());
        result->set("name", info->getName());
        result->set("color", info->getColor());
        result->set("desc", info->getDescription());
        result->set("create_time", info->getCreateTime());
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
