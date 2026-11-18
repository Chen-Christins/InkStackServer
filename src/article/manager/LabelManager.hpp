#pragma once

#include "blog/data/label_info.h"

#include <chen/db/query_builder.h>
#include <chen/util/singleton.h>

namespace blog {

class LabelManager {
public:
    void add(data::LabelInfo::ptr info);

    data::LabelInfo::ptr get(int64_t id);

    data::LabelInfo::ptr getByUserIdName(int64_t id, const std::string& name);

    bool listByUserId(std::vector<data::LabelInfo::ptr>& infos, int64_t id, bool valid);

private:
    static data::LabelInfo::ptr parseRow(chen::ISQLData::ptr rt);
};

typedef chen::Singleton<LabelManager> LabelMgr;

} // namespace blog
