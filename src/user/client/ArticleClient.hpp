/**
 * @file ArticleClient.hpp
 * @brief 文章服务 RPC 客户端（user 服务调用方）
 * @author Christins
 * @date 2026-11-18
 * @copyright Apache 2.0
 */
#pragma once

#include <cstdint>
#include <vector>

#include <chen/util/singleton.h>

#include "protocol_article.h" // IWYU pragma: keep

namespace blog {
namespace client {

/**
 * @brief 文章服务客户端
 * @details 只做跨进程 RPC 调用，不 include 文章服务的实现头文件。
 */
class ArticleClient {
public:
    /**
     * @brief 统计用户文章数
     * @param uid 用户 id
     * @param include_deleted 是否包含已删除文章（与原 listByUserId(valid=false) 对齐）
     * @return 文章数，调用失败时返回 0
     */
    int64_t countByUser(int64_t uid, bool include_deleted = false);

    /**
     * @brief 获取文章简介
     * @param id 文章 id
     * @return 文章简介；不存在时 Id 为 0
     */
    tagArticleBrief getArticleBrief(int64_t id);

    /**
     * @brief 批量获取文章简介
     * @param ids 文章 id 列表
     * @return 存在的文章简介
     */
    std::vector<tagArticleBrief> getArticleBriefs(const std::vector<int64_t>& ids);
};

typedef chen::Singleton<ArticleClient> ArticleClientMgr;

} // namespace client
} // namespace blog
