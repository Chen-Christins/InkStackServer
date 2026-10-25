#include "notify_servlet.h"

#include "../../util.h"

#include "../../auth/auth_service.h"
#include "../../error_codes.h"
#include "../../manager/notification_manager.h"

#include <chen/log/log.h>
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

NotifyServlet::NotifyServlet()
    : chen::http::WSServlet("Notify")
{
}

int32_t NotifyServlet::onConnect(chen::http::HttpRequest::ptr header, chen::http::WSSession::ptr session)
{
    if (!header || !session) {
        return -1;
    }

    int64_t uid = 0;
    data::UserInfo::ptr uinfo;
    if (!AuthService::ResolveWs(header, uid, uinfo)) {
        INFO(logger) << "[WS] onConnect FAIL: not logged in";
        sendError(session, errcode::NOT_LOGIN, "Please login first");
        return -1;
    }

    NotificationMgr::GetInstance()->addConnection(uid, session);
    INFO(logger) << "[WS] onConnect OK: uid=" << uid;

    return 0;
}

int32_t NotifyServlet::onClose(chen::http::HttpRequest::ptr header, chen::http::WSSession::ptr session)
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
        NotificationMgr::GetInstance()->removeConnection(uid);
    }

    return 0;
}

int32_t NotifyServlet::handle(chen::http::HttpRequest::ptr header, chen::http::WSFrameMessage::ptr msg, chen::http::WSSession::ptr session)
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

    INFO(logger) << "[WS] handle: opcode=" << msg->getOpcode() << " data=" << msg->getData();

    return 0;
}

} // namespace blog::servlet
