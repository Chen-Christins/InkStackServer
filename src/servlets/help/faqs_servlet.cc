#include "faqs_servlet.h"

#include "../../include/managers.h"

#include <chen/log/log.h>

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

FaqsServlet::FaqsServlet()
    : BlogServlet("FaqsServlet")
{
}

int32_t FaqsServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        std::string keyword = request->getParam("keyword");

        std::vector<data::HelpFaqsInfo::ptr> infos;
        if (keyword.empty()) {
            FaqMgr::GetInstance()->listAll(infos);
        } else {
            FaqMgr::GetInstance()->searchByKeyword(keyword, infos);
        }

        Json::Value data;
        for (auto& i : infos) {
            Json::Value v;
            v["id"] = i->getId();
            v["cat"] = i->getCat();
            v["q"] = i->getQ();
            v["a"] = i->getA();
            data.append(v);
        }
        result->set("data", data);
    } while (0);
    DEBUG(logger) << "FaqsServlet handle result: " << result->toJsonString();
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
