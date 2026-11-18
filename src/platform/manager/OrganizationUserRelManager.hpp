#pragma once

#include "blog/data/organization_user_rel_info.h"

#include <chen/db/query_builder.h>
#include <chen/util/singleton.h>

namespace blog {

class OrganizationUserRelManager {
public:
    enum Status {
        PENDING = 0,
        APPROVED = 1,
        REJECTED = 2,
        EXITED = 3
    };

    void add(data::OrganizationUserRelInfo::ptr info);

    data::OrganizationUserRelInfo::ptr get(int64_t id);

    data::OrganizationUserRelInfo::ptr getByOrgAndUser(int64_t o_id, int64_t u_id);

    int64_t getByPages(
        std::vector<data::OrganizationUserRelInfo::ptr>& results,
        int64_t o_id,
        uint64_t offset,
        uint64_t size,
        int32_t status,
        bool isValid);

    int64_t getOrgByUserId(std::vector<data::OrganizationUserRelInfo::ptr>& results, int64_t u_id, int32_t status, bool isValid);

    int64_t getMemberCount(int64_t o_id, int32_t status, bool isValid);

    struct Stats {
        int64_t total_members = 0;
        int64_t active_members = 0;
        int64_t org_admin_count = 0;
        int64_t regular_count = 0;
    };

    Stats getStats(int64_t org_id);

private:
    static data::OrganizationUserRelInfo::ptr parseRow(chen::ISQLData::ptr rt);
};

typedef chen::Singleton<OrganizationUserRelManager> OrganizationUserRelMgr;

} // namespace blog
