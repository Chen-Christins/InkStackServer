#pragma once

#include "blog/data/article_praise_rel_info.h"

#include <chen/db/query_builder.h>
#include <chen/util/singleton.h>

namespace blog {

class ArticlePraiseRelManager {
public:
    void add(data::ArticlePraiseRelInfo::ptr info);

    data::ArticlePraiseRelInfo::ptr get(int64_t id);

    data::ArticlePraiseRelInfo::ptr getByUserAndArticle(int64_t user_id, int64_t article_id);

    // praise/unpraise
    data::ArticlePraiseRelInfo::ptr praise(int64_t user_id, int64_t article_id);

    bool unpraise(int64_t user_id, int64_t article_id);

    bool isPraising(int64_t user_id, int64_t article_id);

    // queries
    void listByArticle(std::vector<data::ArticlePraiseRelInfo::ptr>& results, int64_t article_id, uint64_t offset, uint64_t size);

    void listByUser(std::vector<data::ArticlePraiseRelInfo::ptr>& results, int64_t user_id, uint64_t offset, uint64_t size);

    int64_t countByArticle(int64_t article_id);

    int64_t countByUser(int64_t user_id);

private:
    static data::ArticlePraiseRelInfo::ptr parseRow(chen::ISQLData::ptr rt);
};

typedef chen::Singleton<ArticlePraiseRelManager> ArticlePraiseRelMgr;

} // namespace blog
