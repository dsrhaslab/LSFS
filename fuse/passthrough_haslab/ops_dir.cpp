/* -------------------------------------------------------------------------- */

#define _ATFILE_SOURCE
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 500
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "util.h"
#include "../lsfs_impl.h"

/* -------------------------------------------------------------------------- */

struct dir_handle
{
    DIR *dir_ptr;
    struct dirent *entry;
    off_t offset;
};

inline static struct dir_handle *get_dir_handle(struct fuse_file_info *fi)
{
    return (struct dir_handle *)(uintptr_t)fi->fh;
}

/* -------------------------------------------------------------------------- */

int lsfs_impl::_opendir(
    const char *path,
    struct fuse_file_info *fi
    )
{
    if (path){
        logger->info("OPENDIR " + std::string(path));
        logger->flush();
    }else{
        logger->info("OPENDIR");
        logger->flush();
    }

    // allocate dir_handle

    struct dir_handle *const dir = static_cast<dir_handle *const>(malloc(sizeof(struct dir_handle)));

    if (!dir)
        return -ENOMEM;

    // initialize dir_handle

    dir->dir_ptr = opendir(path);
    dir->entry   = NULL;
    dir->offset  = 0;

    if (!dir->dir_ptr)
    {
        const int err = errno;

        free(dir);

        return -err;
    }

    // ---

    fi->fh = (uint64_t)(uintptr_t)dir;

    return 0;
}

int lsfs_impl::_releasedir(
    const char *path,
    struct fuse_file_info *fi
    )
{
    if (path){
        logger->info("RELEASEDIR " + std::string(path));
        logger->flush();
    }else{
        logger->info("RELEASEDIR");
        logger->flush();
    }

    (void)path;

    struct dir_handle *const dir = get_dir_handle(fi);

    if (closedir(dir->dir_ptr) != 0)
        return -errno;

    free(dir);

    return 0;
}

int lsfs_impl::_fsyncdir(
    const char *path, int isdatasync,
    struct fuse_file_info *fi
    )
{
//    logger->info("FSYNCDIR ");
//    logger->flush();

    (void)path;

    const int fd = dirfd(get_dir_handle(fi)->dir_ptr);

    const int result = isdatasync ? fdatasync(fd) : fsync(fd);

    return (result == 0) ? 0 : -errno;
}

int lsfs_impl::_readdir(
        const char *path, void *buf, fuse_fill_dir_t filler,
        off_t offset, struct fuse_file_info *fi,
        enum fuse_readdir_flags flags)
{
    if (path){
        logger->info("READDIR " + std::string(path));
        logger->flush();
    }else{
        logger->info("READDIR");
        logger->flush();
    }

    (void)path;

    struct dir_handle *const dir = get_dir_handle(fi);

    if (dir->offset != offset)
    {
        seekdir(dir->dir_ptr, offset);

        dir->entry  = NULL;
        dir->offset = offset;
    }

    while (true)
    {
        if (!dir->entry)
        {
            dir->entry = readdir(dir->dir_ptr);

            if (!dir->entry)
                break;
        }

        struct stat st;
        enum fuse_fill_dir_flags fill_flags = static_cast<fuse_fill_dir_flags>(0);

        if (flags & FUSE_READDIR_PLUS)
        {
            const int res = fstatat(
                dirfd(dir->dir_ptr), dir->entry->d_name, &st,
                AT_SYMLINK_NOFOLLOW
                );

            if (res != -1)
                fill_flags = static_cast<fuse_fill_dir_flags>(fill_flags | FUSE_FILL_DIR_PLUS);
        }

        if (!(fill_flags & FUSE_FILL_DIR_PLUS))
        {
            memset(&st, 0, sizeof st);

            st.st_ino  = dir->entry->d_ino;
            st.st_mode = dir->entry->d_type << 12;
        }

        const off_t next_offset = telldir(dir->dir_ptr);

        if (filler(buf, dir->entry->d_name, &st, next_offset, fill_flags))
            break;

        dir->entry  = NULL;
        dir->offset = next_offset;
    }

    return 0;
}

/* -------------------------------------------------------------------------- */
