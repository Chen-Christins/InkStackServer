#include "platform/servlets/message/ConversationReadServlet.hpp"

#include "platform/manager/MessageManager.hpp"
#include "platform/servlets/message/MessageJson.hpp"

namespace blog {
namespace servlet {

ConversationReadServlet::ConversationReadServlet()
    : BlogLoginedServlet("ConversationReadServlet")
{
}

int32_t ConversationReadServlet::handle(
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
        if (CheckChatPermission(result, uid) != errcode::SUCCESS) {
            break;
        }

        std::string id_str = request->getParam("id");
        if (id_str.empty()) {
            result->setErrno(errcode::PARAM_MISSING, "param id is required");
            break;
        }
        int64_t conversation_id = chen::TypeUtil::Atoi(id_str);
        if (!conversation_id) {
            result->setErrno(errcode::PARAM_INVALID, "invalid id");
            break;
        }

        auto conv = MessageMgr::GetInstance()->get(conversation_id);
        if (!conv) {
            result->setErrno(errcode::MESSAGE_CONVERSATION_NOT_FOUND);
            break;
        }
        if (uid != conv->getUserAId() && uid != conv->getUserBId()) {
            result->setErrno(errcode::ACCESS_DENIED);
            break;
        }

        if (!MessageMgr::GetInstance()->markRead(conversation_id, uid)) {
            result->setErrno(errcode::DB_OPERATION_FAILED, "mark read failed");
            break;
        }

        result->setErrno(errcode::SUCCESS);
        result->set("unread", (int32_t)0);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
