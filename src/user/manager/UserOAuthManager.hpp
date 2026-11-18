/**
 * @file UserOAuthManager.hpp
 * @brief 第三方登录绑定管理器 — 按 provider + open_id 查询与绑定
 * @author Christins
 * @date 2026-10-11
 * @copyright Apache 2.0
 */
#pragma once

#include "blog/data/user_oauth_info.h"

#include <chen/db/query_builder.h>
#include <chen/util/singleton.h>

namespace blog {

class UserOAuthManager {
public:
    UserOAuthManager();

    /**
     * @brief 按第三方平台唯一标识查询绑定记录
     * @return 绑定记录，不存在返回 nullptr
     */
    data::UserOauthInfo::ptr getByOpenId(const std::string& provider, const std::string& open_id);

    /**
     * @brief 查询某用户在某平台的绑定记录
     * @return 绑定记录，不存在返回 nullptr
     */
    data::UserOauthInfo::ptr getByUser(int64_t user_id, const std::string& provider);

    /**
     * @brief 绑定第三方账号到本地用户
     * @return 0 成功，负数失败
     */
    int bind(int64_t user_id, const std::string& provider, const std::string& open_id, const std::string& login_name);
};

typedef chen::Singleton<UserOAuthManager> UserOAuthMgr;

} // namespace blog
