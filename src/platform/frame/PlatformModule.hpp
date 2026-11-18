/**
 * @file PlatformModule.hpp
 * @brief 平台服务模块
 * @author Christins
 * @date 2026-11-18
 * @copyright Apache 2.0
 */
#pragma once

#include <memory>

#include <chen/module/module.h>

namespace blog {

/**
 * @brief 平台服务模块
 * @details 负责组织、作业、研发平台、通知、私信、文件、设置、帮助、
 *          数据库备份、仪表盘、统计与 WebSocket 网关，独立进程运行。
 */
class PlatformModule : public chen::Module {
public:
    typedef std::shared_ptr<PlatformModule> ptr;

    PlatformModule();

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
    void registerWSServlets();
    void registerRPCMethods();
};

} // namespace blog
