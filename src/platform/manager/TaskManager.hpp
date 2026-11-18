#pragma once

#include "blog/data/task_info.h"

#include <chen/db/query_builder.h>
#include <chen/util/singleton.h>

namespace blog {

class TaskManager {
public:
    enum Priority {
        PRIORITY_LOW = 1,
        PRIORITY_MEDIUM = 2,
        PRIORITY_HIGH = 3,
        PRIORITY_URGENT = 4
    };
    enum Status {
        STATUS_TODO = 0,
        STATUS_INPROGRESS = 1,
        STATUS_DONE = 2,
        STATUS_CLOSED = 3
    };

    data::TaskInfo::ptr get(int64_t id);

    uint64_t listByPages(std::vector<data::TaskInfo::ptr>& infos, uint64_t offset, uint64_t size, int32_t status, bool isValid);

    uint64_t listByOrg(std::vector<data::TaskInfo::ptr>& infos, int64_t orgId, uint64_t offset, uint64_t size, int32_t status, bool isValid);

private:
    static data::TaskInfo::ptr parseRow(chen::ISQLData::ptr rt);
};

typedef chen::Singleton<TaskManager> TaskMgr;

} // namespace blog
