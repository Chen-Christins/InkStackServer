/**
 * @file ServiceBootstrap.hpp
 * @brief 服务进程公共启动/收尾逻辑
 * @author Christins
 * @date 2026-11-18
 * @copyright Apache 2.0
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <chen/db/mysql.h>
#include <chen/http/http_server.h>
#include <chen/http/ws_server.h>

namespace blog {

/**
 * @brief 获取配置中的 server.work_path
 * @return 工作目录，未配置时为空
 */
const std::string& GetWorkPath();

/**
 * @brief 服务级 onLoad 公共逻辑
 * @details 校验 work_path、安装崩溃处理器、初始化 i18n / FoxThread / Redis。
 * @param service_name 服务名称，仅用于日志
 * @return 是否初始化成功
 */
bool BootstrapOnLoad(const std::string& service_name);

/**
 * @brief 服务级 onUnload 公共逻辑
 * @details 清理 EventBus、停止 FoxThread、卸载崩溃处理器。
 */
void BootstrapOnUnload();

/**
 * @brief 连接配置中的所有 MySQL
 * @return 已连接的 MySQL 列表，任一连接失败时返回空列表
 */
std::vector<chen::MySQL::ptr> ConnectMySQL();

/**
 * @brief 加载 errors.json
 * @return 是否加载成功
 */
bool LoadErrorCodes();

/**
 * @brief 注册服务级指标
 * @param name 服务名称
 * @param version 服务版本
 * @param http_servers HTTP 服务器列表
 * @param ws_servers WebSocket 服务器列表
 */
void InitServiceMetrics(
    const std::string& name,
    const std::string& version,
    const std::vector<chen::http::HttpServer::ptr>& http_servers,
    const std::vector<chen::http::WSServer::ptr>& ws_servers);

/**
 * @brief 清理本进程过期的 Chen 会话数据
 * @param ttl_seconds 过期秒数
 */
void CleanupExpiredSessions(int64_t ttl_seconds = 3600);

} // namespace blog
