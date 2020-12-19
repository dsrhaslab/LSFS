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
#include "lsfs/fuse_lsfs/lsfs_impl.h"

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
    return 0;
}

int lsfs_impl::_releasedir(
    const char *path,
    struct fuse_file_info *fi
    )
{
    (void)path;

    return 0;
}

int lsfs_impl::_fsyncdir(
    const char *path, int isdatasync,
    struct fuse_file_info *fi
    )
{
    (void)path;

    return 0;
}

int lsfs_impl::_readdir(
        const char *path, void *buf, fuse_fill_dir_t filler,
        off_t offset, struct fuse_file_info *fi,
        enum fuse_readdir_flags flags)
{
    (void)path;

    enum fuse_fill_dir_flags fill_flags = static_cast<fuse_fill_dir_flags>(0);

    filler(buf, ".", NULL, 0, fill_flags); // Current Directory
    if (strcmp(path, "/") != 0) {
        filler(buf, "..", NULL, 0, fill_flags); // Parent Directory
    }

    std::unique_ptr<metadata> met = state->get_metadata(path);

    if(met == nullptr){
        return -errno;
    }

    for(auto& child: met->childs){
        filler(buf, child.c_str(), NULL, 0, fill_flags);
    }

    return 0;
}

/* -------------------------------------------------------------------------- */