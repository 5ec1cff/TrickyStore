#include <vector>
#include <sys/mman.h>
#include <sys/sysmacros.h>
#include <array>
#include <cinttypes>
#include <sys/ptrace.h>
#include <unistd.h>
#include <sys/uio.h>
#include <sys/auxv.h>
#include <elf.h>
#include <link.h>
#include <vector>
#include <string>
#include <sys/mman.h>
#include <sys/wait.h>
#include <cstdlib>
#include <cstdio>
#include <dlfcn.h>
#include <csignal>
#include <cstring>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <cinttypes>
#include <sys/xattr.h>
#include <random>

#include "utils.hpp"
#include "logging.hpp"
#include <sched.h>
#include <fcntl.h>

bool switch_mnt_ns(int pid, int *fd) {
    int nsfd, old_nsfd = -1;
    std::string path;
    if (pid == 0) {
        if (fd != nullptr) {
            nsfd = *fd;
            *fd = -1;
        } else return false;
        path = "/proc/self/fd/";
        path += std::to_string(nsfd);
    } else {
        if (fd != nullptr) {
            old_nsfd = open("/proc/self/ns/mnt", O_RDONLY | O_CLOEXEC);
            if (old_nsfd == -1) {
                PLOGE("get old nsfd");
                return false;
            }
            *fd = old_nsfd;
        }
        path = std::string("/proc/") + std::to_string(pid) + "/ns/mnt";
        nsfd = open(path.c_str(), O_RDONLY | O_CLOEXEC);
        if (nsfd == -1) {
            PLOGE("open nsfd %s", path.c_str());
            close(old_nsfd);
            return false;
        }
    }
    if (setns(nsfd, CLONE_NEWNS) == -1) {
        PLOGE("set ns to %s", path.c_str());
        close(nsfd);
        close(old_nsfd);
        return false;
    }
    close(nsfd);
    return true;
}

ssize_t write_proc(int pid, uintptr_t remote_addr, const void *buf, size_t len, bool use_proc_mem) {
    LOGV("write to %d addr %" PRIxPTR " size %zu use_proc_mem=%d", pid, remote_addr, len, use_proc_mem);
    if (use_proc_mem) {
        char path[64];
        snprintf(path, sizeof(path), "/proc/%d/mem", pid);
        auto fd = open(path, O_WRONLY | O_CLOEXEC);
        if (fd == -1) {
            PLOGE("open proc mem");
            return -1;
        }
        auto l = pwrite(fd, buf, len, remote_addr);
        if (l == -1) {
            PLOGE("pwrite");
        } else if (static_cast<size_t>(l) != len) {
            LOGW("not fully written: %zu, excepted %zu", l, len);
        }
        return l;
    } else {
        struct iovec local{
                .iov_base = (void *) buf,
                .iov_len = len
        };
        struct iovec remote{
                .iov_base = (void *) remote_addr,
                .iov_len = len
        };
        auto l = process_vm_writev(pid, &local, 1, &remote, 1, 0);
        if (l == -1) {
            PLOGE("process_vm_writev");
        } else if (static_cast<size_t>(l) != len) {
            LOGW("not fully written: %zu, excepted %zu", l, len);
        }
        return l;
    }
}

ssize_t read_proc(int pid, uintptr_t remote_addr, void *buf, size_t len) {
    struct iovec local{
            .iov_base = (void *) buf,
            .iov_len = len
    };
    struct iovec remote{
            .iov_base = (void *) remote_addr,
            .iov_len = len
    };
    auto l = process_vm_readv(pid, &local, 1, &remote, 1, 0);
    if (l == -1) {
        PLOGE("process_vm_readv");
    } else if (static_cast<size_t>(l) != len) {
        LOGW("not fully read: %zu, excepted %zu", l, len);
    }
    return l;
}

bool get_regs(int pid, struct user_regs_struct &regs) {
#if defined(__x86_64__) || defined(__i386__)
    if (ptrace(PTRACE_GETREGS, pid, 0, &regs) == -1) {
        PLOGE("getregs");
        return false;
    }
#elif defined(__aarch64__) || defined(__arm__)
    struct iovec iov = {
            .iov_base = &regs,
            .iov_len = sizeof(struct user_regs_struct),
    };
    if (ptrace(PTRACE_GETREGSET, pid, NT_PRSTATUS, &iov) == -1) {
        PLOGE("getregs");
        return false;
    }
#endif
    return true;
}

