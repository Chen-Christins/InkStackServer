# 高性能博客后台系统

## 项目简介

本项目是一个基于 C++20 的高性能博客后台系统，采用模块化设计，支持多用户、多组织、文章管理、标签分类、作业分发等功能。系统架构参考主流
Web 后台设计，注重性能、可扩展性和易维护性。

## 技术栈

- C++20
- CMake + Ninja 构建系统
- MySQL（业务数据）+ Redis（会话、对象缓存、锁等）
- 自研 ORM（XML 配置，自动生成 C++ 数据访问层）
- chen-sdk 基础库
- master/worker 多进程 + 多线程/协程
- GNU gettext 国际化

## 主要功能
- 用户注册、登录、权限管理
- 组织（社群/团队）管理及成员关系
- 文章发布、编辑、分类、标签
- 作业（Assignment）分发与提交
- 日志、配置、定时任务、邮件通知等基础服务

## 目录结构
```
├── bin/                # server 可执行文件、module/*.so、conf/ 运行配置
├── chen-sdk-<ver>/     # chen 框架 SDK（头文件、libchen.so、模块文档）
├── cmake/              # 构建辅助脚本
├── dbproxy/            # ORM 表结构 XML（xml/）与生成数据访问层（data/）
├── deps/               # 三方依赖（构建期使用）
├── dict/               # jieba 分词词典
├── docs/               # 功能与流程文档
├── locale/             # gettext 翻译
├── protocol/           # 协议定义
├── scripts/            # 部署脚本
├── src/                # 业务代码（Manager/Servlets 等）
├── tools/              # 运维/诊断工具
├── build/              # 构建输出
└── Makefile            # 构建入口（make / make orm / make dist）
```

## 快速开始

1. 安装依赖：C++20 编译器、CMake、Ninja、Python 3，以及 MySQL/Redis 客户端库。
2. 首次拉取 SDK 与三方依赖：
   ```sh
   ./deps.py sync
   ```
3. 构建：
   ```sh
   make
   ```
4. 运行（在仓库根目录，配置位于 `bin/conf/`）：
   ```sh
   ./bin/server -s    # 前台运行
   ./bin/server -d    # 守护进程
   ```

## 配置切换（开发/生产）

配置位于 `bin/conf/`，通过 `switch-env.sh` 在开发与生产环境之间快速切换（日志路径、MySQL、Redis 等）：

```sh
# 切换为开发配置
./switch-env.sh dev

# 切换为生产配置（首次需先复制并填写 prod.env）
cp bin/conf/prod.env.example bin/conf/prod.env
./switch-env.sh prod
```

- 模板文件：`bin/conf/system.yml.tpl`、`bin/conf/TechHaven.yml.tpl`
- 环境变量：`bin/conf/dev.env`、`bin/conf/prod.env`（生产密钥不入库）
- 脚本会根据仓库根目录自动推导 `work_path`，无需按机器修改绝对路径

详细的新增配置项方法见 [CONFIG.md](CONFIG.md)。

## 部署打包

`collect.sh` 会收集部署所需文件并打包为 tar.gz（结构与生产目录一致：`server` / `module` / `lib` / `conf` / `dict` / `errors.json` / `locale`）：

```sh
# 打包生产配置（配置值含部署路径 WORK_PATH 全部取自 bin/conf/prod.env）
./collect.sh prod

# 打包开发配置
./collect.sh dev
```

> 部署路径 `WORK_PATH` 在 `prod.env` 中配置（`WORK_PATH=/home/web/apps/server`），无需手动传参。

产物：
- `dist/` — 部署目录（可检查）
- `techhaven-server-<时间戳>.tar.gz` — 上传到服务器解压即可

上传后手动启动（示例）：

```sh
# 在部署目录下
LD_LIBRARY_PATH=./lib ./server -d
```

## 一键出产物（本地 / CI）

`make dist` 把「同步依赖 → 应用 SDK → 生成 ORM → 构建模块 → 打包」串成一条命令：

```sh
make dist ENV=prod      # 架构按当前机器自动识别（dependencies.json 的 arch=auto）
```

等价于：`make deps`（`deps.py sync`）→ `make apply`（SDK `apply.sh`）→ `make orm` → `make build` → `make collect`。

**CI**：`.github/workflows/build.yml` 在 `ubuntu-24.04`(amd64) 与 `ubuntu-24.04-arm`(arm64) 矩阵上执行同样的流程，产出 `techhaven-server-*.tar.gz`：

