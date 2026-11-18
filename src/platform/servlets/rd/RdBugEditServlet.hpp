#pragma once

#include "common/frame/Struct.hpp"
#include <json/json.h>

#include "blog/data/bug_info.h"

namespace blog {
namespace servlet {

class RdBugEditServlet : public BlogLoginedServlet {
public:
    typedef std::shared_ptr<RdBugEditServlet> ptr;
    RdBugEditServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;

private:
    void notifyAssignee(int64_t assignee_id, data::BugInfo::ptr bug);
};

} // namespace servlet
} // namespace blog
