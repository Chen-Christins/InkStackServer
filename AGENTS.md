# AGENTS.md

This file provides mandatory rules for AI agents working in this repository. Violating these rules may cause segfaults or security issues.

**Before building, check your own OS (§0.1). On non-Linux hosts, all compilation and run verification must be handed off
to the user's Linux machine.**

**Before writing code, read the SDK docs in `chen-sdk-*/docs/` and `chen-sdk-*/AGENTS.md`, then the module README you
are going to use (§0.2).**

**When writing or modifying any C++ code, invoke the `/cpp-code-style` skill first for complete coding conventions.**

## 0. 环境与必读资料（MANDATORY）

### 0.1 编译与验证环境

**动手前先确认 AI 自己跑在什么系统上**（`uname -s`）。本项目只在 Linux（x86_64 / arm64）上构建与运行，见 README 与 CI（
`.github/workflows/build.yml`）。

- **Linux 环境**：可以自行执行构建与验证。
- **非 Linux 环境（macOS / Windows 等）**：**禁止**执行任何编译和运行操作，包括但不限于
  `make`、`make build`、直接调用 `cmake` / `ninja`、`make dist`、启动 `bin/server`（含 `-s` / `-d`）。
  - 非 Linux 上的编译结果没有参考价值，还会污染 `build/`、`dist/`。
  - 正确做法：改完代码后，把需要在 Linux 开发机上执行的命令、预期输出、需要重点观察的日志/报错整理给用户，由用户在 Linux
    开发机上验证，**等用户回报结果后再继续**。
  - 允许：静态检查（读代码、grep、`git diff`）、以及不涉及编译的依赖/代码生成步骤（`./deps.py sync`、`make deps`、`make orm`
    ）。若这些步骤本身因缺工具跑不通，直接告知用户，不要强行绕开。
  - 不得把“本地没报错”当作验证通过；最终以用户 Linux 侧编译结果为准。
  - 用户回报编译错误时，依据日志修改代码，再重新给出验证命令。

### 0.2 SDK 文档必读

**写任何业务代码前，必须先读 SDK 自带文档。** SDK 目录形如 `chen-sdk-<version>/`（如 `chen-sdk-1.7.0-rc4/`），由
`./deps.py sync` 下载、版本号会变，用通配路径引用：

1. `chen-sdk-*/docs/framework-first.md` — 框架优先：能力对照表、分领域规则、反模式自查、Review 清单
2. `chen-sdk-*/AGENTS.md` — SDK 硬性要求
3. `chen-sdk-*/include/chen/<module>/README.md` — 用到的模块 API（log / config / util / http / rpc / db / orm ...）
4. `chen-sdk-*/include/chen/` 下的公开头文件（以此为准，不要凭记忆猜 API）

硬性要求（与 SDK 文档冲突时取更严格的一方）：

- 基础设施先用框架封装，再考虑标准库，最后才自己写：日志 `INFO(logger)`、配置 `Config::Lookup<T>`、时间 `TimeUtil`、文件
  `FSUtil`、字符串 `StringUtil`、随机 `RandomUtil`、加密 `EncryptorUtil`、DB `*Mgr` + `QueryBuilder` + ORM Dao、并发
  `IOManager::schedule/addTimer` + `FiberSemaphore`、缓存 `LruCache`、事件 `EventBusMgr`。
- 禁止在业务代码出现：`printf` / `std::cout`、`std::thread` / `pthread_create`、`std::this_thread::sleep_for`、
  `std::filesystem` / `fopen`、`rand()`、裸 `hiredis` / `mysql_real_connect`、字符串拼 SQL、`node.as<bool>()`、自建连接池 /
  自建 LRU。
- 只 include `<chen/...>` 并链接 `lib/libchen.so`；yaml-cpp / tinyxml2 / jsoncpp / hiredis-vip / protobuf 已静态进
  `libchen.so`，不要再链接或新增第三方依赖。
- 不要复制 SDK 框架源码进业务目录（升级 SDK 会失配）；框架确实缺能力时，先按 `framework-first.md` 的流程确认，再决定是否补框架。
- 模块在 `onUnload` 里注销 `registerMethod` / servlet / EventBus 回调。

## 1. Null Pointer Safety (MANDATORY)

**Any pointer from an external call must be null-checked before dereference. No exceptions.**

### Never chain pointer access:

```cpp
// ❌ ALWAYS WRONG
int32_t role = UserMgr::GetInstance()->get(uid)->getRole();
auto org_role = getByOrgAndUser(org_id, uid)->getRole();
```

### Always store, check, then use:

```cpp
// ✅ REQUIRED PATTERN
auto user = UserMgr::GetInstance()->get(uid);
if (!user) {
    result->setErrno(errcode::USER_NOT_FOUND);
    break;
}
int32_t role = user->getRole();
```

### Before any Manager::get(), check the key:

```cpp
int64_t uid = getUserId(request);
if (!uid) {                    // check id first
    result->setErrno(errcode::NOT_LOGIN);
    break;
}
auto info = Mgr::GetInstance()->get(id);
if (!info) {                   // check pointer second
    result->setErrno(errcode::SOME_NOT_FOUND);
    break;
}
```

### Common null-able return values:

| Return | Check |
|--------|-------|
| `*Mgr::GetInstance()->get(id)` | `if (!ptr)` |
| `*Mgr::GetInstance()->getByXxx(...)` | `if (!ptr)` |
| `*Dao::Query(...)` | `if (!ptr)` |
| `GetDB()` | `if (!db)` |
| `db->prepare(sql)` | `if (!stmt)` |
| `stmt->query()` | `if (!rt)` |
| `getUserId(request)` | `if (!uid)` |
| `m_cache.get(key)` | check before use |

