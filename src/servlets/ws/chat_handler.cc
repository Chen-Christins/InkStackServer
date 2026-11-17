#include "chat_handler.h"

#include "../../error_codes.h"
#include "../../manager/message_manager.h"
#include "../../manager/user_manager.h"
#include "../../manager/ws_connection_manager.h"
#include "../message/message_json.h"

#include <chen/log/log.h>
#include <chen/util/json_util.h>

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

static void SendChatError(chen::http::WSSession::ptr session, int32_t errno_, const std::string& errstr, const std::string& client_id = "")
{
    if (!session || !session->isConnected()) {
        return;
    }

    Json::Value err;
    err["channel"] = "chat";
    err["type"] = "send_error";
    if (!client_id.empty()) {
        err["client_id"] = client_id;
    }
    err["errno"] = errno_;
    err["errstr"] = errstr;

    std::string message = chen::JsonUtil::ToString(err);
    if (WSMgr::GetInstance()->sendToSession(session, message) != 0) {
        session->sendMessage(message);
    }
}

static bool CheckConversationAccess(
    chen::http::WSSession::ptr session,
    int64_t uid,
    int64_t conversation_id,
    data::ConversationInfo::ptr& conv,
    int64_t& peer_id,
    const std::string& client_id = "")
{
    conv = MessageMgr::GetInstance()->get(conversation_id);
    if (!conv) {
        SendChatError(session, errcode::MESSAGE_CONVERSATION_NOT_FOUND, "Conversation not found", client_id);
        return false;
    }
    if (uid != conv->getUserAId() && uid != conv->getUserBId()) {
        SendChatError(session, errcode::ACCESS_DENIED, "Access denied", client_id);
        return false;
    }
    peer_id = conv->getUserAId() == uid ? conv->getUserBId() : conv->getUserAId();
    return true;
}

static int32_t HandleSend(int64_t uid, const Json::Value& root, chen::http::WSSession::ptr session)
{
    std::string client_id = root.get("client_id", "").asString();
    int64_t conversation_id = root.get("conversation_id", 0).asInt64();
    std::string text = root.get("text", "").asString();

    if (conversation_id <= 0 || text.empty()) {
        SendChatError(session, errcode::PARAM_MISSING, "conversation_id and text required", client_id);
        return 0;
    }
    if (text.size() > MessageManager::kMaxMessageLen) {
        SendChatError(session, errcode::MESSAGE_TEXT_TOO_LONG, "Message too long", client_id);
        return 0;
    }

    data::ConversationInfo::ptr conv;
    int64_t peer_id = 0;
    if (!CheckConversationAccess(session, uid, conversation_id, conv, peer_id, client_id)) {
        return 0;
    }

    auto msg_info = MessageMgr::GetInstance()->sendMessage(conversation_id, uid, text);
    if (!msg_info) {
        SendChatError(session, errcode::MESSAGE_SEND_FAILED, "Send failed", client_id);
        return 0;
    }

    INFO(logger) << "[ChatWS] send OK: uid=" << uid << " conv=" << conversation_id << " msg_id=" << msg_info->getId() << " client_id=" << client_id;

    // ACK 只回发起设备，client_id 是单标签页的
    {
        Json::Value ack;
        ack["channel"] = "chat";
        ack["type"] = "message_ack";
        ack["client_id"] = client_id;
        ack["conversation_id"] = conversation_id;
        Json::Value mj;
        BuildMessageJson(mj, msg_info, uid);
        ack["message"] = mj;
        ack["recipient_online"] = MessageMgr::GetInstance()->isChatConnected(peer_id);
        WSMgr::GetInstance()->sendToSession(session, chen::JsonUtil::ToString(ack));
    }

    // 推送给接收方所有设备
    if (MessageMgr::GetInstance()->isChatConnected(peer_id)) {
        Json::Value push;
        push["type"] = "message";
        push["conversation_id"] = conversation_id;
        Json::Value mj;
        BuildMessageJson(mj, msg_info, peer_id);
        push["message"] = mj;
        MessageMgr::GetInstance()->sendToUser(peer_id, chen::JsonUtil::ToString(push));
    }

    INFO(logger) << "[ChatWS] ack->" << uid << " push->" << peer_id << " peer_online=" << MessageMgr::GetInstance()->isChatConnected(peer_id);
    return 0;
}

static int32_t HandleRead(int64_t uid, const Json::Value& root, chen::http::WSSession::ptr session)
{
    int64_t conversation_id = root.get("conversation_id", 0).asInt64();
    if (conversation_id <= 0) {
        SendChatError(session, errcode::PARAM_MISSING, "conversation_id required");
        return 0;
    }

    data::ConversationInfo::ptr conv;
    int64_t peer_id = 0;
    if (!CheckConversationAccess(session, uid, conversation_id, conv, peer_id)) {
        return 0;
    }

    MessageMgr::GetInstance()->markRead(conversation_id, uid);

    // 已读回执给对端所有设备
    if (MessageMgr::GetInstance()->isChatConnected(peer_id)) {
        Json::Value rcpt;
        rcpt["type"] = "read";
        rcpt["conversation_id"] = conversation_id;
        rcpt["unread"] = 0;
        MessageMgr::GetInstance()->sendToUser(peer_id, chen::JsonUtil::ToString(rcpt));
    }
    return 0;
}

int32_t ChatHandler::Handle(int64_t uid, const Json::Value& root, chen::http::WSSession::ptr session)
{
    if (uid <= 0 || !session) {
        return -1;
    }

    std::string type = root.get("type", "").asString();
    if (type != "send" && type != "read") {
        return -1;
    }

    // 兜底：连接鉴权之外的二次校验，防止角色变动后仍可用私信
    auto user = UserMgr::GetInstance()->get(uid);
    if (!user || user->getRole() == UserManager::Role::USER) {
        SendChatError(session, errcode::ACCESS_DENIED, "Chat not available for normal users", root.get("client_id", "").asString());
        return 0;
    }

    if (type == "send") {
        return HandleSend(uid, root, session);
    }
    return HandleRead(uid, root, session);
}

} // namespace servlet
} // namespace blog
