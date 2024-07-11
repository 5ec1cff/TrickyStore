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
#include <sys/socket.h>
#include <sys/un.h>
#include <android/dlext.h>
#include <fcntl.h>
#include <csignal>
#include <sys/system_properties.h>
#include <string>
#include <cinttypes>

#include "lsplt.hpp"

#include "logging.hpp"
#include "utils.hpp"

using namespace std::string_literals;

// zygote inject

bool inject_library(int pid, const char *lib_path, const char* entry_name) {
    LOGI("injecting %s and calling %s in %d", lib_path, entry_name, pid);
    struct user_regs_struct regs{}, backup{};
    std::vector<lsplt::MapInfo> map;

    if (ptrace(PTRACE_ATTACH, pid, 0, 0) == -1) {
        PLOGE("attach");
    }
    int status;
    {
        wait_for_trace(pid, &status, __WALL);
    }
    if (WIFSTOPPED(status) && WSTOPSIG(status) == SIGSTOP) {
        if (!get_regs(pid, regs)) return false;
        // The linker has been initialized now, we can do dlopen
        LOGD("stopped at entry");
        // backup registers
        memcpy(&backup, &regs, sizeof(regs));
        {
            map = lsplt::MapInfo::Scan(std::to_string(pid));
        }
        auto local_map = lsplt::MapInfo::Scan();
        auto libc_return_addr = find_module_return_addr(map, "libc.so");
        LOGD("libc return addr %p", libc_return_addr);

        std::vector<uintptr_t> args;
        uintptr_t str, remote_handle, injector_entry;
        auto close_addr = find_func_addr(local_map, map, "libc.so", "close");
        if (!close_addr) return false;

        int lib_fd = -1;

        // prepare fd
        {
            set_sockcreate_con("u:object_r:system_file:s0");
            UniqueFd local_socket = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
            if (local_socket == -1) {
                PLOGE("create local socket");
                return false;
            }
            if (setfilecon(lib_path, "u:object_r:system_file:s0") == -1) {
                PLOGE("set context of lib");
            }
            UniqueFd local_lib_fd = open(lib_path, O_RDONLY | O_CLOEXEC);
            if (local_lib_fd == -1) {
                PLOGE("open lib");
                return false;
            }
            auto socket_addr = find_func_addr(local_map, map, "libc.so", "socket");
            auto bind_addr = find_func_addr(local_map, map, "libc.so", "bind");
            auto recvmsg_addr = find_func_addr(local_map, map, "libc.so", "recvmsg");
            auto errno_addr = find_func_addr(local_map, map, "libc.so", "__errno");
            auto get_remote_errno = [&]() -> int {
                if (!errno_addr) {
                    LOGE("could not get errno!");
                    return 0;
                }
                auto addr = remote_call(pid, regs, (uintptr_t) errno_addr, 0, args);
                int err = 0;
                if (!read_proc(pid, addr, &err, sizeof(err))) return 0;
                return err;
            };
            if (!socket_addr || !bind_addr || !recvmsg_addr) return false;
            args.clear();
            args.push_back(AF_UNIX);
            args.push_back(SOCK_DGRAM | SOCK_CLOEXEC);
            args.push_back(0);
            int remote_fd = (int) remote_call(pid, regs, (uintptr_t) socket_addr, 0, args);
            if (remote_fd == -1) {
                errno = get_remote_errno();
                PLOGE("remote socket");
                return false;
            }

            auto close_remote = [&](int fd) -> void {
                args.clear();
                args.push_back(fd);
                if (remote_call(pid, regs, (uintptr_t) close_addr, 0, args) != 0) {
                    LOGE("remote not closed: %d", fd);
                }
            };

            auto magic = generateMagic(16);
            struct sockaddr_un addr{
                    .sun_family = AF_UNIX,
                    .sun_path = {0}
            };
            LOGD("socket name %s", magic.c_str());
            memcpy(addr.sun_path + 1, magic.c_str(), magic.size());
            socklen_t len = sizeof(addr.sun_family) + 1 + magic.size();
            auto remote_addr = push_memory(pid, regs, &addr, sizeof(addr));
            args.clear();
            args.push_back(remote_fd);
            args.push_back(remote_addr);
            args.push_back(len);
            auto bind_result = remote_call(pid, regs, (uintptr_t) bind_addr, 0, args);
            if (bind_result == (uintptr_t) -1) {
                errno = get_remote_errno();
                PLOGE("remote bind");
                close_remote(remote_fd);
                return false;
            }

            char cmsgbuf[CMSG_SPACE(sizeof(int))] = {0};
            auto remote_cmsgbuf = push_memory(pid, regs, &cmsgbuf, sizeof(cmsgbuf));

            struct msghdr hdr{};
            hdr.msg_control = (void*) remote_cmsgbuf;
            hdr.msg_controllen = sizeof(cmsgbuf);

            auto remote_hdr = push_memory(pid, regs, &hdr, sizeof(hdr));

            args.clear();
            args.push_back(remote_fd);
            args.push_back(remote_hdr);
            args.push_back(MSG_WAITALL);
            if (!remote_pre_call(pid, regs, (uintptr_t) recvmsg_addr, 0, args)) {
                // we can't do anything more
                LOGE("pre call remote recvmsg");
                return false;
            }

            hdr.msg_control = &cmsgbuf;
            hdr.msg_name = &addr;
            hdr.msg_namelen = len;
            {
                cmsghdr *cmsg = CMSG_FIRSTHDR(&hdr);
                cmsg->cmsg_len = CMSG_LEN(sizeof(int));
                cmsg->cmsg_level = SOL_SOCKET;
                cmsg->cmsg_type = AF_UNIX;
                *(int *) CMSG_DATA(cmsg) = local_lib_fd;
            }

            if (sendmsg(local_socket, &hdr, 0) == -1) {
                PLOGE("send to remote");
                close_remote(remote_fd);
                return false;
            }

            auto recvmsg_result = (ssize_t) remote_post_call(pid, regs, 0);
            if (recvmsg_result == -1) {
                errno = get_remote_errno();
                PLOGE("post call recvmsg");
                close_remote(remote_fd);
                return false;
            }

            if (read_proc(pid, remote_cmsgbuf, &cmsgbuf, sizeof(cmsgbuf)) != sizeof(cmsgbuf)) {
                LOGE("failed to read proc");
                close_remote(remote_fd);
                return false;
            }

            cmsghdr *cmsg = CMSG_FIRSTHDR(&hdr);
            if (cmsg == nullptr || cmsg->cmsg_len != CMSG_LEN(sizeof(int)) || cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_RIGHTS) {
                LOGE("remote recv fd failed!");
                close_remote(remote_fd);
                return false;
            }

            lib_fd = *(int*) CMSG_DATA(cmsg);
            LOGD("remote lib fd: %d", lib_fd);
            close_remote(remote_fd);
        }

        // call dlopen
        {
                        auto dlopen_addr = find_func_addr(local_map, map, "libdl.so", "android_dlopen_ext");
            if (dlopen_addr == nullptr) return false;
            android_dlextinfo info{};
            info.flags = ANDROID_DLEXT_USE_LIBRARY_FD;
            info.library_fd = lib_fd;
            uintptr_t remote_info = push_memory(pid, regs, &info, sizeof(info));
            str = push_string(pid, regs, lib_path);
            args.clear();
            args.push_back((long) str);
            args.push_back((long) RTLD_NOW);
            args.push_back(remote_info);
            remote_handle = remote_call(pid, regs, (uintptr_t) dlopen_addr,
                                        (uintptr_t) libc_return_addr, args);
            LOGD("remote handle %p", (void *) remote_handle);
            if (remote_handle == 0) {
                LOGE("handle is null");
                // call dlerror
                auto dlerror_addr = find_func_addr(local_map, map, "libdl.so", "dlerror");
                if (dlerror_addr == nullptr) {
                    LOGE("find dlerror");
                    return false;
                }
                args.clear();
                auto dlerror_str_addr = remote_call(pid, regs, (uintptr_t) dlerror_addr,
                                                    (uintptr_t) libc_return_addr, args);
                LOGD("dlerror str %p", (void *) dlerror_str_addr);
                if (dlerror_str_addr == 0) return false;
                auto strlen_addr = find_func_addr(local_map, map, "libc.so", "strlen");
                if (strlen_addr == nullptr) {
                    LOGE("find strlen");
                    return false;
                }
                args.clear();
                args.push_back(dlerror_str_addr);
                auto dlerror_len = remote_call(pid, regs, (uintptr_t) strlen_addr,
                                               (uintptr_t) libc_return_addr, args);
                if (dlerror_len <= 0) {
                    LOGE("dlerror len <= 0");
                    return false;
                }
                std::string err;
                err.resize(dlerror_len + 1, 0);
                read_proc(pid, (uintptr_t) dlerror_str_addr, err.data(), dlerror_len);
                LOGE("dlerror info %s", err.c_str());
                return false;
            }

            args.clear();
            args.push_back(lib_fd);
            if (remote_call(pid, regs, (uintptr_t) close_addr, 0, args) != 0) {
                LOGE("remote lib not closed: %d", lib_fd);
                return false;
            }
        }

        // call dlsym(handle, "entry")
        {
                        auto dlsym_addr = find_func_addr(local_map, map, "libdl.so", "dlsym");
            if (dlsym_addr == nullptr) return false;
            args.clear();
            str = push_string(pid, regs, "entry");
            args.push_back(remote_handle);
            args.push_back((long) str);
            injector_entry = remote_call(pid, regs, (uintptr_t) dlsym_addr,
                                         (uintptr_t) libc_return_addr, args);
            LOGD("injector entry %p", (void *) injector_entry);
            if (injector_entry == 0) {
                LOGE("injector entry is null");
                return false;
            }
        }

        // call injector entry(handle)
        {
            args.clear();
            args.push_back(remote_handle);
            remote_call(pid, regs, injector_entry, (uintptr_t) libc_return_addr, args);
        }

        LOGD("restore context");
        // restore registers
        if (!set_regs(pid, backup)) return false;
        if (ptrace(PTRACE_DETACH, pid, 0, 0) == -1) {
            PLOGE("failed to detach");
            return false;
        }
        return true;
    } else {
        LOGE("stopped by other reason: %s", parse_status(status).c_str());
    }
    return false;
}

int main(int argc, char **argv) {
    logging::setPrintEnabled(true);
    auto pid = strtol(argv[1], nullptr, 0);
    char buf[4096];
    realpath(argv[2], buf);
    return !inject_library(pid, buf, argv[3]);
}
