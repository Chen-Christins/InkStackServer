#pragma once

#include "blog/data/organization_apply_info.h"

#include <chen/db/query_builder.h>
#include <chen/util/singleton.h>

namespace blog {

class OrganizationApplyManager {
public:
    enum Status {
        PENDING = 0,
        APPROVED = 1,
        REJECTED = 2
    };

    data::OrganizationApplyInfo::ptr get(int64_t id);

    int64_t listByPages(std::vector<data::OrganizationApplyInfo::ptr>& results, uint64_t offset, uint64_t limit, int32_t status, bool isValid);

    int64_t listByUserId(std::vector<data::OrganizationApplyInfo::ptr>& results, int64_t user_id, uint64_t offset, uint64_t limit, bool isValid);

private:
    static data::OrganizationApplyInfo::ptr parseRow(chen::ISQLData::ptr rt);
};

typedef chen::Singleton<OrganizationApplyManager> OrganizationApplyMgr;

} // namespace blog
