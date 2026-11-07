#!/usr/bin/env python3
"""TechHaven-Server dependency manager.

Reads dependencies.json (optionally overridden by dependencies.local.json) and
downloads the matching chen-sdk release asset to the repository root, replacing
any previously installed SDK. CMake and collect.sh keep working through their
existing `chen-sdk-*` glob. Third-party headers and cppjieba/limonp sources are
installed under deps/ for compile-time use.
"""

import argparse
import hashlib
import json
import os
import platform
import re
import shutil
import sys
import tempfile
import time
import urllib.error
import urllib.request
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent
MANIFEST = ROOT / "dependencies.json"
LOCAL_OVERRIDE = ROOT / "dependencies.local.json"
CACHE_DIR = ROOT / ".deps-cache"

SDK_KEY = "chen-sdk"
THIRDPARTY_KEY = "thirdparty"
CPPJIEBA_KEY = "cppjieba"
DEFAULT_REPO = "Chen-Christins/chen"
SDK_DIR_PREFIX = "chen-sdk-"
DEPS_DIR = ROOT / "deps"
CPPJIEBA_INCLUDE = DEPS_DIR / "include" / "cppjieba"
LIMONP_INCLUDE = DEPS_DIR / "include" / "limonp"

VERSION_RE = re.compile(r"^chen-sdk-(\d[\w.\-]*?)(?:-(arm64|amd64))?$")
ASSET_RE = re.compile(r"^chen-sdk-(\d[\w.\-]*?)-(arm64|amd64)\.zip$")

DOWNLOAD_ATTEMPTS = 3
DOWNLOAD_CHUNK = 65536
RETRYABLE_HTTP_STATUS = {408, 429, 500, 502, 503, 504}


class DepsError(Exception):
    pass


class UI:
    RESET = "\033[0m"
    RED = "\033[31m"
    GREEN = "\033[32m"
    YELLOW = "\033[33m"
    BLUE = "\033[34m"
    MAGENTA = "\033[35m"
    CYAN = "\033[36m"

    def __init__(self, color=True):
        self.color = bool(color) and sys.stdout.isatty()

    def _paint(self, code, text):
        if not self.color:
            return text
        return f"{code}{text}{self.RESET}"

    def info(self, msg):
        print(f"{self._paint(self.BLUE, 'ℹ')} {msg}")

    def step(self, icon, msg):
        print(f"{icon} {self._paint(self.CYAN, msg)}")

    def ok(self, msg):
        print(f"{self._paint(self.GREEN, '✔')} {msg}")

    def warn(self, msg):
        print(f"{self._paint(self.YELLOW, '⚠')} {msg}")

    def error(self, msg):
        print(f"{self._paint(self.RED, '✖')} {msg}", file=sys.stderr)

    def path(self, text):
        return self._paint(self.MAGENTA, text)

    def progress(self, done, total):
        if not self.color:
            return
        if total:
            pct = done * 100 // total
            length = 24
            filled = length * pct // 100
            bar = "█" * filled + "░" * (length - filled)
            line = f"   {bar} {pct:3d}%  {human(done)}/{human(total)}"
        else:
            line = f"   {human(done)}"
        sys.stdout.write("\r" + line + "\x1b[K")
        sys.stdout.flush()

    def progress_end(self):
        if self.color:
            sys.stdout.write("\n")
            sys.stdout.flush()


def human(n):
    value = float(n)
    for unit in ("B", "KB", "MB", "GB"):
        if value < 1024 or unit == "GB":
            return f"{value:.1f}{unit}"
        value /= 1024


def host_arch():
    machine = platform.machine().lower()
    if machine in ("arm64", "aarch64"):
        return "arm64"
    if machine in ("x86_64", "amd64"):
        return "amd64"
    raise DepsError(f"无法识别的架构: {machine}")


def load_json(path):
    if not path.exists():
        return {}
    try:
        with path.open(encoding="utf-8") as fh:
            return json.load(fh)
    except (OSError, json.JSONDecodeError) as exc:
        raise DepsError(f"无法解析 {path.name}: {exc}") from exc


def load_manifest():
    data = load_json(MANIFEST)
    if SDK_KEY not in data:
        raise DepsError(f"{MANIFEST.name} 中缺少 {SDK_KEY} 配置")
    return data


def save_manifest(data):
    with MANIFEST.open("w", encoding="utf-8") as fh:
        json.dump(data, fh, indent=2, ensure_ascii=False)
        fh.write("\n")


def clear_local_override():
    if LOCAL_OVERRIDE.exists():
        LOCAL_OVERRIDE.unlink()


