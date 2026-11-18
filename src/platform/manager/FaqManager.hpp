#pragma once

#include "blog/data/help_faqs_info.h"

#include <chen/util/singleton.h>

namespace blog {

class FaqManager {
public:
    FaqManager();

    bool listAll(std::vector<data::HelpFaqsInfo::ptr>& infos);

    bool searchByKeyword(const std::string& keyword, std::vector<data::HelpFaqsInfo::ptr>& infos);

    /**
     * @brief 软删除 FAQ（设置 is_deleted = 1）
     * @param id FAQ ID
     * @return true 成功，false 失败
     */
    bool remove(int64_t id);

    /**
     * @brief 更新 FAQ
     * @param id FAQ ID
     * @param q 问题
     * @param a 答案
     * @param cat 分类
     * @return true 成功，false 失败
     */
    bool update(int64_t id, const std::string& q, const std::string& a, const std::string& cat);
};

typedef chen::Singleton<FaqManager> FaqMgr;

} // namespace blog
