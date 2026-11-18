#include "common/frame/CrashHandler.hpp"

#include <chen/log/log.h>
#include <chen/util/fs_util.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <execinfo.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

namespace blog {

static const size_t kAltStackSize = 64 * 1024;
alignas(16) static char g_alt_stack[kAltStackSize];
static int g_crash_fd = -1;
static bool g_installed = false;

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
    default:
        return "UNKNOWN";
    }
}

static void CrashHandler(int sig, siginfo_t* info, void* context)
{
    (void)context;
    // 兜底：如果 backtrace 在崩溃上下文中卡住，2 秒后由 SIGALRM 默认处理终止进程
    alarm(2);
    if (g_crash_fd >= 0) {
        char header[256];
        int len = snprintf(
            header, sizeof(header), "\n=== CRASH signal=%d(%s) pid=%d addr=%p ===\n", sig, SignalName(sig), (int)getpid(),
            info ? info->si_addr : nullptr);
        if (len > 0) {
            ssize_t ignored = write(g_crash_fd, header, static_cast<size_t>(len));
            (void)ignored;
        }
        void* frames[64];
        int count = backtrace(frames, 64);
        backtrace_symbols_fd(frames, count, g_crash_fd);
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
