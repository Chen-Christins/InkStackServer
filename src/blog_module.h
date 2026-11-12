/**
 * @file blog_module.h
 * @brief 项目模块
 * @author Christins
 * @date 2025-05-10
 * @copyright Apache 2.0
 */
#pragma once

#include <chen/module/module.h>

namespace blog {

class BlogModule : public chen::Module {
public:
    typedef std::shared_ptr<BlogModule> ptr;
    /**
     * @brief 构造函数
     */
    BlogModule();

    /**
     * @brief 模块加载
     * @return bool
     */
    bool onLoad() override;

    /**
     * @brief 模块卸载：worker 退出前最后调用（热重载/关机）
     * @details 在 onDeactivate 之后、_exit 之前执行；用于释放本进程资源。
     *          正常热重载不会在本进程内 dlclose 模块，进程退出后地址空间由内核回收。
     * @return bool
     */
    bool onUnload() override;

    /**
     * @brief 定时器回调
     */
    void onTick() override;

    /**
     * @brief 设置定时器间隔
     * @return uint64_t
     */
    uint64_t getTickIntervalMs() override;

    /**
     * @brief 服务器就绪
     * @return bool
     */
    bool onServerReady() override;

    /**
     * @brief 服务器启动
     * @return bool
     */
    bool onServerUp() override;

    /**
     * @brief 模块停用：worker 进入 drain 时调用（热重载/关机）
     * @details 热重载场景下新 worker 已接管监听，当前 worker 已停止 accept。
     *          用于 flush 数据、关闭本进程的长连接等收尾工作。
     *          这是多进程模型：本方法只影响当前 worker，不会操作新 worker 中的模块。
     * @return bool
     */
    bool onDeactivate() override;

private:
    /**
     * @brief 初始化数据库
     */
    static bool initMySQL();

    /**
     * @brief 注册Servlet
     */
    void registerServlets();

    /**
     * @brief 注册WebSocket Servlet
     */
    void registerWSServlets();

    /**
     * @brief 注册RPC方法
     */
    void registerRPCMethods();
};

} // namespace blog
