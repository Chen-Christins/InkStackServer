#include "conversation_servlet.h"

#include <chen/http/http.h>

#include "../../manager/message_manager.h"
#include "../../manager/user_manager.h"
#include "message_json.h"

namespace blog {
namespace servlet {

ConversationServlet::ConversationServlet()
    : BlogLoginedServlet("ConversationServlet")
{
}

int32_t ConversationServlet::handle(
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

        if (request->getMethod() == chen::http::HttpMethod::GET) {
            // 会话列表
            std::vector<data::ConversationInfo::ptr> convs;
            MessageMgr::GetInstance()->listConversations(convs, uid);

            Json::Value arr(Json::arrayValue);
            for (auto& c : convs) {
                Json::Value item;
                BuildConversationJson(item, c, uid);
                arr.append(item);
            }

            result->setErrno(errcode::SUCCESS);
            result->set("list", arr);
            break;
        }

        if (request->getMethod() == chen::http::HttpMethod::POST) {
            // 创建/获取会话
            std::string peer_str = request->getParam("peer_id");
            if (peer_str.empty()) {
                result->setErrno(errcode::PARAM_MISSING, "param peer_id is required");
                break;
            }
            int64_t peer_id = chen::TypeUtil::Atoi(peer_str);
            if (!peer_id) {
                result->setErrno(errcode::PARAM_INVALID, "invalid peer_id");
                break;
            }
            if (peer_id == uid) {
                result->setErrno(errcode::MESSAGE_CANNOT_SELF);
                break;
            }

            auto peer = UserMgr::GetInstance()->get(peer_id);
            if (!peer) {
                result->setErrno(errcode::MESSAGE_PEER_NOT_FOUND);
                break;
            }

            auto conv = MessageMgr::GetInstance()->getOrCreate(uid, peer_id);
            if (!conv) {
                result->setErrno(errcode::MESSAGE_CONVERSATION_NOT_FOUND);
                break;
            }

            Json::Value data;
            BuildConversationJson(data, conv, uid);
            result->set("data", data);

            result->setErrno(errcode::SUCCESS);
            break;
        }

        result->setErrno(errcode::INVALID_METHOD);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
