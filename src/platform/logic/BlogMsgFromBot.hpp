#pragma once

#include <chen/rpc/rpc_server.h>

#include "protocol_ss_github.h" // IWYU pragma: keep

namespace blog {

void BlogMsgFromBotInit(chen::rpc::RpcServer::ptr server);

} // namespace blog