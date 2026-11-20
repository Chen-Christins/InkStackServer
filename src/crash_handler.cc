// ucontext_t 里的 uc_mcontext / REG_RIP 需要 _GNU_SOURCE 才会暴露
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "crash_handler.h"

#include <chen/log/log.h>
#include <chen/util/fs_util.h>

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <execinfo.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/syscall.h>
#include <time.h>
#include <ucontext.h>
#include <unistd.h>

namespace blog {

static const size_t kAltStackSize = 64 * 1024;
/// 信号处理里最多 dump 的栈字数量（8 字节一个字）
static const size_t kStackWords = 48;

alignas(16) static char g_alt_stack[kAltStackSize];
static int g_crash_fd = -1;
static bool g_installed = false;
/// 本模块的加载基址与路径，用于把 pc 换算成模块内偏移，方便离线 addr2line
static void* g_module_base = nullptr;
static char g_module_path[256] = {0};

static const char* SignalName(int sig)
{
    switch (sig) {
    case SIGSEGV:
        return "SIGSEGV";
    case SIGABRT:
        return "SIGABRT";
    case SIGBUS:
        return "SIGBUS";
    case SIGFPE:
        return "SIGFPE";
    case SIGILL:
        return "SIGILL";
    default:
        return "UNKNOWN";
    }
}

/**
 * @brief 把 UTC 秒转成 `YYYY-MM-DD HH:MM:SS`
 * @note 信号处理里不能用 gmtime_r / localtime_r（可能加锁或读 TZ），这里用纯算术实现
 */
static void FormatUtc(int64_t sec, char* out, size_t len)
{
    int64_t days = sec / 86400;
    int64_t sod = sec % 86400;
    if (sod < 0) {
        sod += 86400;
        days -= 1;
    }
    // Howard Hinnant 的 civil_from_days 算法
    const int64_t z = days;
    const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const int64_t doe = z - era * 146097;
    const int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int64_t year = yoe + era * 400;
    const int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const int64_t mp = (5 * doy + 2) / 153;
    const int64_t day = doy - (153 * mp + 2) / 5 + 1;
    const int64_t month = mp < 10 ? mp + 3 : mp - 9;
    if (month <= 2) {
        year += 1;
    }
    snprintf(
        out, len, "%04lld-%02lld-%02lld %02lld:%02lld:%02lld", (long long)year, (long long)month, (long long)day, (long long)(sod / 3600),
        (long long)((sod % 3600) / 60), (long long)(sod % 60));
}

/// 只用 write()，避免信号处理里碰 stdio 缓冲
static void WriteCrashLog(const char* buf, int len)
{
    if (g_crash_fd < 0 || !buf || len <= 0) {
        return;
    }
    size_t off = 0;
    while (off < static_cast<size_t>(len)) {
        ssize_t n = write(g_crash_fd, buf + off, static_cast<size_t>(len) - off);
        if (n <= 0) {
            if (n < 0 && errno == EINTR) {
                continue;
            }
            break;
        }
        off += static_cast<size_t>(n);
    }
}

struct CrashRegs {
    uintptr_t pc = 0;
    uintptr_t sp = 0;
    uintptr_t fp = 0;
};

static CrashRegs GetCrashRegs(void* context)
{
    CrashRegs regs;
    if (!context) {
        return regs;
    }
    auto* uc = static_cast<ucontext_t*>(context);
#if defined(__x86_64__)
    regs.pc = static_cast<uintptr_t>(uc->uc_mcontext.gregs[REG_RIP]);
    regs.sp = static_cast<uintptr_t>(uc->uc_mcontext.gregs[REG_RSP]);
    regs.fp = static_cast<uintptr_t>(uc->uc_mcontext.gregs[REG_RBP]);
#elif defined(__aarch64__)
    regs.pc = static_cast<uintptr_t>(uc->uc_mcontext.pc);
    regs.sp = static_cast<uintptr_t>(uc->uc_mcontext.sp);
    regs.fp = static_cast<uintptr_t>(uc->uc_mcontext.regs[29]);
#else
    (void)uc;
#endif
    return regs;
}

/// 栈被破坏时 backtrace 往往只剩信号帧，这里把 sp 附近的栈字一并 dump 出来人工还原
static void DumpStack(uintptr_t sp)
{
    if (!sp) {
        return;
    }
    const uintptr_t* words = reinterpret_cast<const uintptr_t*>(sp);
    char line[160];
    for (size_t i = 0; i < kStackWords; i += 4) {
        int len = snprintf(line, sizeof(line), "    [%p]", (const void*)(words + i));
        if (len < 0) {
            return;
        }
        for (size_t j = 0; j < 4 && i + j < kStackWords; ++j) {
            const int n = snprintf(line + len, sizeof(line) - static_cast<size_t>(len), " %016lx", (unsigned long)words[i + j]);
            if (n < 0) {
                break;
            }
            len += n;
            if (static_cast<size_t>(len) >= sizeof(line)) {
                break;
            }
        }
        if (static_cast<size_t>(len) >= sizeof(line)) {
            len = static_cast<int>(sizeof(line)) - 1;
        }
        line[len] = '\0';
        WriteCrashLog(line, len);
    }
}

static void CrashHandler(int sig, siginfo_t* info, void* context)
{
    // 兜底：如果 backtrace / 栈 dump 在崩溃上下文中卡住，2 秒后由 SIGALRM 默认处理终止进程
    alarm(2);

    const CrashRegs regs = GetCrashRegs(context);

    char time_str[32] = {0};
    struct timespec now;
    long msec = 0;
    if (clock_gettime(CLOCK_REALTIME, &now) == 0) {
        FormatUtc(static_cast<int64_t>(now.tv_sec), time_str, sizeof(time_str));
        msec = now.tv_nsec / 1000000;
    }

    unsigned long pc_offset = 0;
    if (g_module_base && regs.pc >= reinterpret_cast<uintptr_t>(g_module_base)) {
        pc_offset = static_cast<unsigned long>(regs.pc - reinterpret_cast<uintptr_t>(g_module_base));
    }

    char header[640];
    int len = snprintf(
        header, sizeof(header),
        "\n=== CRASH time=%s.%03ld utc signal=%d(%s) pid=%d tid=%ld addr=%p si_code=%d\n"
        "    module=%s base=%p pc=%p pc_offset=0x%lx sp=%p fp=%p ===\n",
        time_str, msec, sig, SignalName(sig), (int)getpid(), (long)syscall(SYS_gettid), info ? info->si_addr : nullptr, info ? info->si_code : 0,
        g_module_path[0] ? g_module_path : "-", g_module_base, (void*)regs.pc, pc_offset, (void*)regs.sp, (void*)regs.fp);
    if (len < 0) {
        len = 0;
    } else if (static_cast<size_t>(len) >= sizeof(header)) {
        len = static_cast<int>(sizeof(header)) - 1;
    }
    WriteCrashLog(header, len);

    void* frames[64];
    int count = backtrace(frames, 64);
    backtrace_symbols_fd(frames, count, g_crash_fd);

    if (regs.sp) {
        static const char stack_title[] = "=== STACK (sp...) ===\n";
        WriteCrashLog(stack_title, static_cast<int>(sizeof(stack_title) - 1));
        DumpStack(regs.sp);
    }

    // 恢复默认处理并重新抛出，让 master 继续按 signal 崩溃处理
    signal(sig, SIG_DFL);
    raise(sig);
}

bool InstallCrashHandler(const std::string& log_path)
{
    if (g_installed) {
        return true;
    }

    // 普通上下文用 FSUtil 确保目录存在；信号处理里仍然只能用 fd 系统调用
    std::string dir = chen::FSUtil::Dirname(log_path);
    if (!dir.empty()) {
        chen::FSUtil::Mkdir(dir);
    }

    g_crash_fd = open(log_path.c_str(), O_CREAT | O_WRONLY | O_APPEND, 0644);
    if (g_crash_fd < 0) {
        ERROR(LOG_ROOT()) << "open crash log failed, path=" << log_path << " errno=" << errno;
        return false;
    }

    // 记录模块基址，便于把 pc_offset 直接丢给 addr2line
    Dl_info dl_info;
    memset(&dl_info, 0, sizeof(dl_info));
    if (dladdr(reinterpret_cast<void*>(&InstallCrashHandler), &dl_info) != 0) {
        g_module_base = dl_info.dli_fbase;
        if (dl_info.dli_fname) {
            snprintf(g_module_path, sizeof(g_module_path), "%s", dl_info.dli_fname);
        }
    }

    stack_t ss;
    memset(&ss, 0, sizeof(ss));
    ss.ss_sp = g_alt_stack;
    ss.ss_size = sizeof(g_alt_stack);
    ss.ss_flags = 0;
    if (sigaltstack(&ss, nullptr) != 0) {
        WARN(LOG_ROOT()) << "sigaltstack failed, errno=" << errno;
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = CrashHandler;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_RESETHAND;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGABRT, &sa, nullptr);
    sigaction(SIGBUS, &sa, nullptr);
    sigaction(SIGFPE, &sa, nullptr);
    sigaction(SIGILL, &sa, nullptr);

    g_installed = true;
    return true;
}

void UninstallCrashHandler()
{
    if (!g_installed) {
        return;
    }
    signal(SIGSEGV, SIG_DFL);
    signal(SIGABRT, SIG_DFL);
    signal(SIGBUS, SIG_DFL);
    signal(SIGFPE, SIG_DFL);
    signal(SIGILL, SIG_DFL);
    stack_t ss;
    memset(&ss, 0, sizeof(ss));
    ss.ss_flags = SS_DISABLE;
    sigaltstack(&ss, nullptr);
    if (g_crash_fd >= 0) {
        close(g_crash_fd);
        g_crash_fd = -1;
    }
    g_installed = false;
}

} // namespace blog
