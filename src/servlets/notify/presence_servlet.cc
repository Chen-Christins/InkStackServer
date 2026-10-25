#include "presence_servlet.h"

#include "../../auth/auth_service.h"
#include "../../error_codes.h"
#include "../../manager/notification_manager.h"

#include <chen/log/log.h>
#include <chen/util/json_util.h>
#include <chen/util/type_util.h>
#include <json/json.h>

namespace blog::servlet {

static chen::Logger::ptr logger = LOG_ROOT();

static void sendError(chen::http::WSSession::ptr session, int32_t errno_, const std::string& errstr)
{
    if (!session) {
        return;
    }
    Json::Value err;
    err["errno"] = errno_;
    err["errstr"] = errstr;
    session->sendMessage(chen::JsonUtil::ToString(err));
}

PresenceServlet::PresenceServlet()
    : chen::http::WSServlet("Presence")
{
}

static void broadcastOnlineCount()
{
    int32_t count = NotificationMgr::GetInstance()->getPresenceOnlineCount();
    Json::Value msg;
    msg["type"] = "online_count";
    msg["count"] = count;
    NotificationMgr::GetInstance()->broadcastPresence(chen::JsonUtil::ToString(msg));
}

int32_t PresenceServlet::onConnect(chen::http::HttpRequest::ptr header, chen::http::WSSession::ptr session)
{
    if (!header || !session) {
        return -1;
    }

    int64_t uid = 0;
    data::UserInfo::ptr uinfo;
    if (!AuthService::ResolveWs(header, uid, uinfo)) {
        INFO(logger) << "[Presence] onConnect FAIL: not logged in";
        sendError(session, errcode::NOT_LOGIN, "Please login first");
        return -1;
    }

    NotificationMgr::GetInstance()->addPresenceConnection(uid, session);
    INFO(logger) << "[Presence] onConnect OK: uid=" << uid;

    broadcastOnlineCount();
    return 0;
}

int32_t PresenceServlet::onClose(chen::http::HttpRequest::ptr header, chen::http::WSSession::ptr session)
{
    int64_t uid = 0;
    if (header) {
        AuthContext ctx = AuthService::Resolve(header, nullptr, false);
        if (ctx.valid()) {
            uid = ctx.uid;
        } else {
            std::string uid_str = header->getParam("uid");
            if (!uid_str.empty()) {
                uid = chen::TypeUtil::Atoi(uid_str);
            }
        }
    }
    if (uid) {
        NotificationMgr::GetInstance()->removePresenceConnection(uid);
        broadcastOnlineCount();
    }

    return 0;
}

int32_t PresenceServlet::handle(chen::http::HttpRequest::ptr header, chen::http::WSFrameMessage::ptr msg, chen::http::WSSession::ptr session)
{
    if (!header || !msg || !session) {
        return -1;
    }
    // 心跳处理
    Json::Value root;
    if (chen::JsonUtil::FromString(root, msg->getData()) && root.isObject()) {
        if (root["type"].asString() == "ping") {
            if (session && session->isConnected()) {
                session->sendMessage("{\"type\":\"pong\"}");
            }
            return 0;
        }
    }
    return 0;
}

} // namespace blog::servlet
