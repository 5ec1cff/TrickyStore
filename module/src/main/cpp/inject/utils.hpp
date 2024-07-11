#pragma once
#include <string>
#include <sys/ptrace.h>
#include <map>
#include "lsplt.hpp"

#define LOG_TAG "TrickyStore"

#define SYSCALL_IS_ERR(e) (((unsigned long) e) > -4096UL)
#define SYSCALL_ERR(e) (-(int)(e))

#if defined(__x86_64__)
#define REG_SP rsp
#define REG_IP rip
#define REG_RET rax
#define REG_NR orig_rax
#define REG_SYS_ARG0 rdi
#elif defined(__i386__)
#define REG_SP esp
#define REG_IP eip
#define REG_RET eax
#define REG_NR orig_eax
#define REG_SYS_ARG0 ebx
#elif defined(__aarch64__)
#define REG_SP sp
#define REG_IP pc
#define REG_RET regs[0]
#define REG_NR regs[8]
#define REG_SYS_ARG0 regs[0]
#elif defined(__arm__)
#define REG_SP uregs[13]
#define REG_IP uregs[15]
#define REG_RET uregs[0]
#define REG_NR uregs[7]
#define REG_SYS_ARG0 uregs[0]
#define user_regs_struct user_regs
#define SYS_mmap SYS_mmap2
#endif

ssize_t write_proc(int pid, uintptr_t remote_addr, const void *buf, size_t len, bool use_proc_mem = false);

ssize_t read_proc(int pid, uintptr_t remote_addr, void *buf, size_t len);

bool get_regs(int pid, struct user_regs_struct &regs);

bool set_regs(int pid, struct user_regs_struct &regs);

std::string get_addr_mem_region(std::vector<lsplt::MapInfo> &info, uintptr_t addr);

void *find_module_base(std::vector<lsplt::MapInfo> &info, std::string_view suffix);

void *find_func_addr(
        std::vector<lsplt::MapInfo> &local_info,
        std::vector<lsplt::MapInfo> &remote_info,
        std::string_view module,
        std::string_view func);

void align_stack(struct user_regs_struct &regs, uintptr_t preserve = 0);

uintptr_t push_memory(int pid, struct user_regs_struct &regs, void* local_addr, size_t len);

uintptr_t push_string(int pid, struct user_regs_struct &regs, const char *str);

uintptr_t remote_call(int pid, struct user_regs_struct &regs, uintptr_t func_addr, uintptr_t return_addr,
                      std::vector<uintptr_t> &args);

int fork_dont_care();

bool wait_for_trace(int pid, int* status, int flags);

std::string parse_status(int status);

#define WPTEVENT(x) (x >> 16)

#define CASE_CONST_RETURN(x) case x: return #x;

inline const char* parse_ptrace_event(int status) {
    status = status >> 16;
    switch (status) {
        CASE_CONST_RETURN(PTRACE_EVENT_FORK)
        CASE_CONST_RETURN(PTRACE_EVENT_VFORK)
        CASE_CONST_RETURN(PTRACE_EVENT_CLONE)
        CASE_CONST_RETURN(PTRACE_EVENT_EXEC)
        CASE_CONST_RETURN(PTRACE_EVENT_VFORK_DONE)
        CASE_CONST_RETURN(PTRACE_EVENT_EXIT)
        CASE_CONST_RETURN(PTRACE_EVENT_SECCOMP)
        CASE_CONST_RETURN(PTRACE_EVENT_STOP)
        default:
            return "(no event)";
    }
}

inline const char* sigabbrev_np(int sig) {
    if (sig > 0 && sig < NSIG) return sys_signame[sig];
    return "(unknown)";
}

std::string get_program(int pid);
void *find_module_return_addr(std::vector<lsplt::MapInfo> &info, std::string_view suffix);

// pid = 0, fd != nullptr -> set to fd
// pid != 0, fd != nullptr -> set to pid ns, give orig ns in fd
bool switch_mnt_ns(int pid, int *fd);

std::vector<std::string> get_cmdline(int pid);

std::string parse_exec(int pid);

bool skip_syscall(int pid);
bool do_syscall(int pid, uintptr_t &ret, int nr, uintptr_t arg0 = 0, uintptr_t arg1 = 0, uintptr_t arg2 = 0, uintptr_t arg3 = 0, uintptr_t arg4 = 0, uintptr_t arg5 = 0);

uintptr_t remote_mmap(int pid, uintptr_t addr, size_t size, int prot, int flags, int fd, off_t offset);
bool remote_munmap(int pid, uintptr_t addr, size_t size);

int remote_open(int pid, uintptr_t path_addr, int flags);
bool remote_close(int pid, int fd);

int wait_for_child(int pid);
int get_elf_class(std::string_view path);

bool remote_pre_call(int pid, struct user_regs_struct &regs, uintptr_t func_addr, uintptr_t return_addr,
                     std::vector<uintptr_t> &args);

uintptr_t remote_post_call(int pid, struct user_regs_struct &regs, uintptr_t return_addr);


// magic.h
constexpr const auto kMainMagicLength = 16;
std::string generateMagic(size_t len);

// files.hpp

int setfilecon(const char* path, const char* con);

// utils.hpp
class UniqueFd {
    using Fd = int;
public:
    UniqueFd() = default;

    UniqueFd(Fd fd) : fd_(fd) {}

    ~UniqueFd() { if (fd_ >= 0) close(fd_); }

    // Disallow copy
    UniqueFd(const UniqueFd&) = delete;

    UniqueFd& operator=(const UniqueFd&) = delete;

    // Allow move
    UniqueFd(UniqueFd&& other) { std::swap(fd_, other.fd_); }

    UniqueFd& operator=(UniqueFd&& other) {
        std::swap(fd_, other.fd_);
        return *this;
    }

    // Implict cast to Fd
    operator const Fd&() const { return fd_; }

private:
    Fd fd_ = -1;
};

// socket_utils.hpp
bool set_sockcreate_con(const char* con);
