/**
 * @file UserModule.hpp
 * @brief 用户服务模块
 * @author Christins
 * @date 2026-11-18
 * @copyright Apache 2.0
 */
#pragma once

#include <memory>

#include <chen/module/module.h>

namespace blog {

/**
 * @brief 用户服务模块
 * @details 负责用户、认证、会话、关注、徽章等接口，独立进程运行。
 */
class UserModule : public chen::Module {
public:
    typedef std::shared_ptr<UserModule> ptr;

    UserModule();

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
