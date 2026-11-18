#pragma once

#include "blog/data/organization_info.h"

#include <chen/db/query_builder.h>
#include <chen/util/singleton.h>

namespace blog {

class OrganizationManager {
public:
    enum Role {
        MEMBER = 1,
        REPORTER = 2,
        DEVELOPER = 3,
        DEV_LEAD = 4,
        ORG_ADMIN = 5
    };
    enum Status {
        INACTIVE = 0,
        ACTIVE = 1,
    };

    void add(data::OrganizationInfo::ptr info);

    data::OrganizationInfo::ptr get(int64_t id);

    data::OrganizationInfo::ptr getByName(const std::string& name);

    int64_t listByPages(std::vector<data::OrganizationInfo::ptr>& orgs, uint64_t offset, uint64_t limit, int32_t status, bool isValid);

    struct OrganizationStats {
        int64_t total = 0;
        int64_t active = 0;
        int64_t inactive = 0;
    };

    OrganizationStats getStats();

private:
    static data::OrganizationInfo::ptr parseRow(chen::ISQLData::ptr rt);
};

typedef chen::Singleton<OrganizationManager> OrganizationMgr;

} // namespace blog
