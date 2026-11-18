#pragma once

#include "blog/data/backup_record_info.h"

#include <chen/db/query_builder.h>
#include <chen/util/singleton.h>

namespace blog {

class BackupRecordManager {
public:
    data::BackupRecordInfo::ptr get(int64_t id);

    bool list(
        std::vector<data::BackupRecordInfo::ptr>& results,
        int64_t& total,
        const std::string& search,
        const std::string& type,
        const std::string& status,
        int32_t offset,
        int32_t limit);

    data::BackupRecordInfo::ptr create(int64_t uid, const std::string& type, const std::string& name, const std::string& description);

    bool remove(int64_t id);

private:
    static data::BackupRecordInfo::ptr parseRow(chen::ISQLData::ptr rt);
};

typedef chen::Singleton<BackupRecordManager> BackupRecordMgr;

} // namespace blog
