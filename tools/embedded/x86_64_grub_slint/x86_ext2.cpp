#include "x86_ext2.h"

#include <radixkernel/radixkernel.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

extern "C" void x86_serial_write(const char *text);
extern "C" int snprintf(char *buffer, size_t size, const char *format, ...);

namespace {
constexpr uint16_t Ext2Magic = 0xef53u;
constexpr uint32_t Ext2RootInode = 2u;
constexpr uint16_t Ext2SIfdir = 0x4000u;
constexpr uint16_t Ext2SIfreg = 0x8000u;
constexpr uint32_t Ext2NameMax = 255u;
constexpr uint32_t MaxBlockSize = 4096u;
constexpr uint32_t MaxOpenFiles = 8u;

struct [[gnu::packed]] Ext2Superblock {
    uint32_t inodes_count;
    uint32_t blocks_count;
    uint32_t reserved_blocks_count;
    uint32_t free_blocks_count;
    uint32_t free_inodes_count;
    uint32_t first_data_block;
    uint32_t log_block_size;
    uint32_t log_frag_size;
    uint32_t blocks_per_group;
    uint32_t frags_per_group;
    uint32_t inodes_per_group;
    uint32_t mtime;
    uint32_t wtime;
    uint16_t mount_count;
    uint16_t max_mount_count;
    uint16_t magic;
    uint16_t state;
    uint16_t errors;
    uint16_t minor_rev_level;
    uint32_t lastcheck;
    uint32_t checkinterval;
    uint32_t creator_os;
    uint32_t rev_level;
    uint16_t def_resuid;
    uint16_t def_resgid;
    uint32_t first_ino;
    uint16_t inode_size;
};

struct [[gnu::packed]] Ext2GroupDesc {
    uint32_t block_bitmap;
    uint32_t inode_bitmap;
    uint32_t inode_table;
    uint16_t free_blocks_count;
    uint16_t free_inodes_count;
    uint16_t used_dirs_count;
    uint16_t pad;
    uint8_t reserved[12];
};

struct [[gnu::packed]] Ext2Inode {
    uint16_t mode;
    uint16_t uid;
    uint32_t size;
    uint32_t atime;
    uint32_t ctime;
    uint32_t mtime;
    uint32_t dtime;
    uint16_t gid;
    uint16_t links_count;
    uint32_t blocks;
    uint32_t flags;
    uint32_t osd1;
    uint32_t block[15];
};

struct [[gnu::packed]] Ext2DirEntry {
    uint32_t inode;
    uint16_t rec_len;
    uint8_t name_len;
    uint8_t file_type;
};

struct Ext2File {
    uint32_t inode_number;
    Ext2Inode inode;
    uint64_t position;
    int used;
};

struct Ext2Context {
    rad_device_t block_device;
    rad_block_info_t block_info;
    uint32_t block_size;
    uint32_t inode_size;
    uint32_t inodes_per_group;
    uint32_t blocks_per_group;
    uint32_t group_desc_offset;
    int mounted;
};

Ext2Context g_ext2{};
Ext2File g_files[MaxOpenFiles];
uint8_t g_sector[512];
uint8_t g_block[MaxBlockSize];
uint8_t g_indirect[MaxBlockSize];

uint32_t min_u32(uint32_t a, uint32_t b) { return a < b ? a : b; }

rad_status_t disk_read(uint64_t offset, void *buffer, size_t size) {
    if (!g_ext2.block_device || !buffer) return RAD_STATUS_INVALID_ARGUMENT;
    auto *out = static_cast<uint8_t*>(buffer);
    size_t done = 0;
    while (done < size) {
        const uint64_t absolute = offset + done;
        const uint64_t sector = absolute / 512u;
        const uint32_t sector_offset = static_cast<uint32_t>(absolute % 512u);
        const uint32_t copy = min_u32(static_cast<uint32_t>(size - done), 512u - sector_offset);
        rad_status_t status = rad_block_read(g_ext2.block_device, sector, 1, g_sector);
        if (status != RAD_STATUS_OK) return status;
        memcpy(out + done, g_sector + sector_offset, copy);
        done += copy;
    }
    return RAD_STATUS_OK;
}

rad_status_t read_block(uint32_t block, void *buffer) {
    return disk_read(static_cast<uint64_t>(block) * g_ext2.block_size, buffer, g_ext2.block_size);
}

rad_status_t read_group_desc(uint32_t group, Ext2GroupDesc *desc) {
    return disk_read(g_ext2.group_desc_offset + static_cast<uint64_t>(group) * sizeof(Ext2GroupDesc), desc, sizeof(*desc));
}

rad_status_t read_inode(uint32_t inode_number, Ext2Inode *inode) {
    if (inode_number == 0 || !inode) return RAD_STATUS_INVALID_ARGUMENT;
    const uint32_t group = (inode_number - 1u) / g_ext2.inodes_per_group;
    const uint32_t index = (inode_number - 1u) % g_ext2.inodes_per_group;
    Ext2GroupDesc desc{};
    rad_status_t status = read_group_desc(group, &desc);
    if (status != RAD_STATUS_OK) return status;
    const uint64_t offset = static_cast<uint64_t>(desc.inode_table) * g_ext2.block_size
        + static_cast<uint64_t>(index) * g_ext2.inode_size;
    return disk_read(offset, inode, sizeof(*inode));
}

uint32_t inode_size(const Ext2Inode& inode) {
    return inode.size;
}

int inode_is_dir(const Ext2Inode& inode) {
    return (inode.mode & 0xf000u) == Ext2SIfdir;
}

int inode_is_reg(const Ext2Inode& inode) {
    return (inode.mode & 0xf000u) == Ext2SIfreg;
}

rad_status_t file_block_number(const Ext2Inode& inode, uint32_t logical_block, uint32_t *physical_block) {
    if (!physical_block) return RAD_STATUS_INVALID_ARGUMENT;
    if (logical_block < 12u) {
        *physical_block = inode.block[logical_block];
        return RAD_STATUS_OK;
    }
    const uint32_t indirect_block = inode.block[12];
    if (!indirect_block) {
        *physical_block = 0;
        return RAD_STATUS_OK;
    }
    const uint32_t index = logical_block - 12u;
    const uint32_t entries = g_ext2.block_size / sizeof(uint32_t);
    if (index >= entries) return RAD_STATUS_NOT_SUPPORTED;
    rad_status_t status = read_block(indirect_block, g_indirect);
    if (status != RAD_STATUS_OK) return status;
    *physical_block = reinterpret_cast<uint32_t*>(g_indirect)[index];
    return RAD_STATUS_OK;
}

rad_status_t read_inode_data(const Ext2Inode& inode, uint64_t offset, void *buffer, size_t size, size_t *bytes_read) {
    if (!buffer) return RAD_STATUS_INVALID_ARGUMENT;
    const uint64_t total_size = inode_size(inode);
    if (offset >= total_size) {
        if (bytes_read) *bytes_read = 0;
        return RAD_STATUS_OK;
    }
    size_t remaining = static_cast<size_t>(total_size - offset);
    if (remaining > size) remaining = size;
    auto *out = static_cast<uint8_t*>(buffer);
    size_t done = 0;
    while (done < remaining) {
        const uint64_t absolute = offset + done;
        const uint32_t logical = static_cast<uint32_t>(absolute / g_ext2.block_size);
        const uint32_t block_offset = static_cast<uint32_t>(absolute % g_ext2.block_size);
        const uint32_t copy = min_u32(static_cast<uint32_t>(remaining - done), g_ext2.block_size - block_offset);
        uint32_t physical = 0;
        rad_status_t status = file_block_number(inode, logical, &physical);
        if (status != RAD_STATUS_OK) return status;
        if (physical == 0) {
            memset(out + done, 0, copy);
        } else {
            status = read_block(physical, g_block);
            if (status != RAD_STATUS_OK) return status;
            memcpy(out + done, g_block + block_offset, copy);
        }
        done += copy;
    }
    if (bytes_read) *bytes_read = done;
    return RAD_STATUS_OK;
}

int name_matches(const char *name, const char *entry_name, uint8_t entry_len) {
    return strlen(name) == entry_len && memcmp(name, entry_name, entry_len) == 0;
}

rad_status_t lookup_child(uint32_t dir_inode_number, const char *name, uint32_t *child_inode) {
    Ext2Inode dir{};
    rad_status_t status = read_inode(dir_inode_number, &dir);
    if (status != RAD_STATUS_OK) return status;
    if (!inode_is_dir(dir)) return RAD_STATUS_INVALID_ARGUMENT;

    const uint32_t total = inode_size(dir);
    uint32_t offset = 0;
    while (offset < total) {
        size_t got = 0;
        status = read_inode_data(dir, offset, g_block, min_u32(g_ext2.block_size, total - offset), &got);
        if (status != RAD_STATUS_OK) return status;
        if (got < sizeof(Ext2DirEntry)) return RAD_STATUS_NOT_FOUND;
        uint32_t pos = 0;
        while (pos + sizeof(Ext2DirEntry) <= got) {
            auto *entry = reinterpret_cast<Ext2DirEntry*>(g_block + pos);
            if (entry->rec_len < sizeof(Ext2DirEntry) || pos + entry->rec_len > got) break;
            const char *entry_name = reinterpret_cast<const char*>(entry + 1);
            if (entry->inode && entry->name_len <= Ext2NameMax && name_matches(name, entry_name, entry->name_len)) {
                if (child_inode) *child_inode = entry->inode;
                return RAD_STATUS_OK;
            }
            pos += entry->rec_len;
        }
        offset += static_cast<uint32_t>(got);
    }
    return RAD_STATUS_NOT_FOUND;
}

rad_status_t lookup_path(const char *path, uint32_t *inode_number, Ext2Inode *inode) {
    uint32_t current = Ext2RootInode;
    if (path && *path) {
        char scratch[128];
        size_t len = strlen(path);
        if (len >= sizeof(scratch)) return RAD_STATUS_INVALID_ARGUMENT;
        memcpy(scratch, path, len + 1u);
        char *cursor = scratch;
        while (*cursor == '/') ++cursor;
        while (*cursor) {
            char *part = cursor;
            while (*cursor && *cursor != '/') ++cursor;
            if (*cursor) *cursor++ = '\0';
            if (strcmp(part, ".") == 0 || *part == '\0') continue;
            if (strcmp(part, "..") == 0) return RAD_STATUS_NOT_SUPPORTED;
            rad_status_t status = lookup_child(current, part, &current);
            if (status != RAD_STATUS_OK) return status;
            while (*cursor == '/') ++cursor;
        }
    }
    Ext2Inode found{};
    rad_status_t status = read_inode(current, &found);
    if (status != RAD_STATUS_OK) return status;
    if (inode_number) *inode_number = current;
    if (inode) *inode = found;
    return RAD_STATUS_OK;
}

Ext2File *allocate_file(void) {
    for (size_t i = 0; i < MaxOpenFiles; ++i) {
        if (g_files[i].used) continue;
        memset(&g_files[i], 0, sizeof(g_files[i]));
        g_files[i].used = 1;
        return &g_files[i];
    }
    return nullptr;
}

rad_status_t ext2_open(void*, const char *path, uint32_t flags, void **file) {
    if (!file || (flags & RAD_VFS_WRITE)) return RAD_STATUS_INVALID_ARGUMENT;
    uint32_t inode_number = 0;
    Ext2Inode inode{};
    rad_status_t status = lookup_path(path, &inode_number, &inode);
    if (status != RAD_STATUS_OK) return status;
    if (!inode_is_reg(inode)) return RAD_STATUS_INVALID_ARGUMENT;
    Ext2File *handle = allocate_file();
    if (!handle) return RAD_STATUS_NO_MEMORY;
    handle->inode_number = inode_number;
    handle->inode = inode;
    *file = handle;
    return RAD_STATUS_OK;
}

rad_status_t ext2_read(void *file, void *buffer, size_t size, size_t *bytes_read) {
    auto *handle = static_cast<Ext2File*>(file);
    if (!handle || !handle->used) return RAD_STATUS_INVALID_ARGUMENT;
    size_t got = 0;
    rad_status_t status = read_inode_data(handle->inode, handle->position, buffer, size, &got);
    if (status == RAD_STATUS_OK) handle->position += got;
    if (bytes_read) *bytes_read = got;
    return status;
}

rad_status_t ext2_write(void*, const void*, size_t, size_t*) {
    return RAD_STATUS_NOT_SUPPORTED;
}

rad_status_t ext2_seek(void *file, int64_t offset, rad_seek_origin_t origin) {
    auto *handle = static_cast<Ext2File*>(file);
    if (!handle || !handle->used) return RAD_STATUS_INVALID_ARGUMENT;
    int64_t base = 0;
    if (origin == RAD_SEEK_CUR) base = static_cast<int64_t>(handle->position);
    if (origin == RAD_SEEK_END) base = static_cast<int64_t>(inode_size(handle->inode));
    const int64_t next = base + offset;
    if (next < 0) return RAD_STATUS_INVALID_ARGUMENT;
    handle->position = static_cast<uint64_t>(next);
    return RAD_STATUS_OK;
}

uint64_t ext2_tell(void *file) {
    auto *handle = static_cast<Ext2File*>(file);
    return handle && handle->used ? handle->position : 0;
}

void ext2_close(void *file) {
    auto *handle = static_cast<Ext2File*>(file);
    if (handle) memset(handle, 0, sizeof(*handle));
}

rad_status_t ext2_stat(void*, const char *path, rad_vfs_stat_t *stat) {
    if (!stat) return RAD_STATUS_INVALID_ARGUMENT;
    Ext2Inode inode{};
    rad_status_t status = lookup_path(path, nullptr, &inode);
    if (status != RAD_STATUS_OK) return status;
    stat->size = inode_size(inode);
    stat->is_directory = inode_is_dir(inode);
    stat->mode = inode.mode;
    stat->mtime_millis = static_cast<uint64_t>(inode.mtime) * 1000u;
    return RAD_STATUS_OK;
}

rad_status_t ext2_list(void*, const char *path, rad_vfs_list_callback_t callback, void *callback_context) {
    if (!callback) return RAD_STATUS_INVALID_ARGUMENT;
    Ext2Inode dir{};
    rad_status_t status = lookup_path(path, nullptr, &dir);
    if (status != RAD_STATUS_OK) return status;
    if (!inode_is_dir(dir)) return RAD_STATUS_INVALID_ARGUMENT;
    const uint32_t total = inode_size(dir);
    uint32_t offset = 0;
    while (offset < total) {
        size_t got = 0;
        status = read_inode_data(dir, offset, g_block, min_u32(g_ext2.block_size, total - offset), &got);
        if (status != RAD_STATUS_OK) return status;
        uint32_t pos = 0;
        while (pos + sizeof(Ext2DirEntry) <= got) {
            auto *entry = reinterpret_cast<Ext2DirEntry*>(g_block + pos);
            if (entry->rec_len < sizeof(Ext2DirEntry) || pos + entry->rec_len > got) break;
            const char *entry_name = reinterpret_cast<const char*>(entry + 1);
            if (entry->inode && !(entry->name_len == 1 && entry_name[0] == '.') && !(entry->name_len == 2 && entry_name[0] == '.' && entry_name[1] == '.')) {
                char name[256];
                const uint32_t len = entry->name_len < sizeof(name) - 1u ? entry->name_len : sizeof(name) - 1u;
                memcpy(name, entry_name, len);
                name[len] = '\0';
                Ext2Inode child{};
                rad_vfs_stat_t stat{};
                if (read_inode(entry->inode, &child) == RAD_STATUS_OK) {
                    stat.size = inode_size(child);
                    stat.is_directory = inode_is_dir(child);
                    stat.mode = child.mode;
                    stat.mtime_millis = static_cast<uint64_t>(child.mtime) * 1000u;
                }
                if (!callback(name, &stat, callback_context)) return RAD_STATUS_OK;
            }
            pos += entry->rec_len;
        }
        offset += static_cast<uint32_t>(got);
    }
    return RAD_STATUS_OK;
}

rad_vfs_backend_ops_t make_ops(void) {
    rad_vfs_backend_ops_t ops{};
    ops.open = ext2_open;
    ops.read = ext2_read;
    ops.write = ext2_write;
    ops.seek = ext2_seek;
    ops.tell = ext2_tell;
    ops.close = ext2_close;
    ops.stat = ext2_stat;
    ops.list = ext2_list;
    return ops;
}
}

