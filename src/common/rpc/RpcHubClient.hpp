/**
 * @file RpcHubClient.hpp
 * @brief 到 RPC hub 的长连接：注册自身服务身份、接收转发请求、发起跨服务调用
 * @author Christins
 * @date 2026-11-18
 * @copyright Apache 2.0
 */
#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <type_traits>

#include <chen/rpc/rpc_client.h>
#include <chen/timer/timer.h>
#include <chen/util/singleton.h>

#include "common/rpc/RpcEndpoint.hpp"

namespace blog {
namespace rpc {

/**
 * @brief hub 连接客户端（每个 worker 进程一份）
 * @details 一个进程只维持一条到 hub 的连接：
 *          - 注册服务身份（registerService），供 hub 按 func_id 转发请求过来；
 *          - 在该连接上注册 CmdID handler（registerMethod），接收转发请求；
 *          - 复用同一连接发起 callRouted / notifyRouted 调用。
 *
 *          断线后由定时器自动重连，并按保存的注册表重新注册 handler，
 *          所以业务代码可以在连接建立之前就注册 handler。
 *
 * @note 所有模板方法都可能被业务在任意协程中调用，内部只依赖 RpcClient 自身的互斥量。
 */
class RpcHubClient {
public:
    RpcHubClient() = default;
    ~RpcHubClient() = default;

    /**
     * @brief 连接 hub 并注册本服务
     * @param func_id 本服务的功能类型（见 FuncId）
     * @return 是否连接并注册成功（失败只记日志，由重连定时器兜底）
     */
    bool start(uint32_t func_id);

    /**
     * @brief 注销 handler、断开连接、停止重连定时器
     */
    void stop();

    /**
     * @brief 连接断开时重连（由定时器与 onTick 调用）
     * @return 当前是否已连接
     */
    bool ensureConnected();

    /**
     * @brief 当前连接是否可用
     */
    bool isConnected();

    /**
     * @brief 注册 CmdID handler
     * @param cmd 命令号（非 0）
     * @param func 本地实现，签名形如 R(Args...)
     * @note 连接未建立时只登记，建立后自动下发
     */
    template <typename Func>
    void registerMethod(uint32_t cmd, Func func)
    {
        std::lock_guard lock(m_mutex);
        m_registrars[cmd] = [cmd, func](const chen::rpc::RpcClient::ptr& client) {
            client->registerMethod(cmd, func);
        };
        if (m_client && m_client->isConnected()) {
            m_client->registerMethod(cmd, func);
        }
    }

    /**
     * @brief 调用远端方法，失败时返回 fallback
     * @tparam R 返回值类型（不可为 void）
     * @param func_id 目标功能类型
     * @param cmd 命令号
     * @param fallback 调用失败时返回的值
     * @param args 参数
     */
    template <typename R, typename... Args>
    R callOr(uint32_t func_id, uint32_t cmd, R fallback, Args... args)
    {
        R out{};
        if (!tryCall(func_id, cmd, out, args...)) {
            return fallback;
        }
        return out;
    }

    /**
     * @brief 调用远端方法
     * @param[out] out 返回值
     * @return 是否成功（含超时、未连接、服务端非 0 错误码）
     */
    template <typename R, typename... Args>
    bool tryCall(uint32_t func_id, uint32_t cmd, R& out, Args... args)
    {
        auto client = connectedClient();
        if (!client) {
            logCallFailure(func_id, cmd, "hub not connected");
            return false;
        }
        try {
            if constexpr (std::is_void_v<R>) {
                client->callRouted<R>(RouteTo(func_id), cmd, args...);
            } else {
                out = client->callRouted<R>(RouteTo(func_id), cmd, args...);
            }
            return true;
        } catch (const std::exception& e) {
            logCallFailure(func_id, cmd, e.what());
            return false;
        } catch (...) {
            logCallFailure(func_id, cmd, "unknown error");
            return false;
        }
    }

    /**
     * @brief 单向通知（fire-and-forget），按 func_id 路由
     */
    template <typename... Args>
    bool notify(uint32_t func_id, uint32_t cmd, Args... args)
    {
        return sendNotify(RouteTo(func_id), func_id, cmd, args...);
    }

    /**
     * @brief 单向广播到 func_id 下全部实例
     */
    template <typename... Args>
    bool broadcast(uint32_t func_id, uint32_t cmd, Args... args)
    {
        return sendNotify(BroadcastTo(func_id), func_id, cmd, args...);
    }

private:
    /**
     * @brief 建立连接并注册服务，随后下发已登记的 handler
     * @note 调用方需持有 m_mutex
     */
    bool connectLocked();

    /**
     * @brief 获取可用连接（不触发重连）
     */
    chen::rpc::RpcClient::ptr connectedClient();

    /**
     * @brief 重连定时器回调
     */
    void onReconnectTimer();

    /**
     * @brief 记录调用失败日志
     */
    void logCallFailure(uint32_t func_id, uint32_t cmd, const std::string& why);

    template <typename... Args>
    bool sendNotify(const chen::rpc::RpcRouting& routing, uint32_t func_id, uint32_t cmd, Args... args)
    {
        auto client = connectedClient();
        if (!client) {
            logCallFailure(func_id, cmd, "hub not connected");
            return false;
        }
        try {
            return client->notifyRouted(routing, cmd, args...);
        } catch (const std::exception& e) {
            logCallFailure(func_id, cmd, e.what());
            return false;
        } catch (...) {
            logCallFailure(func_id, cmd, "unknown error");
            return false;
        }
    }

private:
    mutable std::mutex m_mutex;
    chen::rpc::RpcClient::ptr m_client;
    chen::Timer::ptr m_reconnect_timer;
    std::map<uint32_t, std::function<void(const chen::rpc::RpcClient::ptr&)>> m_registrars;
    uint32_t m_func_id = 0;
    uint32_t m_peer_id = 0;
    uint64_t m_last_connect_ms = 0;
};

typedef chen::LeakySingleton<RpcHubClient> RpcHubClientMgr;

} // namespace rpc
} // namespace blog