bool set_regs(int pid, struct user_regs_struct &regs) {
#if defined(__x86_64__) || defined(__i386__)
    if (ptrace(PTRACE_SETREGS, pid, 0, &regs) == -1) {
        PLOGE("setregs");
        return false;
    }
#elif defined(__aarch64__) || defined(__arm__)
    struct iovec iov = {
            .iov_base = &regs,
            .iov_len = sizeof(struct user_regs_struct),
    };
    if (ptrace(PTRACE_SETREGSET, pid, NT_PRSTATUS, &iov) == -1) {
        PLOGE("setregs");
        return false;
    }
#endif
    return true;
}

std::string get_addr_mem_region(std::vector<lsplt::MapInfo> &info, uintptr_t addr) {
    for (auto &map: info) {
        if (map.start <= addr && map.end > addr) {
            auto s = std::string(map.path);
            s += ' ';
            s += map.perms & PROT_READ ? 'r' : '-';
            s += map.perms & PROT_WRITE ? 'w' : '-';
            s += map.perms & PROT_EXEC ? 'x' : '-';
            return s;
        }
    }
    return "<unknown>";
}


void *find_module_return_addr(std::vector<lsplt::MapInfo> &info, std::string_view suffix) {
    for (auto &map: info) {
        if ((map.perms & PROT_EXEC) == 0 && map.path.ends_with(suffix)) {
            return (void *) map.start;
        }
    }
    return nullptr;
}

void *find_module_base(std::vector<lsplt::MapInfo> &info, std::string_view suffix) {
    for (auto &map: info) {
        if (map.offset == 0 && map.path.ends_with(suffix)) {
            return (void *) map.start;
        }
    }
    return nullptr;
}

void *find_func_addr(
        std::vector<lsplt::MapInfo> &local_info,
        std::vector<lsplt::MapInfo> &remote_info,
        std::string_view module,
        std::string_view func) {
    auto lib = dlopen(module.data(), RTLD_NOW);
    if (lib == nullptr) {
        LOGE("failed to open lib %s: %s", module.data(), dlerror());
        return nullptr;
    }
    auto sym = reinterpret_cast<uint8_t *>(dlsym(lib, func.data()));
    if (sym == nullptr) {
        LOGE("failed to find sym %s in %s: %s", func.data(), module.data(), dlerror());
        dlclose(lib);
        return nullptr;
    }
    LOGD("sym %s: %p", func.data(), sym);
    dlclose(lib);
    auto local_base = reinterpret_cast<uint8_t *>(find_module_base(local_info, module));
    if (local_base == nullptr) {
        LOGE("failed to find local base for module %s", module.data());
        return nullptr;
    }
    auto remote_base = reinterpret_cast<uint8_t *>(find_module_base(remote_info, module));
    if (remote_base == nullptr) {
        LOGE("failed to find remote base for module %s", module.data());
        return nullptr;
    }
    LOGD("found local base %p remote base %p", local_base, remote_base);
    auto addr = (sym - local_base) + remote_base;
    LOGD("addr %p", addr);
    return addr;
}

void align_stack(struct user_regs_struct &regs, uintptr_t preserve) {
    regs.REG_SP = (regs.REG_SP - preserve) & ~0xf;
}

uintptr_t push_memory(int pid, struct user_regs_struct &regs, void* local_addr, size_t len) {
    regs.REG_SP -= len;
    align_stack(regs);
    auto addr = static_cast<uintptr_t>(regs.REG_SP);
    if (!write_proc(pid, addr, local_addr, len)) {
        LOGE("failed to write mem %p+%zu", local_addr, len);
    }
    LOGD("pushed mem %" PRIxPTR, addr);
    return addr;
}

