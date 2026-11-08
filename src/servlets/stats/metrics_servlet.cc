#include "metrics_servlet.h"

#include <chen/metrics/metrics.h>
#include <chen/util/json_util.h>

namespace blog {
namespace servlet {

MetricsAdminServlet::MetricsAdminServlet()
    : BlogAdminServlet("MetricsAdminServlet")
{
}

int32_t MetricsAdminServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        auto* registry = chen::metrics::RegistryMgr::GetInstance();
        if (!registry) {
            result->setErrno(errcode::INTERNAL_ERROR);
            break;
        }

        Json::Value data;
        if (!chen::JsonUtil::FromString(data, registry->RenderJson())) {
            result->setErrno(errcode::INTERNAL_ERROR);
            break;
        }

        result->setData(data);
        result->setErrno(errcode::SUCCESS);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