- 手动 `workflow_dispatch`（可选 `prod`/`dev`）→ 上传为 Actions artifact；
- 推送 `v*` tag → 创建 **draft GitHub Release**，notes 自动从 `CHANGELOG.md` 中对应的
  `## v<版本>` 段落提取；确认 notes/资产后手动 publish，再按 tag 触发部署。

> 发版前请先在 `CHANGELOG.md` 增加本次版本的段落；找不到对应版本时 Release notes 会回退为
> `Release <tag>`。draft Release 默认不对外可见，部署前必须先 publish。tag 名包含
> `-rc` / `-beta` / `-alpha` 时会自动标记为 pre-release，正式版本会自动取消该标记。

CI 生产配置由 **GitHub Secrets/Variables** 注入（见 workflow 的 `Prepare prod.env`），未配置时使用占位值，可在部署机再改。涉及的 Secrets/Variables：

| 名称                                                                                  | 类型     | 说明                               |
|---------------------------------------------------------------------------------------|----------|------------------------------------|
| `MYSQL_HOST` / `MYSQL_PORT` / `MYSQL_USER` / `MYSQL_PASSWD` / `MYSQL_DBNAME`          | Secret   | MySQL 连接                         |
| `REDIS_HOST` / `REDIS_DBNAME`                                                         | Secret   | Redis 连接                         |
| `OAUTH_GITHUB_CLIENT_ID` / `OAUTH_GITHUB_CLIENT_SECRET` / `OAUTH_GITHUB_CALLBACK_URL` | Secret   | GitHub OAuth                       |
| `WEBAUTHN_RP_ID` / `WEBAUTHN_RP_NAME` / `WEBAUTHN_ORIGIN`                             | Variable | Passkey RP ID / 名称 / 前端 origin |
| `WEBAUTHN_USER_HANDLE_SECRET`                                                         | Secret   | Passkey user handle HMAC 密钥      |
| `WORK_PATH` / `LOG_LEVEL` / `FRONTEND_BASE`                                           | Variable | 部署路径 / 日志级别 / 前端地址     |

> 升级 SDK/依赖：改 `dependencies.json` 的 `chen-sdk.version`（或 `arch`）后推代码即可触发流水线。

## 部署（cold / hot / stop）

部署服务器为 **amd64**；**手动触发** `.github/workflows/deploy.yml`（可选择**分支或 tag** 运行），只需选 `mode`：

| 模式             | 行为                                                                                                                                                                      |
|------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `cold`（冷启动） | 停服 → 清空运行目录 → 解压产物 → 用 `CONFIG_DIR` 覆盖 `conf/` → `LD_LIBRARY_PATH=./lib ./server -d`                                                                       |
| `hot`（热启动）  | 解压产物到临时目录 → 同步 `conf/`（设置 `CONFIG_DIR` 时）→ 替换 `module/*.so`、`errors.json`、`locale/` → 向 master 发 `SIGHUP`（fork 新 worker，旧 worker drain 后退出） |
| `stop`（停服）   | 进入运行目录执行 `./server -s stop`（SIGTERM 优雅关闭）                                                                                                                   |

产物来源按所选 ref 自动判定：
- **tag** → 取该 tag 的 GitHub Release 资产 `techhaven-server-<tag>-amd64.tar.gz`；
- **分支** → 取该分支**最近一次成功的 `Build & Package`** 的 Actions artifact `techhaven-server-amd64`（所以分支须先跑过构建）。

服务器端逻辑在 `scripts/deploy.sh`（随每次部署 scp 到服务器执行）。

**Secrets**：`SSH_HOST`、`SSH_USER`、`SSH_PORT`、`SSH_PRIVATE_KEY`（免密私钥）
**Variables**：`DEPLOY_PATH`（运行目录）、`CONFIG_DIR`（冷启动覆盖用的配置目录）

> 注意：`CONFIG_DIR` 内 `system.yml` 的 `server.work_path`（及日志/字典路径）必须指向 `DEPLOY_PATH`。cold 会清空运行目录（含日志）；hot
> 只替换模块、`errors.json` 和 `locale/`，如果改了 `server`、`libchen.so` 或端口/监听地址等启动期配置，必须走 cold。`SIGHUP`
> 只是在 master 下 fork 新 worker，不会让 master 重新绑定端口。

## 依赖管理

通过 `dependencies.json` 声明依赖，`deps.py` 负责下载：

```json
{
  "chen-sdk": { "repo": "Chen-Christins/chen", "version": "1.7.0-rc3", "arch": "auto" },
  "thirdparty": { "repo": "Chen-Christins/chen", "tag": "v1.0.0", "asset": "deps-{arch}.zip" },
  "cppjieba": {
    "repo": "yanyiwu/cppjieba",
    "tag": "v5.1.0",
    "limonp_repo": "yanyiwu/limonp",
    "limonp_ref": "a269e34dc4948d5a9209e21a7887b52daa0d3e78"
  }
}
```

