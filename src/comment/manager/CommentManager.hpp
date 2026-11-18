#pragma once

#include "blog/data/comment_info.h"

#include <chen/db/query_builder.h>
#include <chen/util/singleton.h>

namespace blog {

class CommentManager {
public:
    enum Status {
        PENDING = 1,
        APPROVED = 2,
        REJECTED = 3,
        SPAM = 4
    };

    void add(data::CommentInfo::ptr info);

    data::CommentInfo::ptr get(int64_t id);

    data::CommentInfo::ptr create(
        int64_t article_id,
        int64_t user_id,
        const std::string& content,
        int64_t parent_id,
        const std::string& ip,
        const std::string& user_agent);

    bool update(int64_t id, const std::string& content);

    bool del(int64_t id);

    void listAllByArticle(std::vector<data::CommentInfo::ptr>& results, int64_t article_id);

    void listByArticle(std::vector<data::CommentInfo::ptr>& results, int64_t article_id, uint64_t offset, uint64_t size);

    void listReplies(std::vector<data::CommentInfo::ptr>& results, int64_t parent_id, uint64_t offset, uint64_t size);

    int64_t countByArticle(int64_t article_id);

    int64_t countReplies(int64_t parent_id);

    // --- admin methods ---
    int64_t listByAdmin(
        std::vector<data::CommentInfo::ptr>& results,
        int64_t page_num,
        int64_t page_size,
        int32_t status,
        const std::string& keyword,
        int64_t article_id,
        int32_t is_reported);

    int64_t batchUpdateStatus(const std::vector<int64_t>& ids, int32_t status);

    int64_t batchDelete(const std::vector<int64_t>& ids);

    struct CommentStats {
        int64_t total = 0;
        int64_t pending = 0;
        int64_t approved = 0;
        int64_t spam = 0;
        int64_t reported = 0;
    };

    CommentStats getStats();

private:
    static data::CommentInfo::ptr parseRow(chen::ISQLData::ptr rt);
};

typedef chen::Singleton<CommentManager> CommentMgr;

} // namespace blog
