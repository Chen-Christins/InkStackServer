#include "metrics_servlet.h"

#include "../../manager/article_manager.h"
#include "../../manager/notification_manager.h"

#include <chen/log/log.h>

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

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
        // TODO: to expose the details of current service, like every api qps, the server ram info etc.
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
