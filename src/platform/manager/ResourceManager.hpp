#pragma once

#include "blog/data/resource_info.h"

#include <chen/db/query_builder.h>
#include <chen/util/singleton.h>

namespace blog {

class ResourceManager {
public:
    enum ResourceType {
        TYPE_IMAGE = 1,
        TYPE_VIDEO = 2,
        TYPE_DOCUMENT = 3,
        TYPE_COMPRESSED = 4,
        TYPE_AUDIO = 5,
        TYPE_OTHER = 6,
    };

    enum Status {
        NORMAL = 1,
        DELETED = 2,
    };

    ResourceType GetResourceType(const std::string& filename);

    void add(blog::data::ResourceInfo::ptr info);

    data::ResourceInfo::ptr get(int64_t id);

    void getByBizUid(std::vector<data::ResourceInfo::ptr>& results, const std::string& biz_type, int64_t biz_id, int64_t uid);

    data::ResourceInfo::ptr getByBizUidName(const std::string& biz_type, int64_t biz_id, int64_t uid, const std::string& filename);

    data::ResourceInfo::ptr getByPath(const std::string& path);

private:
    static data::ResourceInfo::ptr parseRow(chen::ISQLData::ptr rt);
};

typedef chen::Singleton<ResourceManager> ResourceMgr;

} // namespace blog
