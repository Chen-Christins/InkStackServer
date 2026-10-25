#include "category_delete_servlet.h"

#include <chen/log/log.h>

#include "../../manager/category_manager.h"
#include "../../util.h"
#include "blog/data/category_info.h"

#include <set>

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

CategoryDeleteServlet::CategoryDeleteServlet()
    : BlogAdminServlet("CategoryDeleteServlet")
{
}

void get_delete_values(std::map<int64_t, std::map<int64_t, data::CategoryInfo::ptr>> parent_map, int64_t id, std::set<data::CategoryInfo::ptr>& infos)
{
    auto it = parent_map[0].find(id);
    if (it == parent_map[0].end()) {
        return;
    }
    if (!infos.insert(it->second).second) {
        return;
    }
    auto iit = parent_map.find(id);
    if (iit == parent_map.end()) {
        return;
    }
    for (auto& i : iit->second) {
        get_delete_values(parent_map, i.first, infos);
    }
}

int32_t CategoryDeleteServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_STRING(result, ids, "ids");
        std::set<int64_t> cat_ids;
        auto tmp = chen::StringUtil::Split(ids, ",");
        for (auto& i : tmp) {
            cat_ids.insert(chen::TypeUtil::Atoi(i));
        }

        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(errcode::NOT_LOGIN);
            break;
        }
        std::vector<data::CategoryInfo::ptr> infos;
        CategoryMgr::GetInstance()->listAll(infos);
        if (infos.empty()) {
            result->setErrno(errcode::PARAM_MISSING, "no categories");
            break;
        }
        std::map<int64_t, std::map<int64_t, data::CategoryInfo::ptr>> parent_map;
        for (auto& i : infos) {
            if (i->getParentId()) {
                parent_map[i->getParentId()][i->getId()] = i;
            }
            parent_map[0][i->getId()] = i;
        }

        std::set<data::CategoryInfo::ptr> del_cats;
        for (auto& i : cat_ids) {
            get_delete_values(parent_map, i, del_cats);
        }

        auto db = getDB();
        if (!db) {
            result->setErrno(errcode::DB_OPERATION_FAILED);
            break;
        }
        auto trans = db->openTransaction();
        if (!trans) {
            result->setErrno(errcode::DB_TRANSACTION_FAILED);
            break;
        }
        time_t now = time(0);
        for (auto& i : del_cats) {
            i->setIsDeleted(1);
            i->setUpdateTime(now);
            data::CategoryInfoDao::Update(i, db);
        }
        if (!trans->commit()) {
            ERROR(logger) << "commit fail";
            result->setErrno(errcode::DB_COMMIT_FAILED);

            for (auto& i : del_cats) {
                i->setIsDeleted(0);
            }
            break;
        }
        for (auto& i : del_cats) {
            CategoryMgr::GetInstance()->add(i);
        }
        if (!del_cats.empty()) {
            result->set("total", del_cats.size());
            auto& jids = result->jsondata["list"];
            for (auto& i : del_cats) {
                Json::Value item;
                item["id"] = i->getId();
                item["name"] = i->getName();
                item["color"] = i->getColor();
                item["parent_id"] = i->getParentId();
                item["url"] = i->getUrl();
                item["icon"] = i->getIcon();
                item["desc"] = i->getDescription();
                item["status"] = i->getStatus();
                jids.append(item);
            }
        }
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
