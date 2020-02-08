/* -------------------------------------------------------------------------- */

#define _ATFILE_SOURCE
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 500
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "ops_metadata.h"

#include "fuse31.h"
#include "util.h"

/* -------------------------------------------------------------------------- */

int fuse_high_pt_ops_getattr(
    const char *path, struct stat *stbuf,
    struct fuse_file_info *fi
    )
{
    const int result = fi ? fstat((int)fi->fh, stbuf) : lstat(path, stbuf);

    return (result == 0) ? 0 : -errno;
}

int fuse_high_pt_ops_chmod(
    const char *path, mode_t mode,
    struct fuse_file_info *fi
    )
{
    const int result = fi ? fchmod((int)fi->fh, mode) : chmod(path, mode);

    return (result == 0) ? 0 : -errno;
}

int fuse_high_pt_ops_chown(
    const char *path, uid_t uid, gid_t gid,
    struct fuse_file_info *fi
    )
{
    const int result =
        fi ?
        fchown((int)fi->fh, uid, gid) :
        lchown(path, uid, gid);

    return (result == 0) ? 0 : -errno;
}

int fuse_high_pt_ops_utimens(
    const char *path, const struct timespec ts[2],
    struct fuse_file_info *fi
    )
{
    int result;

    if (fi)
        result = futimens((int)fi->fh, ts);
    else
        result = utimensat(AT_FDCWD, path, ts, AT_SYMLINK_NOFOLLOW);

    return (result == 0) ? 0 : -errno;
}

int fuse_high_pt_ops_truncate(
    const char *path, off_t size,
    struct fuse_file_info *fi
    )
{
    const int result = fi ? ftruncate((int)fi->fh, size) : truncate(path, size);

    return (result == 0) ? 0 : -errno;
}

int fuse_high_pt_ops_fallocate(
    const char *path, int mode, off_t offset, off_t length,
    struct fuse_file_info *fi
    )
{
    (void)path;

    return (fallocate((int)fi->fh, mode, offset, length) == 0) ? 0 : -errno;
}

/* -------------------------------------------------------------------------- */
