#include "conversation_message_servlet.h"

#include <chen/http/http.h>
#include <json/json.h>

#include "../../manager/message_manager.h"
#include "message_json.h"

namespace blog {
namespace servlet {

ConversationMessageServlet::ConversationMessageServlet()
    : BlogLoginedServlet("ConversationMessageServlet")
{
}

int32_t ConversationMessageServlet::handle(
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
        int64_t peer_id = (conv->getUserAId() == uid) ? conv->getUserBId() : conv->getUserAId();

        if (request->getMethod() == chen::http::HttpMethod::GET) {
            // 会话消息记录
            int32_t offset = request->getParamAs<int32_t>("offset", 0);
            int32_t size = request->getParamAs<int32_t>("size", 50);

            std::vector<data::ConversationMessageInfo::ptr> msgs;
            MessageMgr::GetInstance()->listMessages(msgs, conversation_id, offset, size);

            Json::Value arr(Json::arrayValue);
            for (auto& m : msgs) {
                Json::Value item;
                BuildMessageJson(item, m, uid);
                arr.append(item);
            }

            result->setErrno(errcode::SUCCESS);
            result->set("list", arr);
            break;
        }

        if (request->getMethod() == chen::http::HttpMethod::POST) {
            // 发送消息
            std::string text = request->getParam("text");
            if (text.empty()) {
                result->setErrno(errcode::PARAM_MISSING, "param text is required");
                break;
            }
            if (text.size() > MessageManager::kMaxMessageLen) {
                result->setErrno(errcode::MESSAGE_TEXT_TOO_LONG);
                break;
            }

            auto msg = MessageMgr::GetInstance()->sendMessage(conversation_id, uid, text);
            if (!msg) {
                result->setErrno(errcode::MESSAGE_SEND_FAILED);
                break;
            }

            Json::Value data;
            BuildMessageJson(data, msg, uid);
            result->set("data", data);

            result->setErrno(errcode::SUCCESS);

            // HTTP 兜底发送也走聊天 WS 推送给接收方
            Json::Value frame;
            frame["type"] = "message";
            frame["conversation_id"] = conversation_id;
            Json::Value msg_json;
            BuildMessageJson(msg_json, msg, peer_id);
            frame["message"] = msg_json;
            MessageMgr::GetInstance()->sendToUser(peer_id, chen::JsonUtil::ToString(frame));
            break;
        }

        result->setErrno(errcode::INVALID_METHOD);
    } while (0);

    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
