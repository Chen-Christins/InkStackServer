#include "platform/servlets/database/ExportDeleteServlet.hpp"

#include "platform/manager/ExportRecordManager.hpp"
#include "user/manager/UserManager.hpp"

namespace blog {
namespace servlet {

ExportDeleteServlet::ExportDeleteServlet()
    : BlogLoginedServlet("ExportDeleteServlet")
{
}

int32_t ExportDeleteServlet::handle(
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
        if (current_user->getRole() != UserManager::Role::ADMIN) {
            result->setErrno(errcode::ACCESS_DENIED);
            break;
        }

        std::string id_str = request->getParam("id");
        if (id_str.empty()) {
            result->setErrno(errcode::PARAM_MISSING, "param id is required");
            break;
        }
        int64_t id = chen::TypeUtil::Atoi(id_str);
        if (!id) {
            result->setErrno(errcode::PARAM_INVALID, "invalid id");
            break;
        }

        auto info = ExportRecordMgr::GetInstance()->get(id);
        if (!info) {
            result->setErrno(errcode::DB_OPERATION_FAILED, "export record not found");
            break;
        }

        if (!ExportRecordMgr::GetInstance()->remove(id)) {
            result->setErrno(errcode::DB_OPERATION_FAILED, "delete export failed");
            break;
        }

        result->setErrno(errcode::SUCCESS);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