def resolve_config():
    cfg = dict(load_manifest()[SDK_KEY])
    override = load_json(LOCAL_OVERRIDE).get(SDK_KEY, {})
    cfg.update(override)
    cfg.setdefault("repo", DEFAULT_REPO)
    return cfg


def require(cfg, key):
    value = cfg.get(key)
    if not value:
        raise DepsError(f"dependencies.json 缺少 chen-sdk.{key}")
    return value


def asset_name(version, arch):
    return f"chen-sdk-{version}-{arch}.zip"


def sdk_dir(version):
    return ROOT / f"{SDK_DIR_PREFIX}{version}"


def valid_sdk(path):
    path = Path(path)
    return (path / "lib" / "libchen.so").is_file() and (path / "include" / "chen").is_dir()


def parse_version_dir(path):
    match = VERSION_RE.match(Path(path).name)
    return match.group(1) if match else None


def parse_arch_dir(path):
    match = VERSION_RE.match(Path(path).name)
    return match.group(2) if match else None


def parse_asset(name):
    match = ASSET_RE.match(Path(name).name)
    if not match:
        return None, None
    return match.group(1), match.group(2)


def installed_dirs():
    result = []
    for entry in ROOT.glob(f"{SDK_DIR_PREFIX}*"):
        if entry.is_dir() or entry.is_symlink():
            result.append(entry)
    return sorted(result, key=lambda p: p.name)


def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def cached_zip_ok(path):
    path = Path(path)
    if not path.is_file() or path.stat().st_size <= 0:
        return False
    try:
        return zipfile.is_zipfile(path)
    except OSError:
        return False


def discard_cached(path):
    """Delete a corrupt file inside .deps-cache/. Never touches local archives."""
    path = Path(path)
    try:
        if path.parent.resolve() != CACHE_DIR.resolve():
            return False
        path.unlink(missing_ok=True)
        return True
    except OSError:
        return False


def _download_once(url, part, ui):
    resume_from = part.stat().st_size if part.is_file() else 0
    headers = {"User-Agent": "techhaven-deps"}
    if resume_from:
        headers["Range"] = f"bytes={resume_from}-"
    req = urllib.request.Request(url, headers=headers)
    with urllib.request.urlopen(req, timeout=60) as resp:
        status = getattr(resp, "status", None) or resp.getcode()
        length = int(resp.headers.get("Content-Length") or 0)
        if status == 206 and resume_from:
            mode = "ab"
            done = resume_from
            expected = resume_from + length if length else None
        elif status == 200:
            mode = "wb"
            done = 0
            expected = length or None
        else:
            raise DepsError(f"意外的 HTTP 状态 {status}: {url}")
        total = expected or 0
        with part.open(mode) as fh:
            while True:
                chunk = resp.read(DOWNLOAD_CHUNK)
                if not chunk:
                    break
                fh.write(chunk)
                done += len(chunk)
                ui.progress(done, total)
        actual = part.stat().st_size
        if expected is not None and actual != expected:
            raise DepsError(f"下载不完整: {human(actual)} / {human(expected)}")


def download(url, dest, ui, attempts=DOWNLOAD_ATTEMPTS):
    dest.parent.mkdir(parents=True, exist_ok=True)
    part = dest.with_suffix(dest.suffix + ".part")
    last_error = None
    for attempt in range(1, attempts + 1):
        try:
            _download_once(url, part, ui)
            ui.progress_end()
            part.replace(dest)
            return dest
        except urllib.error.HTTPError as exc:
            ui.progress_end()
            if exc.code == 416:
                part.unlink(missing_ok=True)
                last_error = exc
                if attempt < attempts:
                    ui.warn(f"续传位置无效，从头重新下载 {dest.name}")
                    continue
                break
            if exc.code not in RETRYABLE_HTTP_STATUS:
                raise DepsError(f"下载失败 HTTP {exc.code}: {url}") from exc
            last_error = exc
        except Exception as exc:
            ui.progress_end()
            last_error = exc
        if attempt < attempts:
            delay = attempt
            ui.warn(f"下载中断，{delay} 秒后重试 ({attempt}/{attempts}): {last_error}")
            time.sleep(delay)
    hint = "，已保留断点文件，重新运行可续传" if part.is_file() else ""
    raise DepsError(f"下载失败（已尝试 {attempts} 次）: {last_error}{hint}") from last_error


