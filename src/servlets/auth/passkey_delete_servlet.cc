#include "passkey_delete_servlet.h"

#include "../../manager/passkey_manager.h"

namespace blog {
namespace servlet {

PasskeyDeleteServlet::PasskeyDeleteServlet()
    : BlogLoginedServlet("PasskeyDeleteServlet")
{
}

int32_t PasskeyDeleteServlet::handle(
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

        int64_t id = request->getParamAs<int64_t>("id", 0);
        if (id <= 0) {
            result->setErrno(errcode::PARAM_INVALID);
            break;
        }

        auto passkey = PasskeyMgr::GetInstance()->get(id);
        if (!passkey || passkey->getUserId() != uid) {
            result->setErrno(errcode::PASSKEY_NOT_FOUND);
            break;
        }

        if (PasskeyMgr::GetInstance()->remove(id) != 0) {
            result->setErrno(errcode::DB_OPERATION_FAILED);
            break;
        }

        result->setErrno(errcode::SUCCESS);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
