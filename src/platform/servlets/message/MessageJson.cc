#include "platform/servlets/message/MessageJson.hpp"

#include "platform/manager/MessageManager.hpp"
#include "user/manager/UserManager.hpp"

namespace blog {
namespace servlet {

void BuildMessageJson(Json::Value& json, data::ConversationMessageInfo::ptr msg, int64_t uid)
{
    json["id"] = msg->getId();
    json["fromMe"] = (msg->getSenderId() == uid);
    json["text"] = msg->getContent();
    json["create_time"] = msg->getCreateTime();
}

void BuildConversationJson(Json::Value& json, data::ConversationInfo::ptr conv, int64_t uid)
{
    int64_t peer_id = (conv->getUserAId() == uid) ? conv->getUserBId() : conv->getUserAId();
    int32_t unread = (conv->getUserAId() == uid) ? conv->getUnreadA() : conv->getUnreadB();

    json["id"] = conv->getId();
    json["peer_id"] = peer_id;
    json["online"] = MessageMgr::GetInstance()->isChatConnected(peer_id);

    auto user = UserMgr::GetInstance()->get(peer_id);
    json["name"] = user ? user->getName() : "";

    json["last_time"] = conv->getLastMessageTime();
    json["unread"] = unread;

    Json::Value arr(Json::arrayValue);
    if (conv->getLastMessageId() > 0) {
        Json::Value last;
        last["id"] = conv->getLastMessageId();
        last["fromMe"] = (conv->getLastSenderId() == uid);
        last["text"] = conv->getLastMessage();
        last["create_time"] = conv->getLastMessageTime();
        arr.append(last);
    }
    json["messages"] = arr;
}

int32_t CheckChatPermission(Result::ptr result, int64_t uid)
{
    auto user = UserMgr::GetInstance()->get(uid);
    if (!user) {
        result->setErrno(errcode::USER_NOT_FOUND);
        return errcode::USER_NOT_FOUND;
    }
    if (user->getRole() == UserManager::Role::USER) {
        result->setErrno(errcode::ACCESS_DENIED, "私信仅对内部成员开放");
        return errcode::ACCESS_DENIED;
    }
    return errcode::SUCCESS;
}

} // namespace servlet
} // namespace blog
