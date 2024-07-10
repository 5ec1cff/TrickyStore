#include "lsplt.hpp"

#include <sys/mman.h>
#include <sys/sysmacros.h>
#include <array>
#include <cinttypes>
#include <list>
#include <map>
#include <mutex>
#include <vector>

namespace lsplt {
    inline namespace v2 {
        [[maybe_unused]] std::vector<MapInfo>
        MapInfo::Scan(std::string proc, std::function<bool(const MapInfo &)> filter) {
            constexpr static auto kPermLength = 5;
            constexpr static auto kMapEntry = 7;
            std::vector<MapInfo> info;
            auto name = std::string("/proc/") + proc + "/maps";
            auto maps = std::unique_ptr<FILE, decltype(&fclose)>{fopen(name.c_str(), "r"),
                                                                 &fclose};
            if (maps) {
                char *line = nullptr;
                size_t len = 0;
                ssize_t read;
                while ((read = getline(&line, &len, maps.get())) > 0) {
                    line[read - 1] = '\0';
                    uintptr_t start = 0;
                    uintptr_t end = 0;
                    uintptr_t off = 0;
                    ino_t inode = 0;
                    unsigned int dev_major = 0;
                    unsigned int dev_minor = 0;
                    std::array<char, kPermLength> perm{'\0'};
                    int path_off;
                    if (sscanf(line, "%" PRIxPTR"-%"
                                     PRIxPTR
                                     " %4s %"
                                     PRIxPTR
                                     " %x:%x %lu %n%*s", &start,
                               &end, perm.data(), &off, &dev_major, &dev_minor, &inode,
                               &path_off) == kMapEntry) {
                        while (path_off < read && isspace(line[path_off])) path_off++;
                        MapInfo sm{};
                        sm.start = start;
                        sm.end = end;
                        sm.perms = 0;
                        sm.is_private = perm[3] == 'p';
                        sm.offset = off;
                        sm.dev = static_cast<dev_t>(makedev(dev_major, dev_minor));
                        sm.inode = inode;
                        sm.path = line + path_off;
                        if (perm[0] == 'r') sm.perms |= PROT_READ;
                        if (perm[1] == 'w') sm.perms |= PROT_WRITE;
                        if (perm[2] == 'x') sm.perms |= PROT_EXEC;
                        if (filter(sm)) info.emplace_back(sm);
                        continue;
                    }
                }
                free(line);
            }
            return info;
        }

        // https://cs.android.com/android/kernel/superproject/+/common-android-mainline:common/fs/proc/task_mmu.c;l=827;drc=08582d678fcf11fc86188f0b92239d3d49667d8e
        [[maybe_unused]] std::vector<SMapInfo>
        SMapInfo::Scan(std::string proc, std::function<bool(const MapInfo &)> filter) {
            constexpr static auto kPermLength = 5;
            constexpr static auto kMapEntry = 7;
            std::vector<SMapInfo> info;
            auto name = std::string("/proc/") + proc + "/smaps";
            auto maps = std::unique_ptr<FILE, decltype(&fclose)>{fopen(name.c_str(), "r"),
                                                                 &fclose};
            if (!maps) {
                return info;
            }
            char *line = nullptr;
            size_t len = 0;
            ssize_t read;
            while ((read = getline(&line, &len, maps.get())) > 0) {
                line[read - 1] = '\0';
                uintptr_t start = 0;
                uintptr_t end = 0;
                uintptr_t off = 0;
                ino_t inode = 0;
                unsigned int dev_major = 0;
                unsigned int dev_minor = 0;
                std::array<char, kPermLength> perm{'\0'};
                int path_off;
                if (sscanf(line, "%" PRIxPTR"-%"
                                 PRIxPTR
                                 " %4s %"
                                 PRIxPTR
                                 " %x:%x %lu %n%*s", &start,
                           &end, perm.data(), &off, &dev_major, &dev_minor, &inode,
                           &path_off) == kMapEntry) {
                    while (path_off < read && isspace(line[path_off])) path_off++;
                    SMapInfo sm{};
                    sm.start = start;
                    sm.end = end;
                    sm.perms = 0;
                    sm.is_private = perm[3] == 'p';
                    sm.offset = off;
                    sm.dev = static_cast<dev_t>(makedev(dev_major, dev_minor));
                    sm.inode = inode;
                    sm.path = line + path_off;
                    if (perm[0] == 'r') sm.perms |= PROT_READ;
                    if (perm[1] == 'w') sm.perms |= PROT_WRITE;
                    if (perm[2] == 'x') sm.perms |= PROT_EXEC;
                    if (filter(sm)) info.emplace_back(sm);
                    continue;
                }
                if (info.empty()) continue;
                auto &current = *info.rbegin();
                ssize_t value;
                auto s = strstr(line, ":");
                if (s == nullptr) continue;
                *s = 0;
                if (sscanf(s + 1, "%zu", &value) != 1) continue;
                std::string_view ss{line};
                if (ss == "Size") current.size = value;
                else if (ss == "Rss") current.rss = value;
                else if (ss == "Pss") current.pss = value;
                else if (ss == "Shared_Clean") current.shared_clean = value;
                else if (ss == "Shared_Dirty") current.shared_dirty = value;
                else if (ss == "Private_Clean") current.private_clean = value;
                else if (ss == "Private_Dirty") current.private_dirty = value;
                else if (ss == "Referenced") current.referenced = value;
                else if (ss == "Anonymous") current.anonymous = value;
            }
            free(line);
            return info;
        }

        std::string MapInfo::display() const {
            char buf[sizeof(long) * 2 + 1];
            std::string result;
            snprintf(buf, sizeof(buf), "%" PRIxPTR, start);
            result += buf;
            result += "-";
            snprintf(buf, sizeof(buf), "%" PRIxPTR, end);
            result += buf;
            result += ' ';
            result += (perms & PROT_READ) ? 'r' : '-';
            result += (perms & PROT_WRITE) ? 'w' : '-';
            result += (perms & PROT_EXEC) ? 'x' : '-';
            result += is_private ? 'p' : 's';
            result += ' ';
            snprintf(buf, sizeof(buf), "%08" PRIxPTR, offset);
            result += buf;
            result += ' ';
            snprintf(buf, sizeof(buf), "%02" PRIxPTR, major(dev));
            result += buf;
            result += ':';
            snprintf(buf, sizeof(buf), "%02" PRIxPTR, minor(dev));
            result += buf;
            result += ' ';
            snprintf(buf, sizeof(buf), "%lu", inode);
            result += buf;
            result += ' ';
            result += path;
            return result;
        }

        std::string SMapInfo::display() const {
            auto result = MapInfo::display();
            char buf[sizeof(long) * 2 + 1];
            result += '\n';
            result += "SZ:";
            snprintf(buf, sizeof(buf), "%zu", size);
            result += buf;
            result += " RSS:";
            snprintf(buf, sizeof(buf), "%zu", rss);
            result += buf;
            result += " PSS:";
            snprintf(buf, sizeof(buf), "%zu", pss);
            result += buf;
            result += " SC:";
            snprintf(buf, sizeof(buf), "%zu", shared_clean);
            result += buf;
            result += " SD:";
            snprintf(buf, sizeof(buf), "%zu", shared_dirty);
            result += buf;
            result += " PC:";
            snprintf(buf, sizeof(buf), "%zu", private_clean);
            result += buf;
            result += " PD:";
            snprintf(buf, sizeof(buf), "%zu", private_dirty);
            result += buf;
            result += " REF:";
            snprintf(buf, sizeof(buf), "%zu", referenced);
            result += buf;
            result += " ANON:";
            snprintf(buf, sizeof(buf), "%zu", anonymous);
            result += buf;
            return result;
        }
    }
}
