#include "label_query_servlet.h"

#include "../../include/managers.h"
#include "../../util.h"
#include "blog/data/article_label_rel_info.h"

#include <chen/log/log.h>

#include <map>

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

LabelQueryServlet::LabelQueryServlet()
    : BlogLoginedServlet("LabelQueryServlet")
{
}

int32_t LabelQueryServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        int64_t user_id = request->getParamAs<int64_t>("user_id");
        std::string ids = request->getParam("ids");
        if (user_id == 0 && ids.empty()) {
            result->setErrno(errcode::PARAM_MISSING);
            break;
        }

        std::vector<data::LabelInfo::ptr> infos;
        if (user_id) {
            LabelMgr::GetInstance()->listByUserId(infos, user_id, true);
        } else {
            auto tmp = chen::StringUtil::Split(ids, ",");
            for (auto& i : tmp) {
                auto id = chen::TypeUtil::Atoi(i);
                if (id) {
                    auto info = LabelMgr::GetInstance()->get(id);
                    if (info) {
                        infos.push_back(info);
                    }
                }
            }
        }
        std::map<int64_t, int64_t> article_counts;
        if (!infos.empty()) {
            auto db = GetDB();
            if (db) {
                std::vector<int64_t> label_ids;
                label_ids.reserve(infos.size());
                for (auto& i : infos) {
                    label_ids.push_back(i->getId());
                }
                auto qb = data::ArticleLabelRelInfoDao::newQuery("r");
                qb->select("r.label_id, COUNT(*) cnt");
                qb->join("article a", "r.article_id = a.id");
                qb->whereIn("r.label_id", label_ids);
                qb->where("r.is_deleted", "=", (int64_t)0);
                qb->where("a.state", "=", (int64_t)ArticleManager::PUBLISHED);
                qb->where("a.is_deleted", "=", (int64_t)0);
                qb->groupBy("r.label_id");
                std::vector<std::pair<int64_t, int64_t>> rows;
                if (qb->queryPairs<int64_t, int64_t>(rows, db) == 0) {
                    for (auto& [label_id, cnt] : rows) {
                        article_counts[label_id] = cnt;
                    }
                }
            }
        }

        Json::Value data;
        for (auto& i : infos) {
            Json::Value v;
            v["id"] = i->getId();
            v["name"] = i->getName();
            v["color"] = i->getColor();
            v["desc"] = i->getDescription();
            v["create_time"] = i->getCreateTime();
            v["article_count"] = article_counts[i->getId()];
            data.append(v);
        }
        result->setData(data);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
