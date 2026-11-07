#include "label_delete_servlet.h"

#include <chen/log/log.h>

#include "../../manager/label_manager.h"
#include "../../util.h"
#include "blog/data/label_info.h"

#include <set>

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

LabelDeleteServlet::LabelDeleteServlet()
    : BlogLoginedServlet("LabelDeleteServlet")
{
}

int32_t LabelDeleteServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_STRING(result, ids, "ids");
        std::set<int64_t> label_ids;
        auto tmp = chen::StringUtil::Split(ids, ",");
        for (auto& i : tmp) {
            label_ids.insert(chen::TypeUtil::Atoi(i));
        }

        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(errcode::NOT_LOGIN);
            break;
        }
        std::vector<data::LabelInfo::ptr> infos;
        if (!LabelMgr::GetInstance()->listByUserId(infos, uid, true)) {
            break;
        }

        std::vector<data::LabelInfo::ptr> del_labels;
        for (auto& i : infos) {
            if (label_ids.count(i->getId())) {
                del_labels.push_back(i);
            }
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
        for (auto& i : del_labels) {
            i->setIsDeleted(1);
            i->setUpdateTime(now);
            data::LabelInfoDao::Update(i, db);
        }
        if (!trans->commit()) {
            ERROR(logger) << "commit fail";
            result->setErrno(errcode::DB_COMMIT_FAILED);

            for (auto& i : del_labels) {
                i->setIsDeleted(0);
            }
            break;
        }
        for (auto& i : del_labels) {
            LabelMgr::GetInstance()->add(i);
        }
        if (!del_labels.empty()) {
            Json::Value json_ids;
            for (auto& i : del_labels) {
                json_ids.append(i->getId());
            }
            result->set("ids", json_ids);
        }
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