def ensure_cached(url, asset, ui):
    cache = CACHE_DIR / asset
    if cache.is_file() and cache.stat().st_size > 0:
        if cached_zip_ok(cache):
            ui.ok(f"📦 命中缓存 {ui.path(cache.name)}")
            return cache
        ui.warn(f"缓存已损坏，重新下载 {ui.path(cache.name)}")
        discard_cached(cache)
    ui.step("📦", f"下载 {asset}")
    ui.info(url)
    download(url, cache, ui)
    if not cached_zip_ok(cache):
        discard_cached(cache)
        raise DepsError(f"{asset} 下载内容不是有效的 zip，请重新运行 ./deps.py sync")
    return cache


def ensure_download(repo, version, arch, ui):
    asset = asset_name(version, arch)
    url = f"https://github.com/{repo}/releases/download/v{version}/{asset}"
    return ensure_cached(url, asset, ui)


def fetch_digest(repo, version, asset, ui):
    url = f"https://api.github.com/repos/{repo}/releases/tags/v{version}"
    headers = {
        "Accept": "application/vnd.github+json",
        "User-Agent": "techhaven-deps",
    }
    token = os.environ.get("GITHUB_TOKEN") or os.environ.get("GH_TOKEN")
    if token:
        headers["Authorization"] = f"Bearer {token}"
    try:
        req = urllib.request.Request(url, headers=headers)
        with urllib.request.urlopen(req, timeout=20) as resp:
            data = json.load(resp)
    except urllib.error.HTTPError as exc:
        ui.warn(f"GitHub API 请求失败 ({exc.code})，跳过 sha256 校验")
        return None
    except Exception as exc:
        ui.warn(f"GitHub API 不可用（{exc.__class__.__name__}），跳过 sha256 校验")
        return None
    for item in data.get("assets", []):
        if item.get("name") == asset:
            digest = item.get("digest") or ""
            if digest.startswith("sha256:"):
                return digest.split(":", 1)[1]
    ui.warn(f"release v{version} 未提供 {asset} 的 sha256，跳过校验")
    return None


def verify_zip(zip_path, repo, version, asset, ui, local=False):
    if local:
        ui.warn("🪄 使用本地包，跳过 sha256 校验")
        return
    ui.step("🔐", f"校验 sha256 {asset}")
    expected = fetch_digest(repo, version, asset, ui)
    if not expected:
        return
    actual = sha256_file(zip_path)
    if actual != expected:
        removed = discard_cached(zip_path)
        hint = (
            "已删除损坏缓存，重新运行 ./deps.py sync 即可重新下载"
            if removed
            else "本地包可能已损坏，请更换后重试"
        )
        raise DepsError(f"sha256 校验失败\n  期望: {expected}\n  实际: {actual}\n  {hint}")
    ui.ok("sha256 校验通过")


def locate_sdk_root(base):
    base = Path(base)
    if valid_sdk(base):
        return base
    for child in sorted(base.iterdir()):
        if child.is_dir() and valid_sdk(child):
            return child
    for child in sorted(base.iterdir()):
        if child.is_dir():
            for grand in sorted(child.iterdir()):
                if grand.is_dir() and valid_sdk(grand):
                    return grand
    return None


def swap_dir(src_root, dest, ui):
    dest = Path(dest)
    if dest.exists():
        if dest.is_symlink():
            dest.unlink()
        else:
            shutil.rmtree(dest)
    shutil.move(str(src_root), str(dest))


def install_zip(zip_path, version, ui):
    dest = sdk_dir(version)
    tmp = Path(tempfile.mkdtemp(prefix=".sdk-tmp-", dir=ROOT))
    try:
        ui.step("📂", f"解压到 {ui.path(dest.name + '/')}")
        try:
            with zipfile.ZipFile(zip_path) as zf:
                extract_zip_restore(zf, tmp)
        except zipfile.BadZipFile as exc:
            discard_cached(zip_path)
            raise DepsError(
                f"{Path(zip_path).name} 不是有效的 zip（可能下载不完整），请重新运行 ./deps.py sync"
            ) from exc
        sdk_root = locate_sdk_root(tmp)
        if sdk_root is None:
            raise DepsError(f"{Path(zip_path).name} 内未找到 lib/libchen.so")
        swap_dir(sdk_root, dest, ui)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    if not valid_sdk(dest):
        raise DepsError(f"安装后校验失败：{dest.name} 缺少 lib/libchen.so 或 include/chen")
    ensure_bin_executable(dest)


def install_dir(src, version, ui):
    dest = sdk_dir(version)
    if not valid_sdk(src):
        raise DepsError(f"{src} 不是有效的 SDK 目录（缺少 lib/libchen.so）")
    ui.step("📂", f"复制到 {ui.path(dest.name + '/')}")
    tmp = Path(tempfile.mkdtemp(prefix=".sdk-tmp-", dir=ROOT))
    try:
        work = tmp / Path(src).name
        shutil.copytree(src, work)
        swap_dir(work, dest, ui)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    if not valid_sdk(dest):
        raise DepsError(f"安装后校验失败：{dest.name} 缺少依赖文件")
    ensure_bin_executable(dest)


