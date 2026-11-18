/**
 * @file ArticleModule.hpp
 * @brief 文章服务模块
 * @author Christins
 * @date 2025-05-10
 * @copyright Apache 2.0
 */
#pragma once

#include <memory>

#include <chen/module/module.h>

namespace blog {

/**
 * @brief 文章服务模块
 * @details 负责文章、分类、标签、搜索索引等接口，独立进程运行。
 */
class ArticleModule : public chen::Module {
public:
    typedef std::shared_ptr<ArticleModule> ptr;

    ArticleModule();

    bool onLoad() override;
    bool onUnload() override;
    void onTick() override;
    uint64_t getTickIntervalMs() override;
    bool onServerReady() override;
    bool onServerUp() override;
    bool onDeactivate() override;

private:
    static bool initMySQL();
    void registerServlets();
};

} // namespace blog
