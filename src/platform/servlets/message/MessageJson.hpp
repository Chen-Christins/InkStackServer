/**
 * @file MessageJson.hpp
 * @brief 私信会话与消息的 JSON 组装工具（供各私信 servlet 共用）
 * @author Christins
 * @date 2026-09-01
 * @copyright Apache 2.0
 */
#pragma once

#include "common/frame/Struct.hpp"

#include <json/json.h>

#include "blog/data/conversation_info.h"
#include "blog/data/conversation_message_info.h"

namespace blog {
namespace servlet {

/**
 * @brief 组装单条消息 JSON（供消息列表/发送接口使用）
 * @param json 输出 JSON 对象
 * @param msg 消息实体
 * @param uid 当前用户 ID（用于计算 fromMe）
 */
void BuildMessageJson(Json::Value& json, data::ConversationMessageInfo::ptr msg, int64_t uid);

/**
 * @brief 组装会话 JSON（供会话列表/创建会话接口使用）
 * @param json 输出 JSON 对象
 * @param conv 会话实体
 * @param uid 当前用户 ID（用于计算 name/online/unread/fromMe）
 */
void BuildConversationJson(Json::Value& json, data::ConversationInfo::ptr conv, int64_t uid);

/**
 * @brief 校验私信使用权限（仅非普通用户可用，防止滥用打爆数据库）
 * @param result 用于写入错误码
 * @param uid 当前用户 ID
 * @return errcode::SUCCESS 有权限；否则为错误码
 */
int32_t CheckChatPermission(Result::ptr result, int64_t uid);

} // namespace servlet
} // namespace blog
