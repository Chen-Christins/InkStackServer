#include "category_create_servlet.h"

#include "../../manager/category_manager.h"
#include "../../manager/user_manager.h"
#include "../../permission.h"
#include "../../util.h"

#include <chen/log/log.h>

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

CategoryCreateServlet::CategoryCreateServlet()
    : BlogLoginedServlet("CategoryCreateServlet")
{
}

bool CategoryCreateServlet::handlePre(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    if (!BlogLoginedServlet::handlePre(request, response, session, result)) {
        return false;
    }
    if (!isAdminRoute()) {
        return true;
    }
    int64_t uid = getUserId(request);
    data::UserInfo::ptr user;
    if (uid) {
        user = UserMgr::GetInstance()->get(uid);
    }
    int32_t rt = permission::CheckAdminRoute(uid, user);
    if (rt) {
        result->setErrno(rt);
        return false;
    }
    return true;
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
        int64_t req_uid = request->getParamAs<int64_t>("user_id", 0);

        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(errcode::NOT_LOGIN);
            break;
        }
        bool admin_route = isAdminRoute();

        data::CategoryInfo::ptr info;
        int64_t owner_id = uid;
        if (cid) {
            info = CategoryMgr::GetInstance()->get(cid);
            if (!info) {
                result->setErrno(errcode::PARAM_INVALID, "invalid cid");
                break;
            }
            if (!admin_route && info->getUserId() != uid) {
                result->setErrno(errcode::ACCESS_DENIED);
                break;
            }
            owner_id = info->getUserId();
        } else {
            if (req_uid && req_uid != uid) {
                if (!admin_route) {
                    result->setErrno(errcode::ACCESS_DENIED);
                    break;
                }
                owner_id = req_uid;
            }
            auto owner = UserMgr::GetInstance()->get(owner_id);
            if (!owner) {
                result->setErrno(errcode::USER_NOT_FOUND);
                break;
            }
        }

        if (parent_id) {
            auto parent_info = CategoryMgr::GetInstance()->get(parent_id);
            if (!parent_info || parent_info->getUserId() != owner_id) {
                result->setErrno(errcode::PARAM_INVALID, "invalid parent_id");
                break;
            }
            if (parent_id == cid) {
                result->setErrno(errcode::PARAM_INVALID, "invalid parent_id");
                break;
            }
        }

        if (!info) {
            info = CategoryMgr::GetInstance()->getByUserIdName(owner_id, name);
            if (!info) {
                info.reset(new data::CategoryInfo);
                info->setUserId(owner_id);
                info->setName(name);
                info->setCreateTime(time(0));
            } else if (info->getIsDeleted()) {
                info->setIsDeleted(0);
            }
        } else if (info->getName() != name) {
            auto same = CategoryMgr::GetInstance()->getByUserIdName(owner_id, name);
            if (same && same->getId() != info->getId()) {
                result->setErrno(errcode::PARAM_INVALID, "category name exists");
                break;
            }
            info->setName(name);
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
        result->set("user_id", info->getUserId());
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