- `chen-sdk`：框架 SDK（`chen-sdk-<version>-<arch>.zip`），解压为根目录 `chen-sdk-<version>/`。
- `thirdparty`：三方依赖包（protobuf / yaml-cpp / tinyxml2 / jsoncpp / hiredis-vip 的头文件 + 静态库），解压到 `deps/`。
- `cppjieba`：中文分词头文件（`deps/include/cppjieba`）及其 `limonp` 依赖（`deps/include/limonp`）。两者均为 header-only，编译时直接进入 `libblog.so`。

| 命令                                        | 说明                                              |
|---------------------------------------------|---------------------------------------------------|
| `./deps.py sync`                            | 下载 chen-sdk + thirdparty + cppjieba（默认全部） |
| `./deps.py sync --sdk-only`                 | 只下载 chen-sdk                                   |
| `./deps.py thirdparty`                      | 只下载三方依赖与 cppjieba 到 `deps/`              |
| `./deps.py use <version> --arch amd64`      | 切换 SDK 版本/架构                                |
| `./deps.py use --archive /tmp/chen-sdk.zip` | 使用本地临时 SDK 包                               |
| `./deps.py current`                         | 查看已安装 SDK 与 `deps/` 状态                    |
| `./deps.py list`                            | 列出远端可用资产                                  |
| `./deps.py clean`                           | 删除 SDK、`deps/` 与缓存                          |

说明：

- 资产缓存在 `.deps-cache/`，用 GitHub Release 的 `digest` 做 sha256 校验（离线时仅告警）；cppjieba/limonp 的源码包按 tag/commit 缓存。
- 下载失败会自动重试（最多 3 次）并利用 `.part` 断点续传；校验失败或缓存 zip 损坏时会自动删除坏缓存，重新运行
  `./deps.py sync` 即可重新下载。
- 三方依赖与 cppjieba 头文件只用于**编译**（`deps/include`）。模块**不**静态链接三方库，运行时符号由预编译 `bin/server` 提供，避免与 server 内的静态副本重复；部署机也无需安装这些库。cppjieba 为 header-only，不存在需要额外链接的 `.a`。
- 架构按主机自动识别（`dependencies.json` 的 `arch=auto`）：mac(arm64) / Linux(amd64) 各自 `./deps.py sync` 即可，无需软链；如需固定，用 `./deps.py use <version> --arch <arch>` 改配置。

## 国际化（i18n）

- 语言解析顺序：`?lang=` → `Accept-Language` → 系统设置 `language` → `zh_CN`。
- 当前支持 `zh_CN`、`en`。
- 错误码消息：`errors.json` 提供 errno/key 与兜底，`locale/po/*.po` 提供 gettext 翻译，运行时加载 `locale/C/LC_MESSAGES/techhaven_<lang>.mo`。
- 自定义消息：`result->setErrno(ec, "literal")`；带参数使用 `result->setErrnoFmt(ec, "param %s is required", param)`。
- 新增或修改消息后：

```sh
ninja i18n-sync    # 重新提取源码消息并合并 PO
# 编辑 locale/po/en.po、locale/po/zh_CN.po
ninja i18n-check   # 校验 POT/PO 完整性及占位符
```

- `.mo` 由构建自动生成到 `locale/C/LC_MESSAGES/`（已 gitignore）。
- 构建依赖：Linux 安装 `gettext`（提供 `xgettext` / `msgfmt` / `msgmerge` / `msginit`）；运行时使用 glibc gettext，无需额外库。
- 部署包会包含 `locale/C/LC_MESSAGES/*.mo`；热部署会同步 `locale/` 目录。

服务启动后可验证：

```sh
curl -H 'Accept-Language: en-US' http://127.0.0.1:8090/api/v1/user/info
# msg: Not logged in

curl 'http://127.0.0.1:8090/api/v1/user/info?lang=zh-CN'
# msg: 未登录

curl 'http://127.0.0.1:8090/api/v1/error-codes?lang=en'
# data: 英文错误码表
```

## 相关文档

- `src/servlets/` 下的 servlet 注册即接口定义
- `dbproxy/xml/` 下的 XML 文件定义所有表结构，`dbproxy/data/` 为生成的数据访问层
- `docs/` 下按功能划分的流程与接口说明
- [CONFIG.md](CONFIG.md) 配置项与热重载机制

## 贡献与交流
欢迎提交 Issue 或 PR，或通过邮件与作者联系。

---
高性能 · 模块化 · 易扩展
