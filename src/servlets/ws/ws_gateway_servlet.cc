#include "ws_gateway_servlet.h"

#include "../../auth/auth_service.h"
#include "../../auth/session_store.h"
#include "../../error_codes.h"
#include "../../manager/ws_connection_manager.h"
#include "../../struct.h"
#include "chat_handler.h"

#include <chen/log/log.h>
#include <chen/util/json_util.h>

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

static void SendFrame(chen::http::WSSession::ptr session, const std::string& message)
{
    if (!session || !session->isConnected()) {
        return;
    }
    if (WSMgr::GetInstance()->sendToSession(session, message) != 0) {
        session->sendMessage(message);
    }
}

static void SendError(chen::http::WSSession::ptr session, int32_t errno_, const std::string& errstr)
{
    if (!session) {
        return;
    }

    Json::Value err;
    err["channel"] = "system";
    err["type"] = "error";
    err["errno"] = errno_;
    err["errstr"] = errstr;
    SendFrame(session, chen::JsonUtil::ToString(err));
}

WSGatewayServlet::WSGatewayServlet()
    : WSServlet("WSGateway")
{
}

int32_t WSGatewayServlet::onConnect(chen::http::HttpRequest::ptr header, chen::http::WSSession::ptr session)
{
    if (!header || !session) {
        return -1;
    }

    std::string session_id = header->getCookie(CookieKey::SESSION_KEY);
    if (session_id.empty()) {
        INFO(logger) << "[WS] onConnect FAIL: no session cookie";
        SendError(session, errcode::NOT_LOGIN, "Please login first");
        return -1;
    }

    int64_t uid = 0;
    data::UserInfo::ptr user;
    if (!AuthService::ResolveWs(header, uid, user) || !user) {
        INFO(logger) << "[WS] onConnect FAIL: not logged in";
        SendError(session, errcode::NOT_LOGIN, "Please login first");
        return -1;
    }

    std::string device_id = SessionStore::HashId(session_id);
    if (device_id.empty()) {
        ERROR(logger) << "[WS] onConnect FAIL: empty device id uid=" << uid;
        SendError(session, errcode::INTERNAL_ERROR, "Invalid session");
        return -1;
    }

    auto result = WSMgr::GetInstance()->addConnection(uid, device_id, session, false);
    if (result.replaced_session && result.replaced_session != session) {
        Json::Value replaced;
        replaced["channel"] = "system";
        replaced["type"] = "replaced";
        replaced["reason"] = "connected_elsewhere";
        WSMgr::GetInstance()->closeSession(result.replaced_session, chen::JsonUtil::ToString(replaced));
        // 旧连接 onClose 不一定会立刻触发，这里主动清理旧 session 映射
        WSMgr::GetInstance()->removeConnection(result.replaced_session);
        INFO(logger) << "[WS] replace old connection: uid=" << uid << " device=" << device_id.substr(0, 12);
    }

    Json::Value connected;
    connected["channel"] = "system";
    connected["type"] = "connected";
    connected["uid"] = uid;
    connected["device_id"] = device_id;
    connected["online_users"] = WSMgr::GetInstance()->onlineUserCount();
    SendFrame(session, chen::JsonUtil::ToString(connected));

    if (result.presence_changed) {
        WSMgr::GetInstance()->broadcastPresence();
    }

    INFO(logger) << "[WS] onConnect OK: uid=" << uid << " device=" << device_id.substr(0, 12);
    return 0;
}

int32_t WSGatewayServlet::onClose(chen::http::HttpRequest::ptr, chen::http::WSSession::ptr session)
{
    if (!session) {
        return 0;
    }

    auto result = WSMgr::GetInstance()->removeConnection(session);
    if (result.removed) {
        INFO(logger) << "[WS] onClose: uid=" << result.user_id;
    }
    return 0;
}

int32_t WSGatewayServlet::handle(chen::http::HttpRequest::ptr header, chen::http::WSFrameMessage::ptr msg, chen::http::WSSession::ptr session)
{
    if (!header || !msg || !session) {
        return -1;
    }

    int64_t uid = 0;
    std::string device_id;
    if (!WSMgr::GetInstance()->getIdentity(session, uid, device_id)) {
        SendError(session, errcode::NOT_LOGIN, "Not logged in");
        return -1;
    }

    Json::Value root;
    if (!chen::JsonUtil::FromString(root, msg->getData()) || !root.isObject()) {
        SendError(session, errcode::PARAM_INVALID, "Invalid frame");
        return 0;
    }

    std::string type = root.get("type", "").asString();
    if (type == "ping") {
        Json::Value rsp;
        rsp["channel"] = "system";
        rsp["type"] = "pong";
        SendFrame(session, chen::JsonUtil::ToString(rsp));
        return 0;
    }

    if (type == "send" || type == "read") {
        if (ChatHandler::Handle(uid, root, session) != 0) {
            SendError(session, errcode::PARAM_INVALID, "Unsupported frame type");
        }
        return 0;
    }

    SendError(session, errcode::PARAM_INVALID, "Unsupported frame type");
    return 0;
}

} // namespace servlet
} // namespace blog