def gc(keep, keep_asset, ui, keep_old=False):
    removed = []
    if keep_old:
        return removed
    keep_resolved = Path(keep).resolve() if keep else None
    for entry in installed_dirs():
        if keep_resolved and entry.resolve() == keep_resolved:
            continue
        if entry.is_symlink():
            entry.unlink()
        else:
            shutil.rmtree(entry)
        removed.append(entry.name)
    for zip_file in sorted(ROOT.glob(f"{SDK_DIR_PREFIX}*.zip")):
        if keep_asset and zip_file.name == keep_asset:
            continue
        zip_file.unlink()
        removed.append(zip_file.name)
    if CACHE_DIR.is_dir():
        for cached in sorted(CACHE_DIR.glob(f"{SDK_DIR_PREFIX}*.zip")):
            if keep_asset and cached.name == keep_asset:
                continue
            cached.unlink()
            removed.append(f".deps-cache/{cached.name}")
    return removed


def report_gc(removed, ui):
    if not removed:
        return
    ui.step("🧹", f"清理旧版本 {', '.join(removed)}")


def thirdparty_config(manifest=None):
    data = manifest if manifest is not None else load_json(MANIFEST)
    tp = data.get(THIRDPARTY_KEY)
    if not tp:
        return None
    override = load_json(LOCAL_OVERRIDE).get(THIRDPARTY_KEY, {})
    tp = dict(tp)
    tp.update(override)
    tp.setdefault("repo", DEFAULT_REPO)
    tp.setdefault("asset", "deps-{arch}.zip")
    return tp


def resolve_tp_arch(tp, sdk_cfg=None):
    arch = tp.get("arch")
    if (not arch or arch == "auto") and sdk_cfg:
        sdk_arch = sdk_cfg.get("arch")
        if sdk_arch and sdk_arch != "auto":
            arch = sdk_arch
    if not arch or arch == "auto":
        arch = host_arch()
    return arch


def cppjieba_config(manifest=None):
    data = manifest if manifest is not None else load_json(MANIFEST)
    cfg = data.get(CPPJIEBA_KEY)
    if not cfg:
        return None
    override = load_json(LOCAL_OVERRIDE).get(CPPJIEBA_KEY, {})
    cfg = dict(cfg)
    cfg.update(override)
    cfg.setdefault("repo", "yanyiwu/cppjieba")
    cfg.setdefault("tag", "v5.1.0")
    cfg.setdefault("limonp_repo", "yanyiwu/limonp")
    return cfg


def cppjieba_ready():
    return (CPPJIEBA_INCLUDE / "Jieba.hpp").is_file() and (LIMONP_INCLUDE / "StringUtil.hpp").is_file()


def ensure_asset(repo, tag, asset, ui):
    url = f"https://github.com/{repo}/releases/download/{tag}/{asset}"
    return ensure_cached(url, asset, ui)


def ensure_source_archive(repo, ref, asset, ui):
    url = f"https://github.com/{repo}/archive/{ref}.zip"
    return ensure_cached(url, asset, ui)


def source_asset_name(prefix, ref):
    value = re.sub(r"[^A-Za-z0-9._-]+", "-", ref).strip("-")
    return f"{prefix}-{value or 'source'}.zip"


def fetch_digest_tag(repo, tag, asset, ui):
    url = f"https://api.github.com/repos/{repo}/releases/tags/{tag}"
    headers = {"Accept": "application/vnd.github+json", "User-Agent": "techhaven-deps"}
    token = os.environ.get("GITHUB_TOKEN") or os.environ.get("GH_TOKEN")
    if token:
        headers["Authorization"] = f"Bearer {token}"
    try:
        req = urllib.request.Request(url, headers=headers)
        with urllib.request.urlopen(req, timeout=20) as resp:
            data = json.load(resp)
    except urllib.error.HTTPError as exc:
        ui.warn(f"GitHub API 请求失败 ({exc.code})，跳过 sha256 校验")
        return None
    except Exception as exc:
        ui.warn(f"GitHub API 不可用（{exc.__class__.__name__}），跳过 sha256 校验")
        return None
    for item in data.get("assets", []):
        if item.get("name") == asset:
            digest = item.get("digest") or ""
            if digest.startswith("sha256:"):
                return digest.split(":", 1)[1]
    ui.warn(f"release {tag} 未提供 {asset} 的 sha256，跳过校验")
    return None


