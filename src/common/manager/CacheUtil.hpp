/**
 * @file CacheUtil.hpp
 * @brief Redis 缓存工具（COUNT 缓存、列表结果缓存）
 * @author Christins
 * @date 2026-06-06
 * @copyright Apache 2.0
 */
#pragma once

#include <chen/db/query_builder.h>

#include <json/json.h>

namespace blog {

/// COUNT 缓存 TTL（秒）
static const int kCountCacheTTL = 30;
/// 列表结果缓存 TTL（秒）
static const int kListCacheTTL = 10;
/// Stats 缓存 TTL（秒）
static const int kStatsCacheTTL = 60;
/// ID 映射缓存 TTL（秒）— alternate_key → id
static const int kIdMapCacheTTL = 300;

/**
 * @brief 执行 COUNT 查询，优先从 Redis 缓存读取
 * @param qb QueryBuilder（已配置好 WHERE/JOIN）
 * @param conn 数据库连接
 * @param cacheKey 缓存键（不含前缀）
 * @param ttlSec 缓存 TTL 秒数
 * @return COUNT 结果，失败返回 0
 */
int64_t executeCountCached(chen::QueryBuilder::ptr qb, chen::IDB::ptr conn, const std::string& cacheKey, int ttlSec = kCountCacheTTL);

/**
 * @brief 缓存分页列表结果（ID 数组）
 * @param cacheKey 缓存键
 * @param ids 文章/评论 ID 列表
 * @param ttlSec TTL 秒数
 */
void cacheListResult(const std::string& cacheKey, const std::vector<int64_t>& ids, int ttlSec = kListCacheTTL);

/**
 * @brief 读取缓存的分页列表结果
 * @param cacheKey 缓存键
 * @param[out] ids 解析后的 ID 列表
 * @return true 表示命中缓存
 */
bool getCachedListResult(const std::string& cacheKey, std::vector<int64_t>& ids);

/**
 * @brief 缓存字符串结果
 * @param cacheKey 缓存键
 * @param value 字符串值
 * @param ttlSec TTL 秒数
 */
void cacheStringResult(const std::string& cacheKey, const std::string& value, int ttlSec = kStatsCacheTTL);

/**
 * @brief 读取缓存的字符串结果
 * @param cacheKey 缓存键
 * @param[out] value 缓存的字符串
 * @return true 表示命中缓存
 */
bool getCachedStringResult(const std::string& cacheKey, std::string& value);

/**
 * @brief 缓存 alternate_key → id 映射
 * @param cacheKey 缓存键（不含前缀）
 * @param id 主键 ID
 * @param ttlSec TTL 秒数
 */
void cacheIdMapping(const std::string& cacheKey, int64_t id, int ttlSec = kIdMapCacheTTL);

/**
 * @brief 读取 alternate_key → id 映射
 * @param cacheKey 缓存键（不含前缀）
 * @return ID，未命中返回 0
 */
int64_t getCachedIdMapping(const std::string& cacheKey);

/**
 * @brief Redis 对象缓存是否开启（cache.obj.enabled）
 */
bool isObjectCacheEnabled();

/**
 * @brief 读取 Redis 字符串（各 Manager 对象缓存的基础操作）
 * @param key 完整 key
 * @return 值，未命中/失败/缓存关闭时返回空串
 */
std::string redisCacheGet(const std::string& key);

/**
 * @brief 写入 Redis 字符串并设置 TTL（各 Manager 对象缓存的基础操作）
 * @param key 完整 key
 * @param value 值
 * @param ttlSec TTL 秒数，<= 0 表示不设置过期
 * @return true 成功
 */
bool redisCacheSetex(const std::string& key, const std::string& value, int ttlSec);

/**
 * @brief 删除 Redis key（各 Manager 对象缓存的基础操作）
 * @return true 删除成功（key 存在）
 */
bool redisCacheDel(const std::string& key);

/**
 * @brief 判断 Redis key 是否存在（各 Manager 对象缓存的基础操作）
 */
bool redisCacheExists(const std::string& key);

/**
 * @brief 从缓存 JSON 中读取 int64（兼容字符串/数字）
 */
int64_t cacheJsonInt64(const Json::Value& v, const char* key);

/**
 * @brief 从缓存 JSON 中读取 int32
 */
int32_t cacheJsonInt32(const Json::Value& v, const char* key);

/**
 * @brief 从缓存 JSON 中读取 string
 */
std::string cacheJsonString(const Json::Value& v, const char* key);

/**
 * @brief 从缓存 JSON 中读取时间戳（兼容 "YYYY-MM-DD HH:MM:SS" 与数字）
 */
int64_t cacheJsonTime(const Json::Value& v, const char* key);

} // namespace blog
