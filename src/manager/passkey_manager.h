/**
 * @file passkey_manager.h
 * @brief 用户 Passkey 凭据管理器
 * @author Christins
 * @date 2026-10-23
 * @copyright Apache 2.0
 */
#pragma once

#include "blog/data/user_passkey_info.h"

#include <chen/db/query_builder.h>
#include <chen/util/singleton.h>

namespace blog {

class PasskeyManager {
public:
    /**
     * @brief 按 credential ID 查询（不返回已删除记录）
     */
    data::UserPasskeyInfo::ptr getByCredentialId(const std::string& credential_id);

    /**
     * @brief 按主键查询（不返回已删除记录）
     */
    data::UserPasskeyInfo::ptr get(int64_t id);

    /**
     * @brief 列出用户有效 Passkey
     */
    int listByUser(std::vector<data::UserPasskeyInfo::ptr>& results, int64_t uid);

    /**
     * @brief 新增 Passkey；同一用户曾软删除同一 credential 时复用旧行
     * @return 0 成功，负数失败
     */
    int add(data::UserPasskeyInfo::ptr info);

    /**
     * @brief 更新 Passkey
     * @return 0 成功，负数失败
     */
    int update(data::UserPasskeyInfo::ptr info);

    /**
     * @brief 软删除 Passkey
     * @return 0 成功，负数失败
     */
    int remove(int64_t id);
};

typedef chen::Singleton<PasskeyManager> PasskeyMgr;

} // namespace blog
