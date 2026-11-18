/**
 * @file ServletRegistrar.hpp
 * @brief ServletDispatch 代理，注册路由时注入指标标签
 * @author Christins
 * @date 2026-11-18
 * @copyright Apache 2.0
 */
#pragma once

#include <memory>
#include <string>
#include <utility>

#include <chen/http/servlet.h>
#include <chen/http/servlet_dispatch.h>

#include "common/frame/Struct.hpp"

namespace blog {

/**
 * @brief ServletDispatch 代理
 * @details 注册路由时把路由模板注入 BlogServlet，用于指标标签；现有注册代码无需改动
 */
class ServletRegistrar {
public:
    explicit ServletRegistrar(chen::http::ServletDispatch::ptr dispatch)
        : m_dispatch(std::move(dispatch))
    {
    }

    ServletRegistrar* operator->() { return this; }

    void addServlet(const std::string& uri, chen::http::Servlet::ptr servlet)
    {
        if (!m_dispatch || !servlet) {
            return;
        }
        auto blog_servlet = std::dynamic_pointer_cast<BlogServlet>(servlet);
        if (blog_servlet) {
            blog_servlet->setRoute(uri);
        }
        m_dispatch->addServlet(uri, servlet);
    }

private:
    chen::http::ServletDispatch::ptr m_dispatch;
};

} // namespace blog
