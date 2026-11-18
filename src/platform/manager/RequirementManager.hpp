#pragma once

#include "blog/data/requirement_info.h"

#include <chen/db/query_builder.h>
#include <chen/util/singleton.h>

namespace blog {

class RequirementManager {
public:
    enum Priority {
        PRIORITY_LOW = 1,
        PRIORITY_MEDIUM = 2,
        PRIORITY_HIGH = 3,
        PRIORITY_URGENT = 4
    };
    enum Status {
        STATUS_DRAFT = 0,
        STATUS_PENDING = 1,
        STATUS_INPROGRESS = 2,
        STATUS_DONE = 3,
        STATUS_CLOSED = 4
    };

    data::RequirementInfo::ptr get(int64_t id);

    uint64_t listByPages(std::vector<data::RequirementInfo::ptr>& infos, uint64_t offset, uint64_t size, int32_t status, bool isValid);

    uint64_t listByOrg(std::vector<data::RequirementInfo::ptr>& infos, int64_t orgId, uint64_t offset, uint64_t size, int32_t status, bool isValid);

private:
    static data::RequirementInfo::ptr parseRow(chen::ISQLData::ptr rt);
};

typedef chen::Singleton<RequirementManager> RequirementMgr;

} // namespace blog
