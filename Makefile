define time_wrapper
	echo "开始时间: $$(date '+%Y-%m-%d %H:%M:%S.%3N')" && \
	start_time=$$(date +%s%3N) && \
	$(1) && \
	end_time=$$(date +%s%3N) && \
	duration=$$((end_time - start_time)) && \
	echo "结束时间: $$(date '+%Y-%m-%d %H:%M:%S.%3N')" && \
	echo "总计耗时: $$(echo "scale=3; $$duration/1000" | bc) 秒"
endef

# ---- 构建 ----
#   make               → 全量构建（默认）
#   make build         → 全量构建
#   make user          → 构建 user 服务模块
#   make article       → 构建 article 服务模块
#   make comment       → 构建 comment 服务模块
#   make platform      → 构建 platform 服务模块
#   make clean         → 清理并删除 build/ 目录
#   make -j4           → 手动控制并行数（默认留一核）

# 默认目标
.DEFAULT_GOAL := build

_NPROC := $(shell nproc)
_JOBS  ?= $(shell expr $(_NPROC) - 1)

# 从 make -jN 提取并行数
_JOB_CMD = _J="$$(echo "$(MAKEFLAGS)" | sed -n 's/.*-j[[:space:]]*\([0-9]\{1,\}\).*/\1/p')"

.PHONY: build
build:
	@$(_JOB_CMD) && \
	$(call time_wrapper, \
		if [ -d "build" ]; then \
			cd build && cmake -G Ninja .. && ninja -j"$${_J:-$(_JOBS)}"; \
		else \
			mkdir -p build && cd build && cmake -G Ninja .. && ninja -j"$${_J:-$(_JOBS)}"; \
		fi)

%:
	@$(_JOB_CMD) && \
	$(call time_wrapper, \
		if [ -d "build" ]; then \
			cd build && cmake -G Ninja .. && ninja -j"$${_J:-$(_JOBS)}" $@; \
		else \
			mkdir -p build && cd build && cmake -G Ninja .. && ninja -j"$${_J:-$(_JOBS)}" $@; \
		fi)

clean:
	@$(_JOB_CMD) && \
	$(call time_wrapper, \
		if [ -d "build" ]; then \
			cd build && cmake -G Ninja .. && ninja -j"$${_J:-$(_JOBS)}" clean && cd ..; \
			rm -rf build; \
		fi)

orm:
	@$(_JOB_CMD) && \
	CHEN_SDK_LIB="$$(for d in chen-sdk-*/lib; do [ -d "$$d" ] && printf '%s' "$$d" && break; done)"; \
	$(call time_wrapper, LD_LIBRARY_PATH="$$CHEN_SDK_LIB:$$LD_LIBRARY_PATH" bin/orm dbproxy/xml dbproxy/data)

# ---- 依赖同步 / 一键出产物 ----
#   make deps                 → 下载 chen-sdk + 三方依赖（deps.py sync，架构按本机自动识别）
#   make apply                → 用 SDK 的 apply.sh 同步 bin/tools/protocol/resources
#   make collect [ENV=prod]   → 打包部署产物（collect.sh）
#   make dist [ENV=prod]      → deps + apply + orm + build + collect 一条龙

.PHONY: deps apply collect dist
deps:
	@python3 deps.py sync

apply:
	@set -e; found=0; \
	for d in chen-sdk-*/; do \
		[ -f "$${d}apply.sh" ] || continue; \
		sh "$${d}apply.sh"; found=1; \
	done; \
	[ "$$found" = "1" ] || { echo "错误: 未找到 chen-sdk-*/apply.sh，请先执行 make deps" >&2; exit 1; }

collect:
	@./collect.sh $(if $(ENV),$(ENV),prod)

dist:
	@$(MAKE) deps
	@$(MAKE) apply
	@$(MAKE) orm
	@$(MAKE) build
	@$(MAKE) collect ENV=$(if $(ENV),$(ENV),prod)
