#include "platform/manager/ExportRecordManager.hpp"

#include "../../common/util/Util.hpp"

#include <chen/log/log.h>

namespace blog {

static chen::Logger::ptr logger = LOG_ROOT();

data::ExportRecordInfo::ptr ExportRecordManager::parseRow(chen::ISQLData::ptr rt)
{
    return data::ExportRecordInfoDao::ParseRow(rt);
}

data::ExportRecordInfo::ptr ExportRecordManager::get(int64_t id)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    return data::ExportRecordInfoDao::Query(id, db);
}

bool ExportRecordManager::list(
    std::vector<data::ExportRecordInfo::ptr>& results,
    int64_t& total,
    const std::string& search,
    const std::string& type,
    const std::string& status,
    int32_t offset,
    int32_t limit)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return false;
    }
    auto qb = data::ExportRecordInfoDao::newQuery();
    qb->where("is_deleted", "=", (int64_t)0);
    if (!search.empty()) {
        qb->where("name", "LIKE", "%" + search + "%");
    }
    if (!type.empty()) {
        qb->where("type", "=", type);
    }
    if (!status.empty()) {
        qb->where("status", "=", status);
    }
    qb->orderBy("id", "DESC");
    if (data::ExportRecordInfoDao::QueryByBuilderPages(results, total, qb, offset, limit, db)) {
        ERROR(logger) << "export_record QueryByBuilderPages failed";
        return false;
    }
    return true;
}

data::ExportRecordInfo::ptr ExportRecordManager::create(int64_t uid, const std::string& type, const std::string& name)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    auto info = std::make_shared<data::ExportRecordInfo>();
    info->setCreatedBy(uid);
    info->setType(type);
    info->setName(name);
    info->setStatus("processing");
    info->setIsDeleted(0);
    info->setSize(0);
    info->setRecordCount(0);
    info->setCreateTime(time(0));
    if (data::ExportRecordInfoDao::Insert(info, db)) {
        ERROR(logger) << "Insert export_record failed";
        return nullptr;
    }
    return info;
}

bool ExportRecordManager::remove(int64_t id)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return false;
    }
    auto info = get(id);
    if (!info) {
        return false;
    }
    info->setIsDeleted(1);
    if (data::ExportRecordInfoDao::Update(info, db)) {
        ERROR(logger) << "Delete export_record failed";
        return false;
    }
    return true;
}

} // namespace blog
