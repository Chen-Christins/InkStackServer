/**
 * @file RdPrDistributionServlet.hpp
 * @brief RD PR 分布统计接口
 * @author Christins
 * @date 2026-10-19
 * @copyright Apache 2.0
 */
#pragma once

#include "common/frame/Struct.hpp"

#include <json/json.h>

namespace blog {
namespace servlet {

class RdPrDistributionServlet : public BlogLoginedServlet {
public:
    typedef std::shared_ptr<RdPrDistributionServlet> ptr;
    RdPrDistributionServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};

} // namespace servlet
} // namespace blog