uintptr_t push_string(int pid, struct user_regs_struct &regs, const char *str) {
    auto len = strlen(str) + 1;
    regs.REG_SP -= len;
    align_stack(regs);
    auto addr = static_cast<uintptr_t>(regs.REG_SP);
    if (!write_proc(pid, addr, str, len)) {
        LOGE("failed to write string %s", str);
    }
    LOGD("pushed string %" PRIxPTR, addr);
    return addr;
}

bool remote_pre_call(int pid, struct user_regs_struct &regs, uintptr_t func_addr, uintptr_t return_addr,
                     std::vector<uintptr_t> &args) {
    align_stack(regs);
    LOGV("calling remote function %" PRIxPTR " args %zu", func_addr, args.size());
    for (auto &a: args) {
        LOGV("arg %p", (void *) a);
    }
#if defined(__x86_64__)
    if (args.size() >= 1) {
        regs.rdi = args[0];
    }
    if (args.size() >= 2) {
        regs.rsi = args[1];
    }
    if (args.size() >= 3) {
        regs.rdx = args[2];
    }
    if (args.size() >= 4) {
        regs.rcx = args[3];
    }
    if (args.size() >= 5) {
        regs.r8 = args[4];
    }
    if (args.size() >= 6) {
        regs.r9 = args[5];
    }
    if (args.size() > 6) {
        auto remain = (args.size() - 6) * sizeof(uintptr_t);
        align_stack(regs, remain);
        if (!write_proc(pid, (uintptr_t) regs.REG_SP, args.data(), remain)) {
            LOGE("failed to push arguments");
        }
    }
    regs.REG_SP -= sizeof(uintptr_t);
    if (!write_proc(pid, (uintptr_t) regs.REG_SP, &return_addr, sizeof(return_addr))) {
        LOGE("failed to write return addr");
    }
    regs.REG_IP = func_addr;
#elif defined(__i386__)
    if (args.size() > 0) {
        auto remain = (args.size()) * sizeof(uintptr_t);
        align_stack(regs, remain);
        if (!write_proc(pid, (uintptr_t) regs.REG_SP, args.data(), remain)) {
            LOGE("failed to push arguments");
        }
    }
    regs.REG_SP -= sizeof(uintptr_t);
    if (!write_proc(pid, (uintptr_t) regs.REG_SP, &return_addr, sizeof(return_addr))) {
        LOGE("failed to write return addr");
    }
    regs.REG_IP = func_addr;
#elif defined(__aarch64__)
    for (size_t i = 0; i < args.size() && i < 8; i++) {
        regs.regs[i] = args[i];
    }
    if (args.size() > 8) {
        auto remain = (args.size() - 8) * sizeof(uintptr_t);
        align_stack(regs, remain);
        write_proc(pid, (uintptr_t)regs.REG_SP, args.data(), remain);
    }
    regs.regs[30] = return_addr;
    regs.REG_IP = func_addr;
#elif defined(__arm__)
    for (size_t i = 0; i < args.size() && i < 4; i++) {
        regs.uregs[i] = args[i];
    }
    if (args.size() > 4) {
        auto remain = (args.size() - 4) * sizeof(uintptr_t);
        align_stack(regs, remain);
        write_proc(pid, (uintptr_t)regs.REG_SP, args.data(), remain);
    }
    regs.uregs[14] = return_addr;
    regs.REG_IP = func_addr;
    constexpr auto CPSR_T_MASK = 1lu << 5;
    if ((regs.REG_IP & 1) != 0) {
        regs.REG_IP = regs.REG_IP & ~1;
        regs.uregs[16] = regs.uregs[16] | CPSR_T_MASK;
    } else {
        regs.uregs[16] = regs.uregs[16] & ~CPSR_T_MASK;
    }
#endif
    if (!set_regs(pid, regs)) {
        LOGE("failed to set regs");
        return false;
    }
    return ptrace(PTRACE_CONT, pid, 0, 0) == 0;
}

