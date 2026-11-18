#pragma once

#include "blog/data/user_follow_rel_info.h"

#include <chen/db/query_builder.h>
#include <chen/util/singleton.h>

namespace blog {

class UserFollowRelManager {
public:
    void add(data::UserFollowRelInfo::ptr info);

    data::UserFollowRelInfo::ptr get(int64_t id);

    data::UserFollowRelInfo::ptr getByFollowerAndFollowing(int64_t follower_id, int64_t following_id);

    // follow/unfollow
    data::UserFollowRelInfo::ptr follow(int64_t follower_id, int64_t following_id);

    bool unfollow(int64_t follower_id, int64_t following_id);

    bool isFollowing(int64_t follower_id, int64_t following_id);

    // queries with pagination
    void listFollowing(std::vector<data::UserFollowRelInfo::ptr>& results, int64_t follower_id, uint64_t offset, uint64_t size);

    void listFollowers(std::vector<data::UserFollowRelInfo::ptr>& results, int64_t following_id, uint64_t offset, uint64_t size);

    int64_t countFollowing(int64_t follower_id);

    int64_t countFollowers(int64_t following_id);

    /// 互相关注的用户ID列表（无分页，集合通常较小）
    void listMutualFollowing(std::vector<int64_t>& user_ids, int64_t uid);

private:
    static data::UserFollowRelInfo::ptr parseRow(chen::ISQLData::ptr rt);
};

typedef chen::Singleton<UserFollowRelManager> UserFollowRelMgr;

} // namespace blog
