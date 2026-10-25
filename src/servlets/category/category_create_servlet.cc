#include "category_create_servlet.h"

#include "../../manager/category_manager.h"
#include "../../util.h"

#include <chen/log/log.h>

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

CategoryCreateServlet::CategoryCreateServlet()
    : BlogAdminServlet("CategoryCreateServlet")
{
}

int32_t CategoryCreateServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_STRING(result, name, "name");
        DEFINE_AND_CHECK_STRING(result, url, "url");
        DEFINE_AND_CHECK_STRING(result, icon, "icon");
        DEFINE_AND_CHECK_STRING(result, color, "color");
        std::string desc = request->getParamAs<std::string>("desc");
        int64_t parent_id = request->getParamAs<int64_t>("parent_id");
        int32_t status = request->getParamAs<int32_t>("status", 1);
        int64_t cid = request->getParamAs<int64_t>("id", 0);

        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(errcode::NOT_LOGIN);
            break;
        }

        data::CategoryInfo::ptr parent_info;
        if (parent_id) {
            parent_info = CategoryMgr::GetInstance()->get(parent_id);
            if (!parent_info) {
                result->setErrno(errcode::PARAM_INVALID, "invalid parent_id");
                break;
            }
        }

        data::CategoryInfo::ptr info;
        if (cid) {
            info = CategoryMgr::GetInstance()->get(cid);
            if (!info) {
                result->setErrno(errcode::PARAM_INVALID, "invalid cid");
                break;
            }
            info->setName(name);
        } else {
            info = CategoryMgr::GetInstance()->getByName(name);
            if (!info) {
                info.reset(new data::CategoryInfo);
                info->setName(name);
                info->setCreateTime(time(0));
            } else if (info->getIsDeleted()) {
                info->setIsDeleted(0);
            }
        }
        info->setColor(color);
        info->setParentId(parent_id);
        info->setIsDeleted(0);
        info->setStatus(status);
        info->setUrl(url);
        info->setIcon(icon);
        info->setDescription(desc);
        info->setUpdateTime(time(0));

        auto db = getDB();
        if (!db) {
            result->setErrno(errcode::DB_OPERATION_FAILED);
            break;
        }

        if (data::CategoryInfoDao::InsertOrUpdate(info, db)) {
            result->setErrno(errcode::DB_OPERATION_FAILED, "category operation failed");
            ERROR(logger) << "db error, errno=" << db->getErrno() << " errstr=" << db->getErrStr();
            break;
        }

        CategoryMgr::GetInstance()->add(info);

        result->set("id", info->getId());
        result->set("name", info->getName());
        result->set("color", info->getColor());
        result->set("parent_id", parent_id);
        result->set("url", info->getUrl());
        result->set("icon", info->getIcon());
        result->set("desc", info->getDescription());
        result->set("status", info->getStatus());
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
