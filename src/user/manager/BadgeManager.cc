#include "user/manager/BadgeManager.hpp"

#include "../../common/util/Util.hpp"

#include <chen/log/log.h>

namespace blog {

static chen::Logger::ptr logger = LOG_ROOT();

BadgeManager::BadgeManager()
{
}

void BadgeManager::ensureDefaults()
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return;
    }

    // 检查是否已有徽章数据
    std::vector<data::BadgeInfo::ptr> existing;
    data::BadgeInfoDao::QueryAll(existing, db);
    if (!existing.empty()) {
        return;
    }

    INFO(logger) << "seeding default badges...";

    insertDefault("初出茅庐", "发布第一篇", "seedling", "#22c55e", "published_articles", 1, 1);
    insertDefault("笔耕不辍", "累计发布 50 篇文章", "pen-nib", "#3b82f6", "published_articles", 50, 2);
    insertDefault("著作等身", "累计发布 100 篇文章", "book-open", "#8b5cf6", "published_articles", 100, 3);
    insertDefault("人气爆棚", "获得 100 个点赞", "heart", "#ef4444", "total_likes", 100, 4);
    insertDefault("广受好评", "获得 500 个点赞", "trophy", "#f59e0b", "total_likes", 500, 5);
    insertDefault("社区之星", "获得 1000 个关注", "star", "#eab308", "total_followers", 1000, 6);
}

void BadgeManager::insertDefault(
    const std::string& name,
    const std::string& desc,
    const std::string& icon,
    const std::string& color,
    const std::string& condition_key,
    int64_t condition_value,
    int32_t sort_order)
{
    auto db = GetDB();
    if (!db) {
        return;
    }

    data::BadgeInfo::ptr badge(new data::BadgeInfo);
    badge->setName(name);
    badge->setDescription(desc);
    badge->setIcon(icon);
    badge->setColor(color);
    badge->setConditionKey(condition_key);
    badge->setConditionValue(condition_value);
    badge->setSortOrder(sort_order);
    badge->setIsDeleted(0);
    badge->setCreateTime(time(0));
    badge->setUpdateTime(time(0));

    if (data::BadgeInfoDao::Insert(badge, db)) {
        ERROR(logger) << "insert badge failed: " << name << " errno=" << db->getErrno() << " errstr=" << db->getErrStr();
    }
}

void BadgeManager::listAll(std::vector<data::BadgeInfo::ptr>& badges)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return;
    }

    auto qb = data::BadgeInfoDao::newQuery();
    qb->where("is_deleted", "=", (int64_t)0);
    qb->orderBy("sort_order", "ASC");
    qb->orderBy("id", "ASC");

    if (data::BadgeInfoDao::QueryByBuilder(badges, qb, db)) {
        ERROR(logger) << "QueryByBuilder failed";
    }
}

} // namespace blog
