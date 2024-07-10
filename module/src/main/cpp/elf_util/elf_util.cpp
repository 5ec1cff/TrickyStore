/*
 * This file is part of LSPosed.
 *
 * LSPosed is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * LSPosed is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with LSPosed.  If not, see <https://www.gnu.org/licenses/>.
 *
 * Copyright (C) 2019 Swift Gan
 * Copyright (C) 2021 LSPosed Contributors
 */
#include <malloc.h>
#include <cstring>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <cassert>
#include <sys/stat.h>
#include "logging.hpp"
#include "elf_util.h"
#include <vector>
#include "lsplt.hpp"

using namespace SandHook;

template<typename T>
inline constexpr auto offsetOf(ElfW(Ehdr) *head, ElfW(Off) off) {
    return reinterpret_cast<std::conditional_t<std::is_pointer_v<T>, T, T *>>(
            reinterpret_cast<uintptr_t>(head) + off);
}

ElfImg::ElfImg(std::string_view base_name, void *base, off_t file_offset, size_t size) : elf(
        base_name), base(base), size(size) {
    // load elf
    int fd = open(elf.data(), O_RDONLY);
    if (fd < 0) {
        LOGE("failed to open %s", elf.data());
        return;
    }

    header = reinterpret_cast<decltype(header)>(mmap(nullptr, size, PROT_READ, MAP_SHARED, fd,
                                                     file_offset));

    close(fd);

    section_header = offsetOf<decltype(section_header)>(header, header->e_shoff);

    auto shoff = reinterpret_cast<uintptr_t>(section_header);
    char *section_str = offsetOf<char *>(header, section_header[header->e_shstrndx].sh_offset);

    for (int i = 0; i < header->e_shnum; i++, shoff += header->e_shentsize) {
        auto *section_h = (ElfW(Shdr) *) shoff;
        char *sname = section_h->sh_name + section_str;
        auto entsize = section_h->sh_entsize;
        switch (section_h->sh_type) {
            case SHT_DYNSYM: {
                if (bias == -4396) {
                    dynsym = section_h;
                    dynsym_offset = section_h->sh_offset;
                    dynsym_start = offsetOf<decltype(dynsym_start)>(header, dynsym_offset);
                    dynsym_size = section_h->sh_size;
                    dynsym_count = dynsym_size / entsize;
                }
                break;
            }
            case SHT_SYMTAB: {
                if (strcmp(sname, ".symtab") == 0) {
                    symtab = section_h;
                    symtab_offset = section_h->sh_offset;
                    symtab_size = section_h->sh_size;
                    symtab_count = symtab_size / entsize;
                    symtab_start = offsetOf<decltype(symtab_start)>(header, symtab_offset);
                }
                break;
            }
            case SHT_STRTAB: {
                if (bias == -4396) {
                    strtab = section_h;
                    symstr_offset = section_h->sh_offset;
                    strtab_start = offsetOf<decltype(strtab_start)>(header, symstr_offset);
                }
                if (strcmp(sname, ".strtab") == 0) {
                    symstr_offset_for_symtab = section_h->sh_offset;
                } else if (strcmp(sname, ".dynstr") == 0) {
                    symstr_offset_for_dynsym = section_h->sh_offset;
                }
                break;
            }
            case SHT_PROGBITS: {
                if (strtab == nullptr || dynsym == nullptr) break;
                if (bias == -4396) {
                    bias = (off_t) section_h->sh_addr - (off_t) section_h->sh_offset;
                }
                break;
            }
            case SHT_HASH: {
                auto *d_un = offsetOf<ElfW(Word)>(header, section_h->sh_offset);
                nbucket_ = d_un[0];
                bucket_ = d_un + 2;
                chain_ = bucket_ + nbucket_;
                break;
            }
            case SHT_GNU_HASH: {
                auto *d_buf = reinterpret_cast<ElfW(Word) *>(((size_t) header) +
                                                             section_h->sh_offset);
                gnu_nbucket_ = d_buf[0];
                gnu_symndx_ = d_buf[1];
                gnu_bloom_size_ = d_buf[2];
                gnu_shift2_ = d_buf[3];
                gnu_bloom_filter_ = reinterpret_cast<decltype(gnu_bloom_filter_)>(d_buf + 4);
                gnu_bucket_ = reinterpret_cast<decltype(gnu_bucket_)>(gnu_bloom_filter_ +
                                                                      gnu_bloom_size_);
                gnu_chain_ = gnu_bucket_ + gnu_nbucket_ - gnu_symndx_;
                break;
            }
        }
    }
}

