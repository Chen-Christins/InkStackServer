/**
 * @file UaParser.hpp
 * @brief User-Agent 解析工具 — 设备平台判定 + 设备名称解析
 * @author Christins
 * @date 2026-08-27
 * @copyright Apache 2.0
 */
#pragma once

#include <string>

namespace blog {

/// 平台常量：移动端
constexpr const char* kPlatformMobile = "mobile";
/// 平台常量：PC 端
constexpr const char* kPlatformPc = "pc";

/**
 * @brief 判断请求所属平台（移动端 / PC 端）
 * @param ua User-Agent 字符串
 * @return kPlatformMobile 或 kPlatformPc
 */
inline const char* DetectPlatform(const std::string& ua)
{
    if (ua.empty()) {
        return kPlatformPc;
    }
    if (ua.find("iPhone") != std::string::npos || ua.find("iPad") != std::string::npos || ua.find("iPod") != std::string::npos
        || ua.find("Android") != std::string::npos || ua.find("Windows Phone") != std::string::npos || ua.find("Mobile") != std::string::npos) {
        return kPlatformMobile;
    }
    return kPlatformPc;
}

/**
 * @brief 解析浏览器名称
 * @param ua User-Agent 字符串
 * @return 浏览器名，如 Chrome / Firefox / Safari / Edge / Opera
 */
inline std::string ParseBrowser(const std::string& ua)
{
    if (ua.find("Edg/") != std::string::npos || ua.find("Edge/") != std::string::npos) {
        return "Edge";
    }
    if (ua.find("OPR/") != std::string::npos || ua.find("Opera") != std::string::npos) {
        return "Opera";
    }
    if (ua.find("Firefox/") != std::string::npos) {
        return "Firefox";
    }
    if (ua.find("Chrome/") != std::string::npos) {
        return "Chrome";
    }
    if (ua.find("Safari/") != std::string::npos) {
        return "Safari";
    }
    return "Browser";
}

/**
 * @brief 解析操作系统名称
 * @param ua User-Agent 字符串
 * @return 系统名，如 Windows / macOS / iPhone / iPad / Android / Linux
 */
inline std::string ParseOS(const std::string& ua)
{
    if (ua.find("Windows") != std::string::npos) {
        return "Windows";
    }
    if (ua.find("iPhone") != std::string::npos) {
        return "iPhone";
    }
    if (ua.find("iPad") != std::string::npos) {
        return "iPad";
    }
    if (ua.find("iPod") != std::string::npos) {
        return "iPod";
    }
    if (ua.find("Android") != std::string::npos) {
        return "Android";
    }
    if (ua.find("Mac OS X") != std::string::npos || ua.find("Macintosh") != std::string::npos) {
        return "macOS";
    }
    if (ua.find("Linux") != std::string::npos) {
        return "Linux";
    }
    return "Unknown";
}

/**
 * @brief 解析设备名称（浏览器 · 系统）
 * @param ua User-Agent 字符串
 * @return 如 "Chrome · macOS" / "Safari · iPhone" / "Edge · Windows"
 */
inline std::string ParseDeviceName(const std::string& ua)
{
    return ParseBrowser(ua) + " · " + ParseOS(ua);
}

} // namespace blog
