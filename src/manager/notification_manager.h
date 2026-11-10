#pragma once

#include <chen/db/query_builder.h>
#include <chen/util/singleton.h>

#include <cstdint>
#include <string>
#include <vector>

#include "blog/data/notification_info.h"

namespace blog {

struct NotificationData {
    int64_t user_id = 0;
    std::string title;
    std::string content;
    std::string type;
    int64_t sender_id = 0;
    int64_t article_id = 0;
    int64_t comment_id = 0;
    int32_t is_broadcast = 0;
    std::string level = "";
    int64_t start_time = 0;
    int64_t end_time = 0;
};

class NotificationManager {
public:
    // WS 推送（连接统一由 WSMgr 管理）
    int32_t sendToUser(int64_t user_id, const std::string& message);

    void broadcast(const std::string& message);

    int32_t getOnlineCount();

    // DB persistence
    data::NotificationInfo::ptr get(int64_t id);

    data::NotificationInfo::ptr addNotification(const NotificationData& data);

    void update(data::NotificationInfo::ptr info);

    void listByUser(std::vector<data::NotificationInfo::ptr>& results, int64_t user_id, uint64_t offset, uint64_t size, const std::string& type = "");

    int64_t countByUser(int64_t user_id, const std::string& type = "");

    int64_t unreadCount(int64_t user_id);

    bool markRead(int64_t notification_id);

    bool markRead(const std::vector<int64_t>& ids);

    int64_t markAllRead(int64_t user_id);

    int64_t markReadByType(int64_t user_id, const std::string& type);

    void cleanupExpiredBroadcasts();

private:
    static data::NotificationInfo::ptr parseRow(chen::ISQLData::ptr rt);
};

typedef chen::Singleton<NotificationManager> NotificationMgr;

} // namespace blog
