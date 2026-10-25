/**
 * @file error_codes.h
 * @brief 统一业务错误码常量定义
 * @author Christins
 * @date 2026-06-11
 * @copyright Apache 2.0
 *
 * 错误码格式: XXYYY (5位数字)
 *   XX  = 模块号 (00=成功, 01=通用, 02=用户, 03=文章, ...)
 *   YYY = 模块内错误序号 (001-999)
 *
 * 与 errors.json 保持同步
 */
#pragma once

#include <cstdint>

namespace blog {
namespace errcode {

// ==================== 成功 ====================
constexpr int32_t SUCCESS = 0;

// ==================== 模块01: 通用错误 (01001-01999) ====================
// 参数相关 (01001-01099)
constexpr int32_t PARAM_MISSING = 1001;  // 缺少必填参数
constexpr int32_t PARAM_INVALID = 1002;  // 参数格式错误
constexpr int32_t INVALID_METHOD = 1003; // 不支持的请求方法

// 认证/权限相关 (01101-01199)
constexpr int32_t NOT_LOGIN = 1101;         // 未登录
constexpr int32_t ACCESS_DENIED = 1102;     // 权限不足
constexpr int32_t ACCOUNT_INVALID = 1103;   // 账号状态异常
constexpr int32_t AUTH_CODE_INVALID = 1104; // 验证码错误

// ==================== 模块02: 用户模块 (02001-02999) ====================
constexpr int32_t USER_NOT_FOUND = 2001;          // 用户不存在
constexpr int32_t USER_ACCOUNT_EXISTS = 2002;     // 账号已存在
constexpr int32_t USER_EMAIL_EXISTS = 2003;       // 邮箱已注册
constexpr int32_t USER_INVALID_ACCOUNT = 2004;    // 账号格式错误
constexpr int32_t USER_INVALID_EMAIL = 2005;      // 邮箱格式错误
constexpr int32_t USER_INVALID_PASSWORD = 2006;   // 密码格式错误
constexpr int32_t USER_PASSWORD_WRONG = 2007;     // 密码错误
constexpr int32_t USER_OLD_PASSWORD_WRONG = 2008; // 旧密码错误
constexpr int32_t USER_ALREADY_DELETED = 2009;    // 用户已删除
constexpr int32_t USER_ALREADY_LOGIN = 2010;      // 已登录
constexpr int32_t USER_EMAIL_NOT_REGISTER = 2011; // 邮箱未注册
constexpr int32_t USER_NO_PARAM = 2012;           // 缺少更新参数
constexpr int32_t USER_PASSWORDS_DIFFER = 2013;   // 两次密码不一致
constexpr int32_t USER_CANNOT_FOLLOW_SELF = 2014; // 不能关注自己
constexpr int32_t USER_NOT_FOLLOWING = 2015;      // 未关注该用户
constexpr int32_t USER_FOLLOW_FAILED = 2016;      // 关注操作失败
constexpr int32_t SEND_CODE_FREQUENT = 2017;      // 验证码发送过于频繁
constexpr int32_t DEVICE_NOT_FOUND = 2018;        // 登录设备不存在
constexpr int32_t DEVICE_KICK_FAILED = 2019;      // 设备下线失败
constexpr int32_t TOTP_INVALID_CODE = 2020;       // TOTP验证码错误
constexpr int32_t TOTP_NOT_ENABLED = 2021;        // 2FA未启用
constexpr int32_t TOTP_ALREADY_ENABLED = 2022;    // 2FA已启用
constexpr int32_t TOTP_ENABLE_FAILED = 2023;      // 2FA启用失败
constexpr int32_t TOTP_CONFIRM_FAILED = 2024;     // 2FA确认失败
constexpr int32_t RECOVERY_CODE_INVALID = 2025;   // 恢复码无效或已使用
constexpr int32_t TEMP_TOKEN_INVALID = 2026;      // 临时凭证无效或已过期
constexpr int32_t GITHUB_OAUTH_FAILED = 2027;     // GitHub 授权登录失败
constexpr int32_t GITHUB_STATE_INVALID = 2028;    // OAuth state 无效或已过期
constexpr int32_t PASSKEY_NOT_FOUND = 2029;       // Passkey 不存在
constexpr int32_t PASSKEY_ALREADY_EXISTS = 2030;  // Passkey 已注册
constexpr int32_t PASSKEY_REGISTER_FAILED = 2031; // Passkey 注册失败
constexpr int32_t WEBAUTHN_NOT_CONFIGURED = 2032; // WebAuthn 未配置
constexpr int32_t WEBAUTHN_CHALLENGE_INVALID = 2033;  // WebAuthn challenge 无效或已过期
constexpr int32_t WEBAUTHN_ORIGIN_MISMATCH = 2034;    // WebAuthn origin 不匹配
constexpr int32_t WEBAUTHN_RP_ID_MISMATCH = 2035;     // WebAuthn rpId 不匹配
constexpr int32_t WEBAUTHN_INVALID_CREDENTIAL = 2036; // WebAuthn 凭据无效
constexpr int32_t WEBAUTHN_UNSUPPORTED_ALG = 2037;    // WebAuthn 不支持的算法
constexpr int32_t WEBAUTHN_VERIFY_FAILED = 2038;      // WebAuthn 校验失败
constexpr int32_t EMAIL_CODE_ATTEMPTS_EXCEEDED = 2039; // 验证码尝试次数过多

// ==================== 模块03: 文章模块 (03001-03999) ====================
constexpr int32_t ARTICLE_NOT_FOUND = 3001;      // 文章不存在
constexpr int32_t ARTICLE_INVALID_TYPE = 3002;   // 文章类型错误
constexpr int32_t ARTICLE_INVALID_STATE = 3003;  // 文章状态错误
constexpr int32_t ARTICLE_INVALID_ID = 3004;     // 文章ID无效
constexpr int32_t ARTICLE_INSERT_FAILED = 3005;  // 创建文章失败
constexpr int32_t ARTICLE_UPDATE_FAILED = 3006;  // 更新文章失败
constexpr int32_t ARTICLE_DELETE_FAILED = 3007;  // 删除文章失败
constexpr int32_t ARTICLE_PUBLISH_FAILED = 3008; // 发布文章失败
constexpr int32_t ARTICLE_CATEGORY_EMPTY = 3009; // 分类参数为空

// ==================== 模块04: 文件/上传模块 (04001-04999) ====================
constexpr int32_t FILE_NOT_FOUND = 4001;              // 文件不存在
constexpr int32_t FILE_TOO_LARGE = 4002;              // 文件过大
constexpr int32_t FILE_INVALID_NAME = 4003;           // 文件名无效
constexpr int32_t FILE_INVALID_PATH = 4004;           // 文件路径无效
constexpr int32_t FILE_READ_ERROR = 4005;             // 文件读取失败
constexpr int32_t FILE_UPLOAD_FAILED = 4006;          // 文件上传失败
constexpr int32_t FILE_SAVE_FAILED = 4007;            // 文件保存失败
constexpr int32_t FILE_PROTOCOL_ERROR = 4008;         // 上传协议错误
constexpr int32_t FILE_DUMP_FAILED = 4009;            // 文件转储失败
constexpr int32_t UPLOAD_SESSION_NOT_FOUND = 4010;    // 上传会话不存在
constexpr int32_t UPLOAD_INVALID_TYPE = 4011;         // 上传类型无效
constexpr int32_t UPLOAD_INVALID_PARAMS = 4012;       // 上传参数无效
constexpr int32_t UPLOAD_CHUNK_INVALID = 4013;        // 分片参数无效
constexpr int32_t UPLOAD_CHUNK_ALREADY_DONE = 4014;   // 分片已接收
constexpr int32_t UPLOAD_CHUNK_INCOMPLETE = 4015;     // 分片未全部上传
constexpr int32_t UPLOAD_FILE_CREATE_FAILED = 4016;   // 创建文件失败
constexpr int32_t UPLOAD_FILE_ASSEMBLE_FAILED = 4017; // 文件合并失败
constexpr int32_t UPLOAD_FILE_HASH_FAILED = 4018;     // 文件哈希计算失败
constexpr int32_t UPLOAD_TEMP_DIR_FAILED = 4019;      // 创建临时目录失败
constexpr int32_t UPLOAD_TEMP_FILE_FAILED = 4020;     // 创建临时文件失败
constexpr int32_t UPLOAD_CHUNK_SIZE_EXCEED = 4021;    // 分片大小超出声明值
constexpr int32_t UPLOAD_TYPE_INVALID_TYPE = 4022;    // 上传文件类型无效
constexpr int32_t UPLOAD_FILE_EMPTY = 4023;           // 上传文件为空

// ==================== 模块05: 组织模块 (05001-05999) ====================
constexpr int32_t ORG_NOT_FOUND = 5001;              // 组织不存在
constexpr int32_t ORG_DISABLED = 5002;               // 组织已禁用
constexpr int32_t ORG_ALREADY_JOINED = 5003;         // 已加入该组织
constexpr int32_t ORG_ALREADY_APPLIED = 5004;        // 已申请加入该组织
constexpr int32_t ORG_APPLY_NOT_FOUND = 5005;        // 申请不存在
constexpr int32_t ORG_APPLY_ALREADY_REVIEWED = 5006; // 申请已审核
constexpr int32_t ORG_INVALID_ROLE = 5007;           // 无效的组织角色
constexpr int32_t ORG_INVALID_STATE = 5008;          // 无效的组织状态
constexpr int32_t ORG_NOT_MEMBER = 5009;             // 不是组织成员
constexpr int32_t ORG_INVALID_ID = 5010;             // 组织ID无效
constexpr int32_t ORG_INSERT_FAILED = 5011;          // 创建组织失败
constexpr int32_t ORG_UPDATE_FAILED = 5012;          // 更新组织失败
constexpr int32_t ORG_DELETE_FAILED = 5013;          // 删除组织失败
constexpr int32_t ORG_NO_VALID_ORGS = 5014;          // 没有有效的组织
constexpr int32_t ORG_APPLY_INSERT_FAILED = 5015;    // 提交申请失败
constexpr int32_t ORG_APPLY_UPDATE_FAILED = 5016;    // 更新申请失败
constexpr int32_t ORG_USER_REL_FAILED = 5017;        // 组织成员操作失败
constexpr int32_t ORG_REPO_NOT_FOUND = 5018;         // 仓库不存在
constexpr int32_t ORG_REPO_NAME_EXISTS = 5019;       // 同组织下仓库名已存在
constexpr int32_t ORG_PR_NOT_FOUND = 5020;           // PR 记录不存在
constexpr int32_t ORG_REPO_TOKEN_MISSING = 5021;     // 仓库未配置 GitHub Token
constexpr int32_t GITHUB_PR_NOT_FOUND = 5022;        // GitHub 上未找到该 PR
constexpr int32_t GITHUB_RATE_LIMITED = 5023;        // GitHub API 触发限流
constexpr int32_t GITHUB_API_FAILED = 5024;          // GitHub API 请求失败
constexpr int32_t ORG_PR_SYNC_IN_PROGRESS = 5025;    // 该 PR 正在同步中

// ==================== 模块06: 作业模块 (06001-06999) ====================
constexpr int32_t ASSIGNMENT_NOT_FOUND = 6001;       // 作业不存在
constexpr int32_t ASSIGNMENT_INVALID_ID = 6002;      // 作业ID无效
constexpr int32_t ASSIGNMENT_SIZE_EXCEED = 6003;     // 作业文件大小超限
constexpr int32_t ASSIGNMENT_MAX_SIZE_EXCEED = 6004; // 提交数量超限
constexpr int32_t ASSIGNMENT_INSERT_FAILED = 6005;   // 创建作业失败
constexpr int32_t ASSIGNMENT_UPDATE_FAILED = 6006;   // 更新作业失败
constexpr int32_t ASSIGNMENT_DELETE_FAILED = 6007;   // 删除作业失败

// ==================== 模块07: 评论模块 (07001-07999) ====================
constexpr int32_t COMMENT_NOT_FOUND = 7001;         // 评论不存在
constexpr int32_t COMMENT_PERMISSION_DENIED = 7002; // 无权操作此评论
constexpr int32_t COMMENT_CREATE_FAILED = 7003;     // 创建评论失败
constexpr int32_t COMMENT_UPDATE_FAILED = 7004;     // 更新评论失败
constexpr int32_t COMMENT_DELETE_FAILED = 7005;     // 删除评论失败
constexpr int32_t COMMENT_PRAISE_FAILED = 7006;     // 点赞评论失败
constexpr int32_t COMMENT_UNPRAISE_FAILED = 7007;   // 取消点赞评论失败
constexpr int32_t COMMENT_PARENT_NOT_FOUND = 7008;  // 父评论不存在

// ==================== 模块08: R&D平台模块 (08001-08999) ====================
constexpr int32_t BUG_NOT_FOUND = 8001;         // Bug不存在
constexpr int32_t REQUIREMENT_NOT_FOUND = 8002; // 需求不存在
constexpr int32_t TASK_NOT_FOUND = 8003;        // 任务不存在
constexpr int32_t RD_INVALID_TYPE = 8004;       // 无效的工单类型
constexpr int32_t RD_NOT_ORG_MEMBER = 8005;     // 不是该组织成员
constexpr int32_t RD_INSERT_FAILED = 8006;      // 创建工单失败
constexpr int32_t RD_UPDATE_FAILED = 8007;      // 更新工单失败
constexpr int32_t RD_DELETE_FAILED = 8008;      // 删除工单失败
constexpr int32_t RD_INVALID_USER = 8009;       // 用户无效

// ==================== 模块09: 系统内部错误 (09001-09999) ====================
constexpr int32_t DB_CONNECTION_FAILED = 9001;          // 数据库连接失败
constexpr int32_t DB_OPERATION_FAILED = 9002;           // 数据库操作失败
constexpr int32_t DB_TRANSACTION_FAILED = 9003;         // 数据库事务失败
constexpr int32_t DB_COMMIT_FAILED = 9004;              // 数据库提交失败
constexpr int32_t INTERNAL_ERROR = 9005;                // 服务器内部错误
constexpr int32_t SETTINGS_NOT_LOADED = 9006;           // 系统设置未加载
constexpr int32_t SETTINGS_SAVE_FAILED = 9007;          // 系统设置保存失败
constexpr int32_t SMTP_NOT_CONFIGURED = 9008;           // 邮件服务未配置
constexpr int32_t REDIS_OPERATION_FAILED = 9009;        // 缓存服务操作失败
constexpr int32_t METHOD_NOT_ALLOWED = 9010;            // 不支持的请求方法
constexpr int32_t NOTIFICATION_SEND_FAILED = 9011;      // 通知发送失败
constexpr int32_t AI_CONFIG_SAVE_FAILED = 9012;         // AI配置保存失败
constexpr int32_t ARTICLE_PRAISE_FAILED = 9013;         // 点赞文章失败
constexpr int32_t ARTICLE_UNPRAISE_FAILED = 9014;       // 取消点赞文章失败
constexpr int32_t NOTIFICATION_TITLE_TOO_LONG = 9015;   // 通知标题过长
constexpr int32_t NOTIFICATION_CONTENT_TOO_LONG = 9016; // 通知内容过长
constexpr int32_t NOTIFICATION_USERS_REQUIRED = 9017;   // 缺少目标用户
constexpr int32_t NOTIFICATION_INVALID_TARGET = 9018;   // 无效的通知目标类型

// ==================== 模块10: 反馈模块 (10001-10999) ====================
constexpr int32_t FEEDBACK_NOT_FOUND = 10001; // 反馈不存在

// ==================== 模块11: 帮助中心模块 (11001-11999) ====================
constexpr int32_t FAQ_NOT_FOUND = 11001; // 常见问题不存在

// ==================== 模块12: 私信模块 (12001-12999) ====================
constexpr int32_t MESSAGE_CONVERSATION_NOT_FOUND = 12001; // 会话不存在
constexpr int32_t MESSAGE_SEND_FAILED = 12002;            // 消息发送失败
constexpr int32_t MESSAGE_TEXT_TOO_LONG = 12003;          // 消息内容过长
constexpr int32_t MESSAGE_PEER_NOT_FOUND = 12004;         // 对端用户不存在
constexpr int32_t MESSAGE_CANNOT_SELF = 12005;            // 不能与自己私信

} // namespace errcode
} // namespace blog
