#pragma once

#include "common/frame/Struct.hpp"
#include <json/json.h>

#include "blog/data/task_info.h"

namespace blog {
namespace servlet {

class RdTaskEditServlet : public BlogLoginedServlet {
public:
    typedef std::shared_ptr<RdTaskEditServlet> ptr;
    RdTaskEditServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;

private:
    void notifyAssignee(int64_t assignee_id, data::TaskInfo::ptr task);
};

} // namespace servlet
} // namespace blog
