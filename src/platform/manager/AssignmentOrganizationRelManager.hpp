#pragma once

#include "blog/data/assignment_organization_rel_info.h"

#include <chen/db/query_builder.h>
#include <chen/util/singleton.h>

namespace blog {

class AssignmentOrganizationRelManager {
public:
    blog::data::AssignmentOrganizationRelInfo::ptr get(int64_t id);

    blog::data::AssignmentOrganizationRelInfo::ptr getByOrgAndAssign(int64_t org_id, int64_t assign_id);

    int64_t getByAssignmentId(std::vector<data::AssignmentOrganizationRelInfo::ptr>& results, int64_t assign_id);

    int64_t getByPages(
        std::vector<data::AssignmentOrganizationRelInfo::ptr>& results,
        int64_t o_id,
        uint64_t offset,
        uint64_t size,
        int32_t status,
        bool isValid);

private:
    static data::AssignmentOrganizationRelInfo::ptr parseRow(chen::ISQLData::ptr rt);
};

typedef chen::Singleton<AssignmentOrganizationRelManager> AssignmentOrganizationRelMgr;

} // namespace blog
