#pragma once

#include "blog/data/organization_repos_info.h"

#include <chen/db/query_builder.h>
#include <chen/util/singleton.h>

namespace blog {

class OrganizationRepoManager {
public:
    void add(data::OrganizationReposInfo::ptr info);

    void del(int64_t id);

    void onTick();

    data::OrganizationReposInfo::ptr get(int64_t id);

    data::OrganizationReposInfo::ptr getByOrgAndName(int64_t org_id, const std::string& name);

    int64_t listByOrgPages(std::vector<data::OrganizationReposInfo::ptr>& repos, int64_t org_id, uint64_t offset, uint64_t limit);

    int64_t getCountByOrg(int64_t org_id);

    /// 获取所有含有 token 的仓库信息
    void getAllWithToken(std::vector<data::OrganizationReposInfo::ptr>& repos);

    /// 从 GitHub API 异步同步仓库数据（stars_count、language、description）
    static void SyncFromGitHub(int64_t repo_id, const std::string& url, const std::string& token);

private:
    static data::OrganizationReposInfo::ptr parseRow(chen::ISQLData::ptr rt);

    void invalidateCountCache(int64_t org_id);

private:
};

typedef chen::Singleton<OrganizationRepoManager> OrganizationRepoMgr;

} // namespace blog