uintptr_t remote_post_call(int pid, struct user_regs_struct &regs, uintptr_t return_addr) {
    int status;
    wait_for_trace(pid, &status, __WALL);
    if (!get_regs(pid, regs)) {
        LOGE("failed to get regs after call");
        return 0;
    }
    if (WSTOPSIG(status) == SIGSEGV) {
        if (static_cast<uintptr_t>(regs.REG_IP) != return_addr) {
            LOGE("wrong return addr %p", (void *) regs.REG_IP);
            siginfo_t siginfo;
            if (ptrace(PTRACE_GETSIGINFO, pid, 0, &siginfo) == -1) {
                PLOGE("failed to get siginfo");
            } else {
                LOGE("si_code=%d si_addr=%p", siginfo.si_code, siginfo.si_addr);
            }
            return 0;
        }
        return regs.REG_RET;
    } else {
        LOGE("stopped by other reason %s at addr %p", parse_status(status).c_str(), (void*) regs.REG_IP);
    }
    return 0;
}

uintptr_t remote_call(int pid, struct user_regs_struct &regs, uintptr_t func_addr, uintptr_t return_addr,
                      std::vector<uintptr_t> &args) {
    if (!remote_pre_call(pid, regs, func_addr, return_addr, args)) return 0;
    return remote_post_call(pid, regs, return_addr);
}

int fork_dont_care() {
    auto pid = fork();
    if (pid < 0) {
        PLOGE("fork 1");
    } else if (pid == 0) {
        pid = fork();
        if (pid < 0) {
            PLOGE("fork 2");
        } else if (pid > 0) {
            exit(0);
        }
    } else {
        int status;
        waitpid(pid, &status, __WALL);
    }
    return pid;
}

bool wait_for_trace(int pid, int* status, int flags) {
    while (true) {
        auto result = waitpid(pid, status, flags);
        if (result == -1) {
            if (errno == EINTR) {
                continue;
            } else {
                PLOGE("wait %d failed", pid);
                return false;
            }
        }
        if (!WIFSTOPPED(*status)) {
            LOGE("process %d not stopped for trace: %s, exit", pid, parse_status(*status).c_str());
            return false;
        }
        return true;
    }
}