ElfW(Sym) *ElfImg::ElfLookup(std::string_view name, uint32_t hash) const {
    if (nbucket_ == 0) return 0;

    char *strings = (char *) strtab_start;

    for (auto n = bucket_[hash % nbucket_]; n != 0; n = chain_[n]) {
        auto *sym = dynsym_start + n;
        if (name == strings + sym->st_name) {
            return sym;
        }
    }
    return nullptr;
}

ElfW(Sym) *ElfImg::GnuLookup(std::string_view name, uint32_t hash) const {
    static constexpr auto bloom_mask_bits = sizeof(ElfW(Addr)) * 8;

    if (gnu_nbucket_ == 0 || gnu_bloom_size_ == 0) return 0;

    auto bloom_word = gnu_bloom_filter_[(hash / bloom_mask_bits) % gnu_bloom_size_];
    uintptr_t mask = 0
                     | (uintptr_t) 1 << (hash % bloom_mask_bits)
                     | (uintptr_t) 1 << ((hash >> gnu_shift2_) % bloom_mask_bits);
    if ((mask & bloom_word) == mask) {
        auto sym_index = gnu_bucket_[hash % gnu_nbucket_];
        if (sym_index >= gnu_symndx_) {
            char *strings = (char *) strtab_start;
            do {
                auto *sym = dynsym_start + sym_index;
                if (((gnu_chain_[sym_index] ^ hash) >> 1) == 0
                    && name == strings + sym->st_name) {
                    return sym;
                }
            } while ((gnu_chain_[sym_index++] & 1) == 0);
        }
    }
    return nullptr;
}

void ElfImg::MayInitLinearMap() const {
    if (symtabs_.empty()) {
        if (symtab_start != nullptr && symstr_offset_for_symtab != 0) {
            for (ElfW(Off) i = 0; i < symtab_count; i++) {
                unsigned int st_type = ELF_ST_TYPE(symtab_start[i].st_info);
                const char *st_name = offsetOf<const char *>(header, symstr_offset_for_symtab +
                                                                     symtab_start[i].st_name);
                if ((st_type == STT_FUNC || st_type == STT_OBJECT) && symtab_start[i].st_size) {
                    symtabs_.emplace(st_name, &symtab_start[i]);
                }
            }
        }
    }
}

