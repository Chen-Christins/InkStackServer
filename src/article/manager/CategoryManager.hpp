#pragma once

#include "blog/data/category_info.h"

#include <chen/db/query_builder.h>
#include <chen/util/singleton.h>

namespace blog {

class CategoryManager {
public:
    void listAll(std::vector<blog::data::CategoryInfo::ptr>& infos, bool isValid = false);

    bool listByUserId(std::vector<blog::data::CategoryInfo::ptr>& infos, int64_t user_id, bool isValid = false);

    void add(blog::data::CategoryInfo::ptr info);

    blog::data::CategoryInfo::ptr get(int64_t id);

    blog::data::CategoryInfo::ptr getByUserIdName(int64_t user_id, const std::string& name);

private:
    static data::CategoryInfo::ptr parseRow(chen::ISQLData::ptr rt);
};

typedef chen::Singleton<CategoryManager> CategoryMgr;

} // namespace blog