std::string parse_status(int status) {
    char buf[64];
    if (WIFEXITED(status)) {
        snprintf(buf, sizeof(buf), "0x%x exited with %d", status, WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        snprintf(buf, sizeof(buf), "0x%x signaled with %s(%d)", status, sigabbrev_np(WTERMSIG(status)), WTERMSIG(status));
    } else if (WIFSTOPPED(status)) {
        auto stop_sig = WSTOPSIG(status);
        snprintf(buf, sizeof(buf), "0x%x stopped by signal=%s(%d),event=%s", status, sigabbrev_np(stop_sig), stop_sig, parse_ptrace_event(status));
    } else {
        snprintf(buf, sizeof(buf), "0x%x unknown", status);
    }
    return buf;
}

std::string get_program(int pid) {
    std::string path = "/proc/";
    path += std::to_string(pid);
    path += "/exe";
    constexpr const auto SIZE = 256;
    char buf[SIZE + 1];
    auto sz = readlink(path.c_str(), buf, SIZE);
    if (sz == -1) {
        PLOGE("readlink /proc/%d/exe", pid);
        return "";
    }
    buf[sz] = 0;
    return buf;
}

std::string parse_exec(int pid) {
    struct user_regs_struct regs;
    if (!get_regs(pid, regs)) return "";
    auto nr = regs.REG_NR;
    if (nr != SYS_execve) {
        // LOGI("syscall %ld != %d", nr, SYS_execve);
        return "";
    }
    auto file_name_ptr = regs.REG_SYS_ARG0;
    char buf[128];
    auto sz = read_proc(pid, file_name_ptr, buf, sizeof(buf));
    if (sz < 0) return "";
    for (auto i = 0; i < sz; i++) {
        if (buf[i] == 0) {
            LOGD("exec len %d prog %s", i, buf);
            return buf;
        }
    }
    // too long
    return "";
}

bool skip_syscall(int pid) {
    struct user_regs_struct regs;
    if (!get_regs(pid, regs)) return false;
#if defined(__aarch64__)
    // https://stackoverflow.com/questions/63620203/ptrace-change-syscall-number-arm64
    int syscallno = 0xffff;
    struct iovec iov = {
            .iov_base = &syscallno,
            .iov_len = sizeof (int),
    };
    if (ptrace(PTRACE_SETREGSET, pid, NT_ARM_SYSTEM_CALL, &iov) == -1) {
        PLOGE("failed to set syscall");
    }
#elif defined(__arm__)
    if (ptrace(PTRACE_SET_SYSCALL, pid, 0, 0xffff) == -1) {
        PLOGE("failed to set syscall");
    }
#else
    auto orig_nr = regs.REG_NR;
    regs.REG_NR = 0xffff;
    if (!set_regs(pid, regs)) return false;
    regs.REG_NR = orig_nr;
#endif
    if (ptrace(PTRACE_SYSCALL, pid, 0, 0) == -1) {
        PLOGE("failed to singlestep");
        return false;
    }
    int status;
    waitpid(pid, &status, __WALL);
#if defined(__x86_64__)
    regs.REG_IP -= 2;
    regs.rax = orig_nr;
#elif defined(__i386__)
    regs.REG_IP -= 2;
    regs.eax = orig_nr;
#elif defined(__aarch64__)
    regs.REG_IP -= 4;
#elif defined(__arm__)
    regs.REG_IP -= (regs.REG_IP % 2) ? 2 : 4;
#endif
    return set_regs(pid, regs);
}

void wait_for_syscall(int pid) {
    int status;
    for (;;) {
        if (!wait_for_trace(pid, &status, __WALL)) {
            LOGE("could not wait for trace");
            exit(1);
        }
        if (WIFSTOPPED(status) && WSTOPSIG(status) == (SIGTRAP | 0x80)) {
            LOGV("!! stopped at syscall");
            break;
        }
        LOGV("! not syscall: %s", parse_status(status).c_str());
        if (ptrace(PTRACE_CONT, pid, 0, WSTOPSIG(status)) == -1) {
            PLOGE("failed to cont");
            exit(1);
        }
    }
}

bool do_syscall(int pid, uintptr_t &ret, int nr, uintptr_t arg0, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, uintptr_t arg4, uintptr_t arg5) {
#if defined(__i386__)
    LOGE("do_syscall is unsupported on i386!");
    return false;
#else
    struct user_regs_struct regs, backup_regs;
    if (ptrace(PTRACE_SYSCALL, pid, 0, 0) == -1) {
        PLOGE("failed to singlestep");
        return false;
    }
    wait_for_syscall(pid);
    if (!get_regs(pid, regs)) return false;
    memcpy(&backup_regs, &regs, sizeof(struct user_regs_struct));
    // set syscall nr and args
#if defined(__aarch64__)
    // https://stackoverflow.com/questions/63620203/ptrace-change-syscall-number-arm64
    int syscallno = nr;
    struct iovec iov = {
            .iov_base = &syscallno,
            .iov_len = sizeof (int),
    };
    if (ptrace(PTRACE_SETREGSET, pid, NT_ARM_SYSTEM_CALL, &iov) == -1) {
        PLOGE("failed to set syscall");
        return false;
    }
    regs.regs[0] = arg0;
    regs.regs[1] = arg1;
    regs.regs[2] = arg2;
    regs.regs[3] = arg3;
    regs.regs[4] = arg4;
    regs.regs[5] = arg5;
#elif defined(__arm__)
    if (ptrace(PTRACE_SET_SYSCALL, pid, 0, nr) == -1) {
        PLOGE("failed to set syscall");
        return false;
    }
    regs.uregs[0] = arg0;
    regs.uregs[1] = arg1;
    regs.uregs[2] = arg2;
    regs.uregs[3] = arg3;
    regs.uregs[4] = arg4;
    regs.uregs[5] = arg5;
#elif defined(__x86_64__)
    auto orig_nr = regs.REG_NR;
    regs.REG_NR = nr;
    regs.rdi = arg0;
    regs.rsi = arg1;
    regs.rdx = arg2;
    regs.r10 = arg3;
    regs.r8 = arg4;
    regs.r9 = arg5;
#endif
    if (!set_regs(pid, regs)) return false;
    if (ptrace(PTRACE_SYSCALL, pid, 0, 0) == -1) {
        PLOGE("failed to singlestep");
        return false;
    }
    wait_for_syscall(pid);
    if (!get_regs(pid, regs)) return false;
    // go back to last instruction
    // on x86, we should fill the nr register with orig_nr
#if defined(__x86_64__)
    ret = regs.rax;
    backup_regs.REG_IP -= 2;
    backup_regs.rax = orig_nr;
#elif defined(__aarch64__)
    ret = regs.regs[0];
    backup_regs.REG_IP -= 4;
#elif defined(__arm__)
    ret = regs.uregs[0];
    backup_regs.REG_IP -= (regs.REG_IP % 2) ? 2 : 4;
#endif
    return set_regs(pid, backup_regs);
#endif // i386
}

uintptr_t remote_syscall(int pid, uintptr_t &ret, int nr, uintptr_t arg0 = 0, uintptr_t arg1 = 0, uintptr_t arg2 = 0, uintptr_t arg3 = 0, uintptr_t arg4 = 0, uintptr_t arg5 = 0) {
    if (!do_syscall(pid, ret, nr, arg0, arg1, arg2, arg3, arg4, arg5)) {
        LOGE("do remote syscall %d failed", nr);
        return -1;
    }
    if (SYSCALL_IS_ERR(ret)) {
        LOGE("do remote syscall %d return error %d %s", nr, SYSCALL_ERR(ret), strerror(SYSCALL_ERR(ret)));
        return -1;
    }
    return ret;
}

uintptr_t remote_mmap(int pid, uintptr_t addr, size_t size, int prot, int flags, int fd, off_t offset) {
    uintptr_t result;
    LOGD("remote mmap %" PRIxPTR " size %zu fd %d off %lu", addr, size, fd, offset);
    if (!remote_syscall(pid, result, SYS_mmap, addr, size, prot, flags, fd, offset)) {
        LOGE("do remote mmap failed");
        return -1;
    }
    LOGD("remote mmap get %" PRIxPTR, result);
    return result;
}

bool remote_munmap(int pid, uintptr_t addr, size_t size) {
    uintptr_t result = 0;
    if (remote_syscall(pid, result, SYS_munmap, addr, size)) {
        LOGE("do remote munmap failed");
        return false;
    }
    return true;
}

int remote_open(int pid, uintptr_t path_addr, int flags) {
    uintptr_t result;
    if (!remote_syscall(pid, result, SYS_openat, AT_FDCWD, path_addr, flags)) {
        LOGE("remote open failed");
        return -1;
    }
    return result;
}

bool remote_close(int pid, int fd) {
    uintptr_t result = 0;
    if (remote_syscall(pid, result, SYS_close, fd)) {
        LOGE("remote close failed");
        return false;
    }
    return true;
}

int wait_for_child(int pid) {
    int status;
    for (;;) {
        if (waitpid(pid, &status, 0) == -1) {
            if (errno == EINTR) continue;
            PLOGE("waitpid %d", pid);
            return -1;
        } else {
            return status;
        }
    }
}

std::string generateMagic(size_t len) {
    constexpr const char chrs[] = "0123456789"
                                  "abcdefghijklmnopqrstuvwxyz"
                                  "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    std::mt19937 rg{std::random_device{}()};
    std::uniform_int_distribution<std::string::size_type> pick(0, sizeof(chrs) - 2);

    std::string s;
    s.reserve(len);
    while(len--) s += chrs[pick(rg)];

    return s;
}

int setfilecon(const char* path, const char* con) {
    return syscall(__NR_setxattr, path, XATTR_NAME_SELINUX, con, strlen(con) + 1, 0);
}

bool set_sockcreate_con(const char* con) {
    auto sz = static_cast<ssize_t>(strlen(con) + 1);
    UniqueFd fd = open("/proc/thread-self/attr/sockcreate", O_WRONLY | O_CLOEXEC);
    if (fd == -1 || write(fd, con, sz) != sz) {
        PLOGE("set socket con");
        char buf[128];
        snprintf(buf, sizeof(buf), "/proc/%d/attr/sockcreate", gettid());
        fd = open(buf, O_WRONLY | O_CLOEXEC);
        if (fd == -1 || write(fd, con, sz) != sz) {
            PLOGE("set socket con fallback");
            return false;
        }
    }
    return true;
}
