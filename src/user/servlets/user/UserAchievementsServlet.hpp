/**
 * @file UserAchievementsServlet.hpp
 * @brief 用户成就数据接口
 * @author Christins
 * @date 2026-07-16
 * @copyright Apache 2.0
 */
#pragma once

#include "common/frame/Struct.hpp"

namespace blog {
namespace servlet {

class UserAchievementsServlet : public BlogLoginedServlet {
public:
    typedef std::shared_ptr<UserAchievementsServlet> ptr;
    UserAchievementsServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};

} // namespace servlet
} // namespace blog
