# 业务配置（4 个业务服务共用，hub 不加载）
# 所有取值来自 runenv/.env.<env>，需要按进程区分时用 <进程>_ 前缀覆盖

# 文章摘要使用的系统默认 AI
ai:
  type: ${AI_TYPE}
  model: ${AI_MODEL}
  api_key: ${AI_API_KEY}

# 默认超级管理员：启动时若该邮箱对应的管理员不存在则自动创建
admin:
  account: ${ADMIN_ACCOUNT}
  passwd: ${ADMIN_PASSWD}
  email: ${ADMIN_EMAIL}

# 分词字典与搜索索引（article 服务使用）
search:
  jieba_dict_path: ${SEARCH_DICT_PATH}
  index_path: ${SEARCH_INDEX_PATH}

# 第三方登录
oauth:
  github:
    client_id: ${GITHUB_CLIENT_ID}
    client_secret: ${GITHUB_CLIENT_SECRET}
    callback_url: ${GITHUB_CALLBACK_URL}
    frontend_base: ${FRONTEND_BASE}

# 会话与认证
auth:
  cookie_secure: ${AUTH_COOKIE_SECURE}
  cookie_same_site: ${AUTH_COOKIE_SAME_SITE}
  # 会话绝对有效期（秒），<=0 表示不限制
  session_absolute_ttl: ${AUTH_SESSION_TTL}

# WebAuthn / Passkey
webauthn:
  rp_id: ${WEBAUTHN_RP_ID}
  rp_name: ${WEBAUTHN_RP_NAME}
  origin: ${WEBAUTHN_ORIGIN}
  timeout_ms: ${WEBAUTHN_TIMEOUT_MS}
  user_verification: ${WEBAUTHN_USER_VERIFICATION}   # preferred | required
  attestation: ${WEBAUTHN_ATTESTATION}
  challenge_ttl: ${WEBAUTHN_CHALLENGE_TTL}
  user_handle_secret: ${WEBAUTHN_USER_HANDLE_SECRET}

# 实体缓存开关；关闭后所有缓存回落到 DB
cache:
  obj:
    enabled: ${CACHE_OBJ_ENABLED}