def verify_asset(zip_path, repo, tag, asset, ui, local=False):
    if local:
        ui.warn("🪄 使用本地包，跳过 sha256 校验")
        return
    ui.step("🔐", f"校验 sha256 {asset}")
    expected = fetch_digest_tag(repo, tag, asset, ui)
    if not expected:
        return
    actual = sha256_file(zip_path)
    if actual != expected:
        removed = discard_cached(zip_path)
        hint = (
            "已删除损坏缓存，重新运行 ./deps.py sync 即可重新下载"
            if removed
            else "本地包可能已损坏，请更换后重试"
        )
        raise DepsError(f"sha256 校验失败\n  期望: {expected}\n  实际: {actual}\n  {hint}")
    ui.ok("sha256 校验通过")


def extract_zip_restore(zf, dest):
    for info in zf.infolist():
        target = zf.extract(info, dest)
        mode = (info.external_attr >> 16) & 0o777
        if mode:
            try:
                os.chmod(target, mode)
            except OSError:
                pass


def ensure_bin_executable(prefix):
    root = Path(prefix)
    for sub in ("bin", "tools/bin", "tools/scripts"):
        bin_dir = root / sub
        if not bin_dir.is_dir():
            continue
        for path in bin_dir.rglob("*"):
            if path.is_file():
                mode = path.stat().st_mode & 0o777
                path.chmod(mode | 0o755)


def valid_deps_dir(path):
    path = Path(path)
    return (path / "include").is_dir() and (path / "lib").is_dir()


def install_thirdparty(zip_path, ui):
    tmp = Path(tempfile.mkdtemp(prefix=".deps-tmp-", dir=ROOT))
    try:
        try:
            with zipfile.ZipFile(zip_path) as zf:
                extract_zip_restore(zf, tmp)
        except zipfile.BadZipFile as exc:
            discard_cached(zip_path)
            raise DepsError(
                f"{Path(zip_path).name} 不是有效的 zip（可能下载不完整），"
                "请重新运行 ./deps.py sync 或 ./deps.py thirdparty"
            ) from exc
        src = tmp / "deps"
        if not src.is_dir():
            raise DepsError(f"{Path(zip_path).name} 内未找到顶层 deps/ 目录")
        if DEPS_DIR.exists():
            shutil.rmtree(DEPS_DIR)
        shutil.move(str(src), str(DEPS_DIR))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    if not valid_deps_dir(DEPS_DIR):
        raise DepsError("解压后校验失败：deps/ 缺少 include 或 lib")
    ensure_bin_executable(DEPS_DIR)


def install_cppjieba_headers(cpp_zip, limonp_zip, ui):
    tmp = Path(tempfile.mkdtemp(prefix=".cppjieba-tmp-", dir=ROOT))
    try:
        cpp_dir = tmp / "cppjieba"
        limonp_dir = tmp / "limonp"
        cpp_dir.mkdir()
        limonp_dir.mkdir()
        try:
            with zipfile.ZipFile(cpp_zip) as zf:
                extract_zip_restore(zf, cpp_dir)
            with zipfile.ZipFile(limonp_zip) as zf:
                extract_zip_restore(zf, limonp_dir)
        except zipfile.BadZipFile as exc:
            discard_cached(cpp_zip)
            discard_cached(limonp_zip)
            raise DepsError(
                "cppjieba/limonp 源码包不是有效的 zip（可能下载不完整），"
                "请重新运行 ./deps.py sync"
            ) from exc

        cpp_header = next(cpp_dir.rglob("include/cppjieba/Jieba.hpp"), None)
        limonp_header = next(limonp_dir.rglob("include/limonp/StringUtil.hpp"), None)
        if cpp_header is None:
            raise DepsError(f"{Path(cpp_zip).name} 内未找到 include/cppjieba/Jieba.hpp")
        if limonp_header is None:
            raise DepsError(f"{Path(limonp_zip).name} 内未找到 include/limonp/StringUtil.hpp")

        include_dir = DEPS_DIR / "include"
        include_dir.mkdir(parents=True, exist_ok=True)
        for dest in (CPPJIEBA_INCLUDE, LIMONP_INCLUDE):
            if dest.exists():
                shutil.rmtree(dest)
        shutil.copytree(cpp_header.parent, CPPJIEBA_INCLUDE)
        shutil.copytree(limonp_header.parent, LIMONP_INCLUDE)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    if not cppjieba_ready():
        raise DepsError("cppjieba 头文件安装后校验失败")


