#include "platform/servlets/resource/ResourceListServlet.hpp"

#include <chen/log/log.h>

#include "../../../common/util/Util.hpp"
#include "user/manager/UserManager.hpp"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

ResourceListServlet::ResourceListServlet()
    : BlogLoginedServlet("ResourceListServlet")
{
}

int32_t ResourceListServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_STRING(result, type, "type");
        DEFINE_AND_CHECK_STRING(result, folder, "folder");

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
        int32_t system_role = current_user->getRole();
        if (!checkPermession(system_role)) {
            result->setErrno(errcode::ACCESS_DENIED);
            break;
        }

        // TODO: 查询资源列表
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

bool ResourceListServlet::checkPermession(int32_t system_role)
{
    if (system_role != UserManager::Role::ADMIN) {
        return false;
    }
    return true;
}

} // namespace servlet
} // namespace blog
