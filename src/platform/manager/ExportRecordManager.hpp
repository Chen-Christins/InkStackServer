#pragma once

#include "blog/data/export_record_info.h"

#include <chen/db/query_builder.h>
#include <chen/util/singleton.h>

namespace blog {

class ExportRecordManager {
public:
    data::ExportRecordInfo::ptr get(int64_t id);

    bool list(
        std::vector<data::ExportRecordInfo::ptr>& results,
        int64_t& total,
        const std::string& search,
        const std::string& type,
        const std::string& status,
        int32_t offset,
        int32_t limit);

    data::ExportRecordInfo::ptr create(int64_t uid, const std::string& type, const std::string& name);

    bool remove(int64_t id);

private:
    static data::ExportRecordInfo::ptr parseRow(chen::ISQLData::ptr rt);
};

typedef chen::Singleton<ExportRecordManager> ExportRecordMgr;

} // namespace blog
