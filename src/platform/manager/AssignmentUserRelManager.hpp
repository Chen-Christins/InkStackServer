#pragma once

#include "blog/data/assignment_user_rel_info.h"

#include <chen/db/query_builder.h>
#include <chen/util/singleton.h>

namespace blog {

class AssignmentUserRelManager {
public:
    enum Status {
        SUBMITTED = 1,
        GRADED = 2,
        LATE = 3
    };

    blog::data::AssignmentUserRelInfo::ptr get(int64_t id);

    blog::data::AssignmentUserRelInfo::ptr getByAssignAndUser(int64_t assign_id, int64_t user_id);

private:
    static data::AssignmentUserRelInfo::ptr parseRow(chen::ISQLData::ptr rt);
};

typedef chen::Singleton<AssignmentUserRelManager> AssignmentUserRelMgr;

} // namespace blog
