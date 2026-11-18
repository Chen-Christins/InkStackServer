#include "platform/servlets/help/FeedbackServlet.hpp"

#include "../../../common/util/Util.hpp"
#include "common/include/Managers.hpp"

#include <chen/log/log.h>

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

FeedbackServlet::FeedbackServlet()
    : BlogServlet("FeedbackServlet")
{
}

int32_t FeedbackServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_STRING(result, type, "type");
        DEFINE_AND_CHECK_STRING(result, content, "content");
        std::string contact = request->getParamAs<std::string>("contact");

        int64_t uid = getUserId(request);

        data::UserFeedbackInfo::ptr info(new data::UserFeedbackInfo);
        info->setType(type);
        info->setContent(content);
        info->setContact(contact);
        info->setUserId(uid);
        info->setCreateTime(time(0));
        info->setUpdateTime(time(0));

        auto db = getDB();
        if (!db) {
            result->setErrno(errcode::DB_OPERATION_FAILED);
            break;
        }

        if (data::UserFeedbackInfoDao::Insert(info, db)) {
            result->setErrno(errcode::DB_OPERATION_FAILED, "feedback insert failed");
            ERROR(logger) << "db error, errno=" << db->getErrno() << " errstr=" << db->getErrStr();
            break;
        }

        result->set("id", info->getId());
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
