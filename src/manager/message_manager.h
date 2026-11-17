/**
 * @file message_manager.h
 * @brief 私信会话与消息管理（会话、消息、未读数、已读标记）
 * @author Christins
 * @date 2026-09-01
 * @copyright Apache 2.0
 */
#pragma once

#include <chen/util/singleton.h>

#include "blog/data/conversation_info.h"
#include "blog/data/conversation_message_info.h"

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace blog {

class MessageManager {
public:
    /// 单条消息最大长度
    static const size_t kMaxMessageLen = 2000;

    /// 会话相关
    data::ConversationInfo::ptr get(int64_t id);

    data::ConversationInfo::ptr getByUsers(int64_t uid_a, int64_t uid_b);

    data::ConversationInfo::ptr getOrCreate(int64_t uid_a, int64_t uid_b);

    void listConversations(std::vector<data::ConversationInfo::ptr>& results, int64_t uid);

    /// 消息相关
    void listMessages(std::vector<data::ConversationMessageInfo::ptr>& results, int64_t conversation_id, int32_t offset, int32_t limit);

    data::ConversationMessageInfo::ptr sendMessage(int64_t conversation_id, int64_t sender_id, const std::string& text);

    bool markRead(int64_t conversation_id, int64_t uid);

    /// 在本端隐藏会话（不影响对方与消息数据）
    bool deleteForUser(int64_t conversation_id, int64_t uid);

    /// 聊天 WS 推送（连接统一由 WSMgr 管理）
    bool isChatConnected(int64_t user_id);

    int32_t sendToUser(int64_t user_id, const std::string& message);

    /// 参与者归一化：user_a_id 恒为较小值
    static int64_t NormalizeUserA(int64_t uid_a, int64_t uid_b) { return uid_a < uid_b ? uid_a : uid_b; }

    static int64_t NormalizeUserB(int64_t uid_a, int64_t uid_b) { return uid_a < uid_b ? uid_b : uid_a; }

private:
    /// 会话状态读改写锁（未读数、隐藏标志），避免 Redis 反序列化后并发丢更新
    std::mutex m_convMutex;
};

typedef chen::Singleton<MessageManager> MessageMgr;

} // namespace blog