extern "C" rad_status_t x86_ext2_mount_root(const char *block_device) {
    if (!block_device) return RAD_STATUS_INVALID_ARGUMENT;
    memset(&g_ext2, 0, sizeof(g_ext2));
    memset(g_files, 0, sizeof(g_files));
    rad_status_t status = rad_block_open(block_device, &g_ext2.block_device);
    if (status != RAD_STATUS_OK) return status;
    status = rad_block_info(g_ext2.block_device, &g_ext2.block_info);
    if (status != RAD_STATUS_OK) return status;
    Ext2Superblock super{};
    status = disk_read(1024, &super, sizeof(super));
    if (status != RAD_STATUS_OK) return status;
    if (super.magic != Ext2Magic) return RAD_STATUS_INVALID_ARGUMENT;
    g_ext2.block_size = 1024u << super.log_block_size;
    if (g_ext2.block_size == 0 || g_ext2.block_size > MaxBlockSize || g_ext2.block_size % 512u != 0) {
        return RAD_STATUS_NOT_SUPPORTED;
    }
    g_ext2.inode_size = super.inode_size ? super.inode_size : 128u;
    g_ext2.inodes_per_group = super.inodes_per_group;
    g_ext2.blocks_per_group = super.blocks_per_group;
    g_ext2.group_desc_offset = g_ext2.block_size == 1024u ? 2048u : g_ext2.block_size;
    if (!g_ext2.inode_size || !g_ext2.inodes_per_group) return RAD_STATUS_INVALID_ARGUMENT;
    rad_vfs_backend_ops_t ops = make_ops();
    status = rad_vfs_mount_provider("/", &ops);
    if (status != RAD_STATUS_OK) return status;
    g_ext2.mounted = 1;
    rad_debug_marker("RADIX_EXT2_MOUNT_OK");
    return RAD_STATUS_OK;
}

extern "C" int x86_ext2_self_test(void) {
    rad_vfs_stat_t stat{};
    if (rad_vfs_stat("/bin/init", &stat) != RAD_STATUS_OK || stat.is_directory || stat.size == 0) {
        rad_debug_marker("RADIX_ROOTFS_INIT_MISSING");
        return 0;
    }
    rad_debug_marker("RADIX_ROOTFS_INIT_FOUND");
    return 1;
}
