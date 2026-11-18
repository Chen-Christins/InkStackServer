#pragma once

#include "common/frame/Struct.hpp"
#include "blog/data/requirement_info.h"
#include <json/json.h>

namespace blog {
namespace servlet {

class RdRequirementEditServlet : public BlogLoginedServlet {
public:
    typedef std::shared_ptr<RdRequirementEditServlet> ptr;
    RdRequirementEditServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;

private:
    void notifyAssignee(int64_t assignee_id, data::RequirementInfo::ptr requirement);
};

} // namespace servlet
} // namespace blog
