/**
 * @file CommentModule.hpp
 * @brief 评论服务模块
 * @author Christins
 * @date 2026-11-18
 * @copyright Apache 2.0
 */
#pragma once

#include <memory>

#include <chen/module/module.h>

namespace blog {

/**
 * @brief 评论服务模块
 * @details 负责文章评论、评论点赞、评论审核等接口，独立进程运行。
 */
class CommentModule : public chen::Module {
public:
    typedef std::shared_ptr<CommentModule> ptr;

    CommentModule();

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
