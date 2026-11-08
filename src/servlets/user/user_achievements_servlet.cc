/**
 * @file user_achievements_servlet.cc
 * @brief 用户成就数据接口实现
 * @author Christins
 * @date 2026-07-16
 * @copyright Apache 2.0
 */
#include "user_achievements_servlet.h"

#include "../../include/managers.h"

#include <chen/log/log.h>
#include <chen/util/time_util.h>

#include <map>

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

UserAchievementsServlet::UserAchievementsServlet()
    : BlogLoginedServlet("UserAchievementsServlet")
{
}

namespace {

// 日期计数映射为热力等级 0-4
int32_t countToLevel(int32_t count)
{
    if (count <= 0) {
        return 0;
    }
    if (count == 1) {
        return 1;
    }
    if (count <= 3) {
        return 2;
    }
    if (count <= 6) {
        return 3;
    }
    return 4;
}

} // anonymous namespace

int32_t UserAchievementsServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(errcode::NOT_LOGIN);
            break;
        }

        // ========== 1. 获取用户所有文章 ==========
        std::vector<data::ArticleInfo::ptr> articles;
        ArticleMgr::GetInstance()->listByUserId(articles, uid, true);

        int64_t published_articles = 0;
        int64_t total_likes = 0;
        std::map<std::string, int32_t> dateContributions; // date -> contribution count

        for (auto& a : articles) {
            if (!a) {
                continue;
            }
            if (a->getIsDeleted()) {
                continue;
            }
            if (a->getState() == ArticleManager::PUBLISHED) {
                published_articles++;
                total_likes += a->getPraise();

                // 使用 publish_time 计算贡献日期
                time_t pt = a->getCreateTime();
                std::string dateKey = chen::TimeUtil::Time2StrSec(pt, "%Y-%m-%d");
                dateContributions[dateKey]++;
            }
        }

        // ========== 2. 查询评论贡献 ==========
        {
            auto db = getDB();
            if (db) {
                time_t now = time(0);
                time_t cutoff = now - 371 * 86400;
                std::string cutoffStr = chen::TimeUtil::Time2StrSec(cutoff, "%Y-%m-%d");

                auto qb = data::CommentInfoDao::newQuery();
                qb->where("user_id", "=", uid);
                qb->where("is_deleted", "=", (int64_t)0);
                qb->where("create_time", ">=", cutoffStr);

                std::vector<int64_t> timestamps;
                if (qb->queryColumn<int64_t>(timestamps, db, "create_time") == 0) {
                    for (auto ts : timestamps) {
                        std::string dateKey = chen::TimeUtil::Time2StrSec(ts, "%Y-%m-%d");
                        dateContributions[dateKey]++;
                    }
                }
            }
        }

        // ========== 3. 计算热度图 (53 周 × 7 天) ==========
        time_t now = time(0);
        time_t today = chen::TimeUtil::GetDayStart(now);

        // 起始日期：53 周前的周日
        int32_t todayDow = chen::TimeUtil::GetDayOfWeek(today);
        time_t startDate = today - (371 + todayDow) * 86400;

        int64_t total_contributions = 0;
        Json::Value heatmap(Json::arrayValue);

        for (int32_t w = 0; w < 53; w++) {
            Json::Value week(Json::arrayValue);
            for (int32_t d = 0; d < 7; d++) {
                int32_t offset = w * 7 + d;
                time_t day = startDate + offset * 86400;
                std::string key = chen::TimeUtil::Time2StrSec(day, "%Y-%m-%d");
                int32_t count = dateContributions[key];
                int32_t level = countToLevel(count);
                total_contributions += level;
                week.append(level);
            }
            heatmap.append(week);
        }

        // ========== 4. 计算关注者数 ==========
        int64_t total_followers = UserFollowRelMgr::GetInstance()->countFollowers(uid);

        // ========== 5. 徽章解锁判断 ==========
        std::vector<data::BadgeInfo::ptr> badges;
        BadgeMgr::GetInstance()->listAll(badges);

        int64_t unlocked_count = 0;
        Json::Value badgesJson(Json::arrayValue);

        for (auto& b : badges) {
            if (!b) {
                continue;
            }

            Json::Value item;
            item["id"] = std::to_string(b->getId());
            item["name"] = b->getName();
            item["desc"] = b->getDescription();
            item["icon"] = b->getIcon();
            item["color"] = b->getColor();

            std::string ck = b->getConditionKey();
            int64_t cv = b->getConditionValue();
            int64_t current = 0;

            if (ck == "published_articles") {
                current = published_articles;
            } else if (ck == "total_likes") {
                current = total_likes;
            } else if (ck == "total_followers") {
                current = total_followers;
            }

            bool unlocked = (current >= cv);
            item["unlocked"] = unlocked;

            if (!unlocked) {
                char buf[64];
                snprintf(buf, sizeof(buf), "%ld / %ld", current, cv);
                item["progress"] = std::string(buf);
            }

            if (unlocked) {
                unlocked_count++;
            }

            badgesJson.append(item);
        }

        // ========== 6. 组装响应 ==========
        Json::Value stats;
        stats["total_contributions"] = total_contributions;
        stats["published_articles"] = published_articles;
        stats["total_likes"] = total_likes;
        stats["unlocked_badges"] = unlocked_count;
        stats["total_badges"] = static_cast<int64_t>(badges.size());

        result->set("stats", stats);
        result->set("badges", badgesJson);
        result->set("heatmap", heatmap);
        result->setErrno(errcode::SUCCESS);
    } while (0);

    DEBUG(logger) << "UserAchievementsServlet handle result: " << result->toJsonString();
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