## 2. Servlet Patterns

### Servlet base classes:
- `BlogServlet` — public endpoint (no login required)
- `BlogLoginedServlet` — requires login (`handlePre` calls `initLogin()`)

### Handler structure:
```cpp
int32_t XxxServlet::handle(request, response, session, result) {
    do {
        // 1. Extract params with DEFINE_AND_CHECK_*
        // 2. Null-check all pointers
        // 3. Business logic
        // 4. Set result data
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}
```

### Error reporting:
```cpp
result->setErrno(errcode::NOT_LOGIN);                    // fixed message from errors.json
result->setErrno(errcode::ACCESS_DENIED, "custom msg");  // custom message
```

### Route registration:
```cpp
// In blog_module.cc → registerServlets()
dp->addServlet("/api/v1/xxx/yyy", XX(XxxServlet));
```

## 3. File Header Comments

New header files (`.h`) require a Doxygen file comment. **Always ask the user which license to use** for the `@copyright` field — do not assume:

```cpp
/**
 * @file xxx.h
 * @brief ...
 * @author Christins
 * @date YYYY-MM-DD
 * @copyright <ask user>
 */
```

Source files (`.cc`) do NOT need this header.

## 4. Process Model & Module Lifecycle

**本项目热重载采用 master/worker 多进程模型，不存在进程内 `dlclose`。**

```
master  绑定端口、接收信号、fork/回收 worker，不加载业务模块
worker  加载 module/*.so，注册 servlet/handler/RPC/EventBus，运行 onTick
```

**启动（worker）：**

```
CreateModule(dlopen)
  → onBeforeArgsParse → onAfterArgsParse
  → onLoad
  → onServerReady      // 注册路由、RPC、EventBus，初始化 Manager
  → server->start()
  → onServerUp         // 启动后台任务
  → [onTick ...]
```

**热重载（SIGHUP）：**
```
master fork 新 worker
  → 新 worker 重新读取 conf/ 和磁盘上的 module/*.so
  → 新 worker 就绪并开始 accept
  → master 向旧 worker 发 SIGQUIT
  → 旧 worker：停止 accept → 关闭连接 → 限时 drain（server.drain_timeout_ms，默认 3000ms）
  → onDeactivate → onUnload → _exit(0)
```

**关机（SIGTERM/SIGINT）：** master 向所有 worker 发 SIGTERM，worker 走同一条 graceful shutdown 路径。

### 必须知道的结论

- 正常启停和热重载 **不会在进程内 `dlclose` 模块**；旧 worker 用 `_exit(0)` 退出，地址空间由内核回收。
- `ModuleMgr` 是 `LeakySingleton`，进程退出时不析构模块；不要依赖静态析构或 `dlclose` 时机做清理。
- EventBus 是 **进程内单例**：新旧 worker 各有一份，旧 worker 的 listener 不会影响新 worker。
- 模块内的 Manager 单例、LRU、计数器、WebSocket 连接都是 worker 私有的；热重载后全部归零。跨重载状态放 Redis/MySQL，或在
  `onDeactivate` flush。
- drain 窗口内新旧 worker 会同时运行，旧 worker 的 tick 定时器在 drain 超时后才取消，`onTick` 必须幂等；定时发布、清理、广播等操作建议加
  Redis 锁或做幂等。
- master 不加载业务模块，也不应访问模块单例（如 `ArticleMgr`、`WSMgr`）。

### EventBus / 回调清理规则（多进程）

- `onServerReady()` 里的 `clearAll()` 不是热重载必需：新 worker 的 EventBus 本来就是空的。若保留，要确保本模块的 listener
  都在 `clearAll()` 之后注册。
- `onDeactivate()` 不是“清掉新模块 listener”的地方——新旧 worker 进程隔离，它只会影响当前 worker。drain
  期间可能仍有事件在处理，不建议在这里清空。
- 需要清理就放在 `onUnload()`（进程退出前最后一步）。
- 只有显式调用 `ModuleMgr::del()/delAll()` 在同一进程内卸载模块时，才必须在模块对象销毁前移除 EventBus / Config /
  Servlet / RPC 回调；正常启停和热重载不会走这条路径。

## 5. New Feature Checklist

When adding a new feature:

1. **XML table** (`dbproxy/xml/`) — define table schema, then `make orm`
2. **Manager** (`src/manager/`) — singleton + LRU cache + CRUD methods + null checks
3. **Servlet** (`src/servlets/<domain>/`) — follow handler pattern above
4. **Includes** — update `src/include/tables.h`, `managers.h`, `servlets.h`
5. **Registration** — `blog_module.cc`: table create, migrate, servlet route
6. **Error codes** — `src/error_codes.h` + `errors.json`

## 6. Code Style Quick Reference

| Element | Convention |
|---------|-----------|
| Classes | `PascalCase` |
| Member variables | `m_snake_case` |
| Local variables | `snake_case` |
| Member functions | `camelCase` |
| Static functions | `PascalCase` |
| Macros | `UPPER_SNAKE_CASE` |
| Brace (class/namespace) | same line |
| Brace (function) | new line |
| Brace (if/for/while) | same line, never omit |
| Null pointer | `nullptr` (never `NULL` or `0`) |
| Indent | 4 spaces (no tabs) |