def sync_cppjieba(ui):
    cfg = cppjieba_config()
    if not cfg:
        return 0
    repo = cfg["repo"]
    tag = cfg["tag"]
    limonp_repo = cfg["limonp_repo"]
    limonp_ref = cfg.get("limonp_ref")
    if not limonp_ref:
        raise DepsError("dependencies.json 中 cppjieba 缺少 limonp_ref")

    cpp_zip = ensure_source_archive(repo, tag, source_asset_name("cppjieba", tag), ui)
    limonp_zip = ensure_source_archive(
        limonp_repo, limonp_ref, source_asset_name("limonp", limonp_ref), ui
    )
    install_cppjieba_headers(cpp_zip, limonp_zip, ui)
    ui.ok(
        f"✅ cppjieba 头文件已就绪 {ui.path('deps/include/cppjieba')} "
        f"(tag={tag}, limonp={limonp_ref[:7]})"
    )
    return 0


def sync_thirdparty(args, ui, sdk_cfg=None):
    tp = thirdparty_config()
    if not tp:
        return 0
    arch = resolve_tp_arch(tp, sdk_cfg)
    asset = tp["asset"].format(arch=arch)
    repo = tp["repo"]
    tag = tp.get("tag")
    if not tag:
        raise DepsError("dependencies.json 中 thirdparty 缺少 tag")

    if tp.get("path"):
        src = Path(tp["path"]).expanduser()
        if not valid_deps_dir(src):
            raise DepsError(f"thirdparty.path 不是有效的 deps 目录: {src}")
        ui.step("🪄", f"使用本地目录 {src}")
        if DEPS_DIR.exists():
            shutil.rmtree(DEPS_DIR)
        shutil.copytree(src, DEPS_DIR)
        ensure_bin_executable(DEPS_DIR)
    elif tp.get("archive"):
        zip_path = Path(tp["archive"]).expanduser()
        if not zip_path.is_file():
            raise DepsError(f"本地压缩包不存在: {zip_path}")
        ui.step("🪄", f"使用本地压缩包 {zip_path.name}")
        verify_asset(zip_path, repo, tag, asset, ui, local=True)
        install_thirdparty(zip_path, ui)
    else:
        zip_path = ensure_asset(repo, tag, asset, ui)
        verify_asset(zip_path, repo, tag, asset, ui)
        install_thirdparty(zip_path, ui)

    ui.ok(f"✅ 已同步三方依赖到 {ui.path('deps/')} (arch={arch})")
    return 0


def sync_deps(args, ui, sdk_cfg=None):
    rc = sync_thirdparty(args, ui, sdk_cfg)
    if rc:
        return rc
    return sync_cppjieba(ui)


def cmd_thirdparty(args, ui):
    sdk_cfg = None
    if MANIFEST.exists():
        try:
            sdk_cfg = resolve_config()
        except DepsError:
            sdk_cfg = None
    return sync_deps(args, ui, sdk_cfg)


def cmd_sync(args, ui):
    if not getattr(args, "thirdparty_only", False):
        rc = sync_sdk(args, ui)
        if rc:
            return rc
    if not getattr(args, "sdk_only", False):
        sdk_cfg = None
        if MANIFEST.exists():
            try:
                sdk_cfg = resolve_config()
            except DepsError:
                sdk_cfg = None
        rc = sync_deps(args, ui, sdk_cfg)
        if rc:
            return rc
    return 0


def sync_sdk(args, ui):
    cfg = resolve_config()
    version = require(cfg, "version")
    arch = require(cfg, "arch")
    if arch == "auto":
        arch = host_arch()
    repo = cfg.get("repo", DEFAULT_REPO)
    dest = sdk_dir(version)
    keep_asset = asset_name(version, arch)

    if dest.exists() and valid_sdk(dest) and not args.force:
        ensure_bin_executable(dest)
        ui.ok(f"✅ 当前已安装 {ui.path(dest.name)} (arch={arch})")
        report_gc(gc(dest, keep_asset, ui, args.keep_old), ui)
        return 0

    archive = cfg.get("archive")
    path = cfg.get("path")

    if path:
        ui.step("🪄", f"使用本地目录 {path}")
        install_dir(Path(path).expanduser(), version, ui)
        report_gc(gc(dest, None, ui, args.keep_old), ui)
    elif archive:
        zip_path = Path(archive).expanduser()
        if not zip_path.is_file():
            raise DepsError(f"本地压缩包不存在: {zip_path}")
        ui.step("🪄", f"使用本地压缩包 {zip_path.name}")
        verify_zip(zip_path, repo, version, zip_path.name, ui, local=True)
        install_zip(zip_path, version, ui)
        report_gc(gc(dest, None, ui, args.keep_old), ui)
    else:
        zip_path = ensure_download(repo, version, arch, ui)
        verify_zip(zip_path, repo, version, keep_asset, ui)
        install_zip(zip_path, version, ui)
        report_gc(gc(dest, keep_asset, ui, args.keep_old), ui)

    ui.ok(f"🔄 已切换到 {ui.path(dest.name)} (arch={arch})")
    return 0


