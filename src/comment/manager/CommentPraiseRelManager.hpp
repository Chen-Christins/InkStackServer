#pragma once

#include "blog/data/comment_praise_rel_info.h"

#include <chen/db/query_builder.h>
#include <chen/util/singleton.h>

namespace blog {

class CommentPraiseRelManager {
public:
    void add(data::CommentPraiseRelInfo::ptr info);

    data::CommentPraiseRelInfo::ptr get(int64_t id);

    data::CommentPraiseRelInfo::ptr getByUserAndComment(int64_t user_id, int64_t comment_id);

    // praise/unpraise
    data::CommentPraiseRelInfo::ptr praise(int64_t user_id, int64_t comment_id);

    bool unpraise(int64_t user_id, int64_t comment_id);

    bool isPraising(int64_t user_id, int64_t comment_id);

    // count praises for a comment
    int64_t countByComment(int64_t comment_id);

private:
    static data::CommentPraiseRelInfo::ptr parseRow(chen::ISQLData::ptr rt);
};

typedef chen::Singleton<CommentPraiseRelManager> CommentPraiseRelMgr;

} // namespace blog
