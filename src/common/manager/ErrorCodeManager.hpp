/**
 * @file ErrorCodeManager.hpp
 * @brief 错误码管理器 - 加载 errors.json 到内存，提供按语言查询
 * @author Christins
 * @date 2026-06-11
 * @copyright Apache 2.0
 */
#pragma once

#include <map>
#include <memory>
#include <string>

#include <chen/util/singleton.h>

namespace blog {

class ErrorCodeManager {
public:
    typedef std::shared_ptr<ErrorCodeManager> ptr;

    ErrorCodeManager();

    /**
     * @brief 从 errors.json 加载错误码配置到内存
     * @param configPath errors.json 的绝对路径
     * @return 是否加载成功
     */
    bool load(const std::string& configPath);

    /**
     * @brief 重新加载（用于热更新）
     */
    bool reload();

    /**
     * @brief 获取指定错误码的默认语言消息
     * @param errCode 错误码
     * @return 错误消息（未找到返回 "Unknown error"）
     */
    std::string getMessage(int32_t errCode) const;

    /**
     * @brief 获取指定错误码的指定语言消息
     * @param errCode 错误码
     * @param lang 语言代码 (zh/en)
     * @return 错误消息
     */
    std::string getMessage(int32_t errCode, const std::string& lang) const;

    /**
     * @brief 获取所有错误码的指定语言映射
     * @param lang 语言代码 (zh/en)
     * @return errno → message 的映射
     */
    std::map<int32_t, std::string> getAllMessages(const std::string& lang) const;

    /**
     * @brief 序列化错误码映射为 JSON 字符串
     * @param lang 语言代码
     * @return JSON 字符串
     */
    std::string toJson(const std::string& lang) const;

    /**
     * @brief 获取配置版本号
     */
    const std::string& getVersion() const { return m_version; }

    /**
     * @brief 获取错误码总数
     */
    size_t getCount() const { return m_zhMessages.size(); }

private:
    std::string m_configPath;
    std::string m_version;
    std::map<int32_t, std::string> m_keys;       // errno → 翻译 key
    std::map<int32_t, std::string> m_zhMessages; // errno → 中文消息
    std::map<int32_t, std::string> m_enMessages; // errno → 英文消息
};

typedef chen::Singleton<ErrorCodeManager> ErrorCodeMgr;

} // namespace blog
