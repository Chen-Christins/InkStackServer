#pragma once

#include "common/frame/Struct.hpp"

namespace blog {
namespace servlet {

class ConversationServlet : public BlogLoginedServlet {
public:
    ConversationServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};

} // namespace servlet
} // namespace blog
