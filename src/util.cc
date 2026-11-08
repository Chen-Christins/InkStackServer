#include "util.h"

#include <chen/config/config.h>
#include <chen/log/log.h>
#include <chen/util/fs_util.h>
#include <chen/util/string_util.h>

namespace blog {

static chen::Logger::ptr g_logger = LOG_ROOT();

static chen::ConfigVar<std::map<std::string, std::map<std::string, std::string>>>::ptr g_mysql_dbs =
    chen::Config::Lookup("mysql.dbs", std::map<std::string, std::map<std::string, std::string>>(), "mysql dbs");

/// 解析 /proc 文件中形如 `Key:  12345 kB` 的整数值
static int64_t ParseProcValue(const std::string& content, const std::string& key)
{
    const auto lines = chen::StringUtil::Split(content, '\n');
    for (const auto& line : lines) {
        if (line.compare(0, key.size(), key) != 0) {
            continue;
        }
        const std::string value = line.substr(key.size());
        const size_t begin = value.find_first_of("0123456789");
        if (begin == std::string::npos) {
            return -1;
        }
        const size_t end = value.find_first_not_of("0123456789", begin);
        return chen::TypeUtil::Atoi(value.substr(begin, end == std::string::npos ? std::string::npos : end - begin));
    }
    return -1;
}

ProcessMemoryInfo GetProcessMemoryInfo()
{
    ProcessMemoryInfo info;
    const std::string status = chen::FSUtil::ReadFileToString("/proc/self/status");
    if (status.empty()) {
        return info;
    }
    info.rss_kb = ParseProcValue(status, "VmRSS:");
    info.vm_kb = ParseProcValue(status, "VmSize:");
    info.threads = ParseProcValue(status, "Threads:");
    return info;
}

SystemMemoryInfo GetSystemMemoryInfo()
{
    SystemMemoryInfo info;
    const std::string meminfo = chen::FSUtil::ReadFileToString("/proc/meminfo");
    if (meminfo.empty()) {
        return info;
    }
    info.total_kb = ParseProcValue(meminfo, "MemTotal:");
    info.available_kb = ParseProcValue(meminfo, "MemAvailable:");
    return info;
}

chen::IDB::ptr GetDB()
{
    const auto& dbs = g_mysql_dbs->getValue();
    if (dbs.empty()) {
        ERROR(g_logger) << "mysql.dbs is empty";
        return nullptr;
    }
    return chen::MySQLMgr::GetInstance()->get(dbs.begin()->first);
}

std::string HashPassword(const std::string& password)
{
    std::string hash = chen::EncryptorUtil::BCryptHash(password);
    if (hash.empty()) {
        ERROR(g_logger) << "bcrypt hash password failed";
    }
    return hash;
}

bool VerifyPassword(const std::string& password, const std::string& stored)
{
    if (stored.empty()) {
        return false;
    }
    if (stored.size() > 1 && stored[0] == '$' && stored[1] == '2') {
        return chen::EncryptorUtil::BCryptVerify(password, stored);
    }
    // 兼容历史无盐 MD5，登录成功后由调用方升级为 bcrypt
    return stored == chen::EncryptorUtil::MD5(password);
}

bool IsLegacyPasswordHash(const std::string& stored)
{
    if (stored.size() != 32) {
        return false;
    }
    for (char c : stored) {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
            return false;
        }
    }
    return true;
}

} // namespace blog
