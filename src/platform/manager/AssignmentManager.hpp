#pragma once

#include <memory>

#include "blog/data/assignment_info.h"

#include <chen/db/query_builder.h>
#include <chen/util/singleton.h>

namespace blog {

class AssignmentManager {
public:
    typedef std::shared_ptr<AssignmentManager> ptr;

    enum Status {
        DRAFT = 0,
        ACTIVE = 1,
        INACTIVE = 2,
    };

    void add(data::AssignmentInfo::ptr info);

    uint64_t listByPages(std::vector<data::AssignmentInfo::ptr>& infos, uint64_t offset, uint64_t size, int32_t status, bool isValid);

    data::AssignmentInfo::ptr get(int64_t id);

    data::AssignmentInfo::ptr getByName(const std::string& subject_name, const std::string& name);

    struct AssignmentStats {
        int64_t total = 0;
        int64_t active = 0;
        int64_t closed = 0;
        int64_t draft = 0;
    };

    AssignmentStats getStats();

private:
    static data::AssignmentInfo::ptr parseRow(chen::ISQLData::ptr rt);
};

typedef chen::Singleton<AssignmentManager> AssignmentMgr;

} // namespace blog
