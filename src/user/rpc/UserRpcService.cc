#include "user/rpc/UserRpcService.hpp"

#include <vector>

#include "protocol_user.h" // IWYU pragma: keep

#include "common/rpc/RpcHubClient.hpp"
#include "user/manager/UserAIConfigManager.hpp"
#include "user/manager/UserFollowRelManager.hpp"
#include "user/manager/UserManager.hpp"

namespace blog {
namespace rpc {

namespace {

/**
 * @brief 用户实体 -> 跨服务只读视图
 */
tagUserBrief ToUserBrief(const data::UserInfo::ptr& user)
{
    tagUserBrief brief;
    brief.Id = user->getId();
    brief.Name = user->getName();
    brief.Avatar = user->getAvatar();
    brief.Email = user->getEmail();
    brief.Role = user->getRole();
    brief.State = user->getState();
    brief.IsDeleted = user->getIsDeleted() != 0;
    brief.UpdateTime = user->getUpdateTime();
    return brief;
}

} // namespace

void RegisterUserRpcMethods()
{
    auto hub = RpcHubClientMgr::GetInstance();

    hub->registerMethod(USER_CMD_GET_BRIEF, [](int64_t uid) -> tagUserBrief {
        auto user = UserMgr::GetInstance()->get(uid);
        if (!user) {
            return {};
        }
        return ToUserBrief(user);
    });

    hub->registerMethod(USER_CMD_GET_BRIEFS, [](std::vector<int64_t> uids) -> std::vector<tagUserBrief> {
        std::vector<tagUserBrief> briefs;
        briefs.reserve(uids.size());
        for (int64_t uid : uids) {
            auto user = UserMgr::GetInstance()->get(uid);
            if (!user) {
                continue;
            }
            briefs.push_back(ToUserBrief(user));
        }
        return briefs;
    });

    hub->registerMethod(USER_CMD_GET_ALL_IDS, []() -> std::vector<int64_t> {
        std::vector<int64_t> ids;
        UserMgr::GetInstance()->getAllIds(ids, true);
        return ids;
    });

    hub->registerMethod(USER_CMD_GET_FOLLOW_COUNTS, [](int64_t uid) -> tagUserFollowCounts {
        tagUserFollowCounts counts;
        counts.Followers = UserFollowRelMgr::GetInstance()->countFollowers(uid);
        counts.Following = UserFollowRelMgr::GetInstance()->countFollowing(uid);
        return counts;
    });

    hub->registerMethod(USER_CMD_IS_FOLLOWING, [](int64_t uid, int64_t target) -> bool {
        return UserFollowRelMgr::GetInstance()->isFollowing(uid, target);
    });

    hub->registerMethod(USER_CMD_GET_AI_CONFIG, [](int64_t uid) -> tagUserAiConfig {
        auto config = UserAIConfigMgr::GetInstance()->getByUserId(uid);
        if (!config) {
            return {};
        }
        tagUserAiConfig out;
        out.Type = config->getType();
        out.Model = config->getModel();
        out.Url = config->getUrl();
        out.ApiKey = config->getApiKey();
        return out;
    });
}

} // namespace rpc
} // namespace blog
