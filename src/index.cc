#include "index.h"
#include "blog/data/article_category_rel_info.h"

#include <chen/config/config.h>
#include <chen/log/log.h>
#include <chen/util/type_util.h>
#include <chen/util/util.h>
#include <cppjieba/MixSegment.hpp>

#include <algorithm>

#include <chen/util/fs_util.h>

#include "manager/article_category_rel_manager.h"
#include "manager/article_label_rel_manager.h"
#include "manager/article_manager.h"
#include "manager/category_manager.h"
#include "manager/label_manager.h"

namespace blog {

static chen::Logger::ptr logger = LOG_ROOT();

/// 读取当前进程 RSS（KB），用于观察词典加载的内存变化
static long RssKb()
{
    std::string status = chen::FSUtil::ReadFileToString("/proc/self/status");
    size_t pos = status.find("VmRSS:");
    if (pos == std::string::npos) {
        return -1;
    }
    size_t end = status.find('\n', pos);
    std::string line = status.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
    if (line.size() <= 6) {
        return -1;
    }
    return chen::TypeUtil::Atoi(line.substr(6));
}

static chen::ConfigVar<std::string>::ptr g_jieba_dict_path =
    chen::Config::Lookup("search.jieba_dict_path", std::string(""), "jieba dict directory path");

static chen::ConfigVar<std::string>::ptr g_index_path = chen::Config::Lookup("search.index_path", std::string(""), "search index file path");

/// 索引文件格式版本。数据结构/内容口径变化时递增，旧文件会被丢弃并全量重建
static const uint32_t kIndexFileVersion = 2;

Index::Index()
    : m_createTime(0)
    , m_endTime(0)
{
}

Index::~Index() = default;

bool Index::set(uint64_t type, uint64_t key, uint32_t idx, bool v)
{
    auto b = m_indexs[type][key];
    if (!b) {
        b.reset(new chen::ds::Bitmap(m_docs.size()));
        m_indexs[type][key] = b;
    }
    b->set(idx, v);
    return true;
}

chen::ds::Bitmap::ptr Index::get(uint64_t type, uint64_t key)
{
    auto it = m_indexs.find(type);
    if (it == m_indexs.end()) {
        return nullptr;
    }
    auto itt = it->second.find(key);
    return itt == it->second.end() ? nullptr : itt->second;
}

void Index::initJieba()
{
    if (hasJieba()) {
        return;
    }
    std::string dict_path = g_jieba_dict_path->getValue();
    try {
        // Index 只使用 Cut，不需要 KeywordExtractor 的 idf/stop_words；
        // 直接使用 MixSegment 可避免加载 25 万条 IDF 词典。
        long rss_before = RssKb();
        m_jieba.reset(new cppjieba::MixSegment(dict_path + "/jieba.dict.utf8", dict_path + "/hmm_model.utf8", dict_path + "/user.dict.utf8"));
        INFO(logger) << "jieba initialized (MixSegment), RSS " << rss_before << " -> " << RssKb() << " KB, dict_path=" << dict_path;
    } catch (const std::exception& e) {
        ERROR(logger) << "jieba init failed: " << e.what();
    }
}

void Index::build()
{
    INFO(logger) << "Index build begin...";

    initJieba();

    m_createTime = time(0);
    std::vector<data::ArticleInfo::ptr> infos;
    ArticleMgr::GetInstance()->listByUserIdPages(infos, 0, 0, 0x7FFFFFFF, true, ArticleManager::PUBLISHED);
    std::sort(infos.begin(), infos.end(), [](const data::ArticleInfo::ptr a, const data::ArticleInfo::ptr b) {
        if (a->getWeight() != b->getWeight()) {
            return a->getWeight() > b->getWeight();
        }
        return a->getId() > b->getId();
    });
    m_docs.clear();
    m_indexs.clear();
    m_strings.clear();
    m_docMap.clear();
    m_docs.reserve(infos.size());
    for (auto& info : infos) {
        m_docs.emplace_back(info->getId());
    }
    for (size_t i = 0; i < infos.size(); ++i) {
        m_docMap[infos[i]->getId()] = i;
        buildIdx(infos[i], i);
    }
    m_endTime = time(0);
    m_isReady.store(true);
    INFO(logger) << "Index build over... used=" << (m_endTime - m_createTime) << " doc.size=" << m_docs.size();
}

void Index::buildIdx(data::ArticleInfo::ptr info, uint32_t idx)
{
    set((uint64_t)IndexType::USER_ID, info->getUserId(), idx, true);
    set((uint64_t)IndexType::STATE, info->getState(), idx, true);
    set((uint64_t)IndexType::YEAR_MON, hash(chen::TimeUtil::Time2Str(info->getPublishTime(), "%Y年%m月"), true), idx, true);
    set((uint64_t)IndexType::CHANNEL, info->getChannel(), idx, true);

    // 中文分词全文索引（正文只取前 2000 字，长文不影响搜索命中且大幅减少分词耗时）
    buildWordIdx(info->getTitle(), idx);
    // buildWordIdx(info->getContent().substr(0, 2000), idx);
    buildWordIdx(info->getContent(), idx);

    std::vector<data::ArticleCategoryRelInfo::ptr> cats;
    ArticleCategoryRelMgr::GetInstance()->listByArticleId(cats, info->getId(), true);
    for (auto& i : cats) {
        auto cat = CategoryMgr::GetInstance()->get(i->getCategoryId());
        if (!cat) {
            ERROR(logger) << "invalid cat_id=" << i->getCategoryId();
            continue;
        }
        set((uint64_t)IndexType::CAT_ID, cat->getId(), idx, true);
        set((uint64_t)IndexType::CAT_NAME, hash(cat->getName().c_str(), true), idx, true);
    }

    std::vector<data::ArticleLabelRelInfo::ptr> labels;
    ArticleLabelRelMgr::GetInstance()->listByArticleId(labels, info->getId(), true);
    for (auto& i : labels) {
        auto label = LabelMgr::GetInstance()->get(i->getLabelId());
        if (!label) {
            ERROR(logger) << "invalid label_id=" << i->getLabelId();
            continue;
        }
        set((uint64_t)IndexType::LABEL_ID, label->getId(), idx, true);
        set((uint64_t)IndexType::LABEL_NAME, hash(label->getName().c_str(), true), idx, true);
    }
}

int32_t Index::search(std::vector<uint64_t>& ids, const std::map<uint64_t, std::set<uint64_t>>& params, uint32_t max_size)
{
    if (!isReady()) {
        WARN(logger) << "Index not initialized";
        return -1;
    }
    auto v = query(params);
    if (!v) {
        return -1;
    }
    uint32_t i = 0;
    for (auto it = v->begin_new(); !*it && i < max_size; it->next(), ++i) {
        ids.emplace_back(m_docs[**it]);
    }
    return v->getCount();
}

int32_t Index::property(
    std::map<uint64_t, std::map<uint64_t, uint64_t>>& props,
    const std::map<uint64_t, std::set<uint64_t>>& params,
    std::map<uint64_t, std::set<uint64_t>>& querys)
{
    // TODO: Index::property
    return -1;
}

uint64_t Index::StrHash(const std::string& str)
{
    return chen::EncryptorUtil::Murmur3_64(chen::StringUtil::ToLower(str).c_str());
}

std::string Index::toString()
{
    // TODO: Index::toString
    return "";
}

std::string Index::getStr(uint64_t id)
{
    auto it = m_strings.find(id);
    return it == m_strings.end() ? "" : it->second;
}

chen::ds::Bitmap::ptr Index::query(const std::map<uint64_t, std::set<uint64_t>>& params)
{
    if (params.empty()) {
        return nullptr;
    }

    chen::ds::Bitmap::ptr result;

    for (auto& [type, keys] : params) {
        if (keys.empty()) {
            continue;
        }

        // 同 type 内：各 key 的 Bitmap 做 OR
        chen::ds::Bitmap::ptr type_result;
        for (auto key : keys) {
            auto b = get(type, key);
            if (!b) {
                continue;
            }
            if (!type_result) {
                type_result.reset(new chen::ds::Bitmap(*b));
            } else {
                *type_result |= *b;
            }
        }

        if (!type_result) {
            // 该 type 下无任何命中，整体无结果
            return nullptr;
        }

        // 不同 type 间：做 AND
        if (!result) {
            result = type_result;
        } else {
            *result &= *type_result;
            if (!result->any()) {
                return nullptr;
            }
        }
    }

    return result;
}

uint64_t Index::hash(const std::string& str, bool save)
{
    auto v = StrHash(str);
    if (save) {
        m_strings.insert(std::pair(v, str));
    }
    return v;
}

void Index::buildWordIdx(const std::string& str, uint32_t idx)
{
    if (!hasJieba() || str.empty()) {
        return;
    }
    std::vector<std::string> words;
    m_jieba->Cut(str, words, true);
    std::set<uint64_t> seen;
    for (auto& w : words) {
        if (w.size() < 2) {
            continue;
        }
        auto h = hash(w, true);
        if (seen.insert(h).second) {
            set((uint64_t)IndexType::WORD, h, idx, true);
        }
    }
}

void Index::cutWord(const std::string& str, std::vector<std::string>& words)
{
    if (hasJieba()) {
        m_jieba->Cut(str, words, true);
    }
}

int32_t Index::getIdx(uint64_t article_id)
{
    auto it = m_docMap.find(article_id);
    return it == m_docMap.end() ? -1 : it->second;
}

void Index::addArticle(data::ArticleInfo::ptr info)
{
    if (!info) {
        return;
    }

    // 已存在则先移除旧索引
    int32_t old = getIdx(info->getId());
    if (old >= 0) {
        removeArticle(info->getId());
    }

    uint32_t idx = m_docs.size();
    m_docs.push_back(info->getId());
    m_docMap[info->getId()] = idx;

    // 将已有位图全部扩容到新 m_docs.size()，避免 query() 中 &= 时 m_size 不一致崩溃
    uint32_t new_size = m_docs.size();
    for (auto& [type, keyMap] : m_indexs) {
        for (auto& [key, bitmap] : keyMap) {
            bitmap->resize(new_size, false);
        }
    }

    buildIdx(info, idx);

    std::string path = g_index_path->getValue();
    if (!path.empty()) {
        save(path);
    }
}

void Index::removeArticle(uint64_t article_id)
{
    int32_t idx = getIdx(article_id);
    if (idx < 0) {
        return;
    }

    // 遍历所有类型的位图，将该文章对应的 bit 置为 false
    for (auto& [type, keyMap] : m_indexs) {
        for (auto& [key, bitmap] : keyMap) {
            bitmap->set(idx, false);
        }
    }

    m_docMap.erase(article_id);

    std::string path = g_index_path->getValue();
    if (!path.empty()) {
        save(path);
    }
}

void Index::updateArticle(data::ArticleInfo::ptr info)
{
    if (!info) {
        return;
    }
    removeArticle(info->getId());
    addArticle(info);
}

bool Index::save(const std::string& path)
{
    if (!isReady()) {
        ERROR(logger) << "index not ready, skip save";
        return false;
    }
    chen::ByteArray::ptr ba(new chen::ByteArray);
    ba->writeUint32(0x42494458); // magic 'BIDX'
    ba->writeUint32(kIndexFileVersion);
    ba->writeInt64(m_docs.size());
    for (auto id : m_docs) {
        ba->writeInt64(id);
    }
    ba->writeUint32(m_docMap.size());
    for (auto& [aid, pos] : m_docMap) {
        ba->writeStringF16(std::to_string(aid));
        ba->writeUint32(pos);
    }
    ba->writeUint32(m_strings.size());
    for (auto& [h, s] : m_strings) {
        ba->writeInt64(h);
        ba->writeStringF16(s);
    }
    ba->writeUint32(m_indexs.size());
    for (auto& [type, keyMap] : m_indexs) {
        ba->writeInt64(type);
        ba->writeUint32(keyMap.size());
        for (auto& [key, bitmap] : keyMap) {
            ba->writeInt64(key);
            size_t pos_before = ba->getPosition();
            ba->writeUint32(0); // placeholder
            bitmap->writeTo(ba);
            size_t pos_after = ba->getPosition();
            ba->setPosition(pos_before);
            ba->writeUint32(pos_after - pos_before - 4);
            ba->setPosition(pos_after);
        }
    }
    ba->setPosition(0);
    if (!ba->writeToFile(path)) {
        ERROR(logger) << "failed to write index file: " << path;
        return false;
    }
    INFO(logger) << "index saved to " << path << ", docs=" << m_docs.size();
    return true;
}

bool Index::load(const std::string& path)
{
    // 先检查文件是否存在，避免 ByteArray::readFromFile 打 ERROR 日志
    if (!chen::FSUtil::Exists(path)) {
        INFO(logger) << "no index file at " << path << ", will build from scratch";
        return false;
    }
    chen::ByteArray::ptr ba(new chen::ByteArray);
    if (!ba->readFromFile(path)) {
        ERROR(logger) << "failed to read index file: " << path;
        return false;
    }
    ba->setPosition(0);
    if (ba->getSize() < 12) {
        return false;
    }
    uint32_t magic = ba->readUint32();
    if (magic != 0x42494458) {
        ERROR(logger) << "invalid index file magic";
        return false;
    }
    uint32_t version = ba->readUint32();
    if (version != kIndexFileVersion) {
        WARN(logger) << "index file version mismatch, file=" << version << " expect=" << kIndexFileVersion << ", rebuild from scratch";
        return false;
    }

    initJieba();

    m_docs.clear();
    m_docMap.clear();
    m_strings.clear();
    m_indexs.clear();

    int64_t doc_count = ba->readInt64();
    m_docs.reserve(doc_count);
    for (int64_t i = 0; i < doc_count; ++i) {
        m_docs.push_back(ba->readInt64());
    }

    uint32_t docmap_size = ba->readUint32();
    for (uint32_t i = 0; i < docmap_size; ++i) {
        std::string key_str = ba->readStringF16();
        int64_t aid = chen::TypeUtil::Atoi(key_str);
        uint32_t pos = ba->readUint32();
        m_docMap[aid] = pos;
    }

    uint32_t string_count = ba->readUint32();
    for (uint32_t i = 0; i < string_count; ++i) {
        int64_t h = ba->readInt64();
        std::string s = ba->readStringF16();
        m_strings[h] = s;
    }

    uint32_t type_count = ba->readUint32();
    for (uint32_t ti = 0; ti < type_count; ++ti) {
        uint64_t type = ba->readInt64();
        uint32_t key_count = ba->readUint32();
        for (uint32_t ki = 0; ki < key_count; ++ki) {
            uint64_t key = ba->readInt64();
            ba->readUint32(); // bm_size, consumed by readFrom below
            auto bm = std::make_shared<chen::ds::Bitmap>(0);
            if (!bm->readFrom(ba)) {
                ERROR(logger) << "failed to read bitmap type=" << type << " key=" << key;
                return false;
            }
            m_indexs[type][key] = bm;
        }
    }

    m_isReady.store(true);
    INFO(logger) << "index loaded from " << path << ", docs=" << m_docs.size();
    return true;
}

bool Index::initFromFile()
{
    if (g_index_path->getValue().empty()) {
        ERROR(logger) << "index path is empty";
        return false;
    }

    std::string index_path = g_index_path->getValue();
    if (!load(index_path)) {
        WARN(logger) << "index load failed, building index from " << index_path;
        build();
        save(index_path);
        return false;
    }

    return true;
}

} // namespace blog