def cmd_use(args, ui):
    if args.archive:
        return use_archive(args, ui)
    if not args.version:
        raise DepsError("use 需要 <version>，或使用 --archive <zip|dir>")
    data = load_manifest()
    data[SDK_KEY]["version"] = args.version
    if args.arch:
        data[SDK_KEY]["arch"] = args.arch
    data[SDK_KEY].setdefault("repo", DEFAULT_REPO)
    save_manifest(data)
    clear_local_override()
    arch = data[SDK_KEY]["arch"]
    ui.ok(f"已更新 dependencies.json: {SDK_KEY} {args.version} / {arch}")
    return cmd_sync(args, ui)


def use_archive(args, ui):
    src = Path(args.archive).expanduser()
    if not src.exists():
        raise DepsError(f"本地包不存在: {src}")
    arch = args.arch
    if src.is_dir():
        version = args.version or parse_version_dir(src)
        if not version:
            raise DepsError("无法从目录名解析版本，请用 --version 指定")
        install_dir(src, version, ui)
        write_override({"version": version, "arch": arch, "path": str(src.resolve())})
        keep_asset = None
    else:
        version = args.version
        parsed_version, parsed_arch = parse_asset(src)
        version = version or parsed_version
        arch = arch or parsed_arch
        if not version:
            raise DepsError("无法从文件名解析版本，请用 --version 指定")
        verify_zip(src, None, version, src.name, ui, local=True)
        install_zip(src, version, ui)
        write_override({"version": version, "arch": arch, "archive": str(src.resolve())})
        keep_asset = src.name
    dest = sdk_dir(version)
    report_gc(gc(dest, keep_asset, ui, args.keep_old), ui)
    arch_display = arch or "?"
    ui.ok(f"🪄 已切换到本地包 {ui.path(dest.name)} (arch={arch_display})")
    return 0


def write_override(extra):
    data = load_json(LOCAL_OVERRIDE)
    entry = data.get(SDK_KEY, {})
    entry.update({k: v for k, v in extra.items() if v is not None})
    data[SDK_KEY] = entry
    with LOCAL_OVERRIDE.open("w", encoding="utf-8") as fh:
        json.dump(data, fh, indent=2, ensure_ascii=False)
        fh.write("\n")


def cmd_current(args, ui):
    cfg = {}
    if MANIFEST.exists():
        cfg = resolve_config()
    source = "local override" if LOCAL_OVERRIDE.exists() else "dependencies.json"
    dirs = installed_dirs()
    for entry in dirs:
        version = parse_version_dir(entry) or "?"
        arch = parse_arch_dir(entry) or "?"
        marker = "✅" if valid_sdk(entry) else "⚠️"
        print(f"{marker} {ui.path(entry.name)}  version={version} arch={arch}")
    if not dirs:
        ui.warn("未发现已安装的 chen-sdk（运行 ./deps.py sync 下载）")
    if cfg:
        ui.info(f"配置: {SDK_KEY} {cfg.get('version')} / {cfg.get('arch')} (来源: {source})")
    if valid_deps_dir(DEPS_DIR):
        libs = sorted(p.name for p in (DEPS_DIR / "lib").glob("*.a"))
        tp = thirdparty_config() or {}
        ui.info(f"三方依赖 deps/ 就绪（{len(libs)} 个静态库, include 齐全）tag={tp.get('tag')}")
    else:
        ui.warn("三方依赖 deps/ 未就绪（运行 ./deps.py sync 或 ./deps.py thirdparty）")
    if cppjieba_ready():
        cfg_cpp = cppjieba_config() or {}
        ui.info(f"cppjieba 头文件就绪（tag={cfg_cpp.get('tag')}）")
    else:
        ui.warn("cppjieba 头文件未就绪（运行 ./deps.py sync 或 ./deps.py thirdparty）")
    return 0 if (dirs or valid_deps_dir(DEPS_DIR) or cppjieba_ready()) else 1


