/**
 * @file Index.hpp
 * @brief 对文章、分类和标签等事情进行索引
 * @author Christins
 * @date 2025-08-30
 * @copyright Apache 2.0
 */
#pragma once

#include <atomic>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <chen/bytearray/bytearray.h>
#include <chen/ds/bitmap.h>
#include <chen/util/singleton.h>

#include "blog/data/article_info.h"

namespace cppjieba {
class MixSegment;
}

namespace blog {

enum class IndexType {
    USER_ID = 1,
    CAT_ID = 2,
    LABEL_ID = 3,
    CAT_NAME = 4,
    LABEL_NAME = 5,
    STATE = 6,
    YEAR_MON = 7,
    CHANNEL = 8,
    WORD = 100
};

class Index {
public:
    typedef std::shared_ptr<Index> ptr;

    Index();

    ~Index();

    bool set(uint64_t type, uint64_t key, uint32_t idx, bool v);

    chen::ds::Bitmap::ptr get(uint64_t type, uint64_t key);

    void build();

    void buildIdx(data::ArticleInfo::ptr info, uint32_t idx);

    /**
     * @brief 增量添加文章到索引
     */
    void addArticle(data::ArticleInfo::ptr info);

    /**
     * @brief 增量从索引中移除文章
     */
    void removeArticle(uint64_t article_id);

    /**
     * @brief 增量更新文章索引（先删后加）
     */
    void updateArticle(data::ArticleInfo::ptr info);

    int32_t search(std::vector<uint64_t>& ids, const std::map<uint64_t, std::set<uint64_t>>& params, uint32_t max_size);

    int32_t property(
        std::map<uint64_t, std::map<uint64_t, uint64_t>>& props,
        const std::map<uint64_t, std::set<uint64_t>>& params,
        std::map<uint64_t, std::set<uint64_t>>& querys);

    static uint64_t StrHash(const std::string& str);

    /// 索引是否就绪（可搜索）
    bool isReady() const { return m_isReady.load(); }

    /// 分词器是否就绪
    bool hasJieba() const { return m_jieba != nullptr; }

    /// 对输入做中文分词
    void cutWord(const std::string& str, std::vector<std::string>& words);

    /// 持久化索引到文件
    bool save(const std::string& path);

    /// 从文件加载索引
    bool load(const std::string& path);

    std::string toString();

    std::string getStr(uint64_t id);

    bool initFromFile();

private:
    chen::ds::Bitmap::ptr query(const std::map<uint64_t, std::set<uint64_t>>& params);

    uint64_t hash(const std::string& str, bool save);

    void buildWordIdx(const std::string& str, uint32_t idx);

    /// 根据 article_id 查找在 m_docs 中的位置
    int32_t getIdx(uint64_t article_id);

    /// 初始化 jieba 分词器
    void initJieba();

private:
    uint64_t m_createTime;

    uint64_t m_endTime;

    std::vector<uint64_t> m_docs;
    /// article_id → m_docs 下标
    std::unordered_map<uint64_t, uint32_t> m_docMap;

    std::map<uint64_t, std::map<uint64_t, chen::ds::Bitmap::ptr>> m_indexs;

    std::unordered_map<uint64_t, std::string> m_strings;
    /// 索引是否已就绪
    std::atomic<bool> m_isReady{false};
    /// 分词器（PIMPL，避免头文件污染）。只加载分词所需词典，不加载未使用的 IDF
    std::unique_ptr<cppjieba::MixSegment> m_jieba;
};

typedef chen::Singleton<Index> IndexMgr;

} // namespace blog