const std::string ElfImg::findSymbolNameForAddr(ElfW(Addr) addr) const {
    if (symtab_start != nullptr && symstr_offset_for_symtab != 0) {
        auto addr_off = (ElfW(Addr)) (addr + bias - (uintptr_t) base);
        for (ElfW(Off) i = 0; i < symtab_count; i++) {
            unsigned int st_type = ELF_ST_TYPE(symtab_start[i].st_info);
            const char *st_name = offsetOf<const char *>(header, symstr_offset_for_symtab +
                                                                 symtab_start[i].st_name);
            if ((st_type == STT_FUNC || st_type == STT_OBJECT) && symtab_start[i].st_size) {
                auto off = symtab_start[i].st_value;
                auto len = symtab_start[i].st_size;
                if (off <= addr_off && addr_off < off + len) {
                    LOGD("found in symtab sym %p name %s", off,
                         st_name);
                    char buf[1024];
                    snprintf(buf, sizeof(buf), "%s (0x%lx)+0x%lx/(0x%lx) from symtab %d", st_name,
                             off, addr_off - off, len, i);
                    return buf;
                }
            }
        }
    }
    if (dynsym_start != nullptr && symstr_offset_for_dynsym != 0) {
        auto addr_off = (ElfW(Addr)) (addr + bias - (uintptr_t) base);
        for (ElfW(Off) i = 0; i < dynsym_count; i++) {
            unsigned int st_type = ELF_ST_TYPE(dynsym_start[i].st_info);
            const char *st_name = offsetOf<const char *>(header, symstr_offset_for_dynsym +
                                                                 dynsym_start[i].st_name);
            if ((st_type == STT_FUNC || st_type == STT_OBJECT) && dynsym_start[i].st_size) {
                auto off = dynsym_start[i].st_value;
                auto len = dynsym_start[i].st_size;
                if (off <= addr_off && addr_off < off + len) {
                    LOGD("found in dynsym sym %p name %s", off,
                         st_name);
                    char buf[1024];
                    snprintf(buf, sizeof(buf), "%s (0x%lx)+0x%lx/(0x%lx) from dynsym %d", st_name,
                             off, addr_off - off, len, i);
                    return buf;
                }
            }
        }
    }
    return "(not found)";
}


ElfW(Sym) *ElfImg::LinearLookup(std::string_view name) const {
    MayInitLinearMap();
    if (auto i = symtabs_.find(name); i != symtabs_.end()) {
        return i->second;
    } else {
        return 0;
    }
}

ElfW(Sym) *ElfImg::PrefixLookupFirst(std::string_view prefix) const {
    MayInitLinearMap();
    if (auto i = symtabs_.lower_bound(prefix); i != symtabs_.end() &&
                                               i->first.starts_with(prefix)) {
        LOGD("found prefix %s of %s %p in %s in symtab by linear lookup", prefix.data(),
             i->first.data(), reinterpret_cast<void *>(i->second->st_value), elf.data());
        return i->second;
    } else {
        return 0;
    }
}

ElfImg::~ElfImg() {
    //open elf file local
    if (buffer) {
        free(buffer);
        buffer = nullptr;
    }
    //use mmap
    if (header) {
        if (munmap(header, size) == -1) PLOGE("munmap");
        LOGI("unmapped %p %ld", header, size);
    }
}

ElfW(Sym) *
ElfImg::getSym(std::string_view name, uint32_t gnu_hash, uint32_t elf_hash) const {
    if (auto sym = GnuLookup(name, gnu_hash); sym) {
        LOGD("found %s %p/%llu in %s in dynsym by gnuhash", name.data(),
             reinterpret_cast<void *>(sym->st_value), sym->st_size, elf.data());
        return sym;
    } else if (sym = ElfLookup(name, elf_hash); sym) {
        LOGD("found %s %p/%llu in %s in dynsym by elfhash", name.data(),
             reinterpret_cast<void *>(sym->st_value), sym->st_size, elf.data());
        return sym;
    } else if (sym = LinearLookup(name); sym) {
        LOGD("found %s %p/%llu in %s in symtab by linear lookup", name.data(),
             reinterpret_cast<void *>(sym->st_value), sym->st_size, elf.data());
        return sym;
    } else {
        return nullptr;
    }

}

constexpr inline bool contains(std::string_view a, std::string_view b) {
    return a.find(b) != std::string_view::npos;
}

ElfInfo ElfInfo::getElfInfoForName(const char *name, const char *pid) {
    auto maps = lsplt::MapInfo::Scan(pid);
    std::string path;
    void *base = nullptr;
    size_t file_size = 0;
    for (auto &map: maps) {
        if (map.path.ends_with(name) && map.offset == 0) {
            struct stat st{};
            if (stat(map.path.c_str(), &st) == -1) {
                PLOGE("stat %s", map.path.c_str());
                break;
            }
            path = map.path;
            file_size = st.st_size;
            base = reinterpret_cast<void *>(map.start);
            LOGD("found path %s size %zu", path.c_str(), file_size);
            break;
        }
    }
    return {path, base, 0, file_size};
}