def cmd_list(args, ui):
    manifest = load_manifest()
    repo = manifest[SDK_KEY].get("repo", DEFAULT_REPO)
    url = f"https://api.github.com/repos/{repo}/releases"
    headers = {"Accept": "application/vnd.github+json", "User-Agent": "techhaven-deps"}
    token = os.environ.get("GITHUB_TOKEN") or os.environ.get("GH_TOKEN")
    if token:
        headers["Authorization"] = f"Bearer {token}"
    ui.step("🌐", f"查询 {repo} 的 Release")
    try:
        req = urllib.request.Request(url, headers=headers)
        with urllib.request.urlopen(req, timeout=20) as resp:
            releases = json.load(resp)
    except Exception as exc:
        raise DepsError(f"查询 Release 失败: {exc}") from exc
    if not releases:
        ui.warn("没有可用 Release")
        return 0
    for release in releases:
        tag = release.get("tag_name", "?")
        assets = [
            a.get("name")
            for a in release.get("assets", [])
            if isinstance(a.get("name"), str)
            and (a["name"].startswith(SDK_DIR_PREFIX) or a["name"].startswith("deps-"))
        ]
        print(f"🏷️  {ui.path(tag)}")
        for name in assets:
            version, arch = parse_asset(name)
            extra = f"  version={version} arch={arch}" if version else ""
            print(f"     {name}{extra}")
    return 0


def cmd_clean(args, ui):
    removed = []
    for entry in installed_dirs():
        if entry.is_symlink():
            entry.unlink()
        else:
            shutil.rmtree(entry)
        removed.append(entry.name)
    if DEPS_DIR.exists():
        shutil.rmtree(DEPS_DIR)
        removed.append("deps/")
    for zip_file in sorted(ROOT.glob(f"{SDK_DIR_PREFIX}*.zip")):
        zip_file.unlink()
        removed.append(zip_file.name)
    if CACHE_DIR.is_dir():
        shutil.rmtree(CACHE_DIR)
        removed.append(".deps-cache/")
    if LOCAL_OVERRIDE.exists():
        LOCAL_OVERRIDE.unlink()
        removed.append(LOCAL_OVERRIDE.name)
    if not removed:
        ui.info("没有需要清理的内容")
        return 0
    ui.ok(f"🧹 已清理: {', '.join(removed)}")
    return 0


def build_parser():
    parser = argparse.ArgumentParser(
        prog="deps.py",
        description="按 dependencies.json 管理 chen-sdk 依赖（下载 / 切换 / 清理）",
    )
    parser.add_argument("--no-color", action="store_true", help="禁用彩色输出")
    parser.add_argument("--keep-old", action="store_true", help="切换后保留旧版本，不清理")
    sub = parser.add_subparsers(dest="command")

    p_sync = sub.add_parser("sync", aliases=["fetch", "install"], help="下载 chen-sdk、三方依赖与 cppjieba")
    p_sync.add_argument("--force", action="store_true", help="即使已安装也重新下载")
    p_sync.add_argument("--sdk-only", action="store_true", help="只同步 chen-sdk")
    p_sync.add_argument("--thirdparty-only", action="store_true", help="只同步三方依赖与 cppjieba")
    p_sync.set_defaults(func=cmd_sync)

    p_tp = sub.add_parser("thirdparty", aliases=["deps"], help="只下载三方依赖与 cppjieba 到 deps/")
    p_tp.set_defaults(func=cmd_thirdparty)

    p_use = sub.add_parser("use", help="切换版本，或用本地包安装")
    p_use.add_argument("version", nargs="?", help="目标版本号，如 1.5.3")
    p_use.add_argument("--arch", help="目标架构 arm64 / amd64")
    p_use.add_argument("--archive", help="本地 zip 或目录，作为临时 SDK 包")
    p_use.add_argument("--force", action="store_true", help="即使已安装也重新下载")
    p_use.set_defaults(func=cmd_use)

    p_current = sub.add_parser("current", help="显示当前已安装的 SDK")
    p_current.set_defaults(func=cmd_current)

    p_list = sub.add_parser("list", aliases=["ls"], help="列出远端可用版本与架构")
    p_list.set_defaults(func=cmd_list)

    p_clean = sub.add_parser("clean", help="删除所有已安装 SDK 与缓存")
    p_clean.set_defaults(func=cmd_clean)

    return parser


def main(argv=None):
    parser = build_parser()
    args = parser.parse_args(argv)
    ui = UI(color=not args.no_color and not os.environ.get("NO_COLOR"))
    if not getattr(args, "command", None):
        parser.print_help()
        return 0
    try:
        return args.func(args, ui)
    except DepsError as exc:
        ui.error(str(exc))
        return 1
    except KeyboardInterrupt:
        ui.error("已取消")
        return 130


if __name__ == "__main__":
    sys.exit(main())
