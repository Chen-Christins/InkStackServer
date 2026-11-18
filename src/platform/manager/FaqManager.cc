#include "platform/manager/FaqManager.hpp"

#include "common/Util.hpp"

#include <chen/log/log.h>

namespace blog {

static chen::Logger::ptr logger = LOG_ROOT();

FaqManager::FaqManager()
{
}

bool FaqManager::listAll(std::vector<data::HelpFaqsInfo::ptr>& infos)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return false;
    }
    auto qb = data::HelpFaqsInfoDao::newQuery();
    qb->where("is_deleted", "=", (int64_t)0);
    qb->orderBy("sort_order", "ASC");
    qb->orderBy("id", "ASC");
    if (data::HelpFaqsInfoDao::QueryByBuilder(infos, qb, db)) {
        ERROR(logger) << "QueryByBuilder failed";
        return false;
    }
    return true;
}

bool FaqManager::searchByKeyword(const std::string& keyword, std::vector<data::HelpFaqsInfo::ptr>& infos)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return false;
    }
    auto qb = data::HelpFaqsInfoDao::newQuery();
    qb->where("is_deleted", "=", (int64_t)0);
    qb->orderBy("sort_order", "ASC");
    qb->orderBy("id", "ASC");
    std::vector<data::HelpFaqsInfo::ptr> all;
    if (data::HelpFaqsInfoDao::QueryByBuilder(all, qb, db)) {
        ERROR(logger) << "QueryByBuilder failed";
        return false;
    }
    for (auto& info : all) {
        if (info->getQ().find(keyword) != std::string::npos || info->getA().find(keyword) != std::string::npos) {
            infos.push_back(info);
        }
    }
    return true;
}

bool FaqManager::remove(int64_t id)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return false;
    }

    auto info = data::HelpFaqsInfoDao::Query(id, db);
    if (!info) {
        ERROR(logger) << "faq not found: " << id;
        return false;
    }

    info->setIsDeleted(1);
    info->setUpdateTime(time(0));

    if (data::HelpFaqsInfoDao::Update(info, db)) {
        ERROR(logger) << "soft delete faq failed, errno=" << db->getErrno() << " errstr=" << db->getErrStr();
        return false;
    }

    return true;
}

bool FaqManager::update(int64_t id, const std::string& q, const std::string& a, const std::string& cat)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return false;
    }

    auto info = data::HelpFaqsInfoDao::Query(id, db);
    if (!info) {
        ERROR(logger) << "faq not found: " << id;
        return false;
    }

    if (!q.empty()) {
        info->setQ(q);
    }
    if (!a.empty()) {
        info->setA(a);
    }
    if (!cat.empty()) {
        info->setCat(cat);
    }
    info->setUpdateTime(time(0));

    if (data::HelpFaqsInfoDao::Update(info, db)) {
        ERROR(logger) << "update faq failed, errno=" << db->getErrno() << " errstr=" << db->getErrStr();
        return false;
    }

    return true;
}

} // namespace blog
