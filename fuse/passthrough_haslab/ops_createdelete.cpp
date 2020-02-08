/* -------------------------------------------------------------------------- */

#define _XOPEN_SOURCE 500
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "ops_createdelete.h"

#include "fuse31.h"
#include "util.h"

/* -------------------------------------------------------------------------- */

int fuse_high_pt_ops_mknod(
    const char *path, mode_t mode, dev_t rdev
    )
{
    if (!fuse_pt_impersonate_calling_process_highlevel(&mode))
        return -errno;

    const int return_value = (mknod(path, mode, rdev) == 0) ? 0 : -errno;

    fuse_pt_unimpersonate();

    return return_value;
}

int fuse_high_pt_ops_link(
    const char *from, const char *to
    )
{
    return (link(from, to) == 0) ? 0 : -errno;
}

int fuse_high_pt_ops_unlink(
    const char *path
    )
{
    return (unlink(path) == 0) ? 0 : -errno;
}

int fuse_high_pt_ops_rename(
    const char *from, const char *to, unsigned int flags
    )
{
    if (flags != 0)
        return -EINVAL;

    return (rename(from, to) == 0) ? 0 : -errno;
}

int fuse_high_pt_ops_mkdir(
    const char *path, mode_t mode
    )
{
    if (!fuse_pt_impersonate_calling_process_highlevel(&mode))
        return -errno;

    const int return_value = (mkdir(path, mode) == 0) ? 0 : -errno;

    fuse_pt_unimpersonate();

    return return_value;
}

int fuse_high_pt_ops_rmdir(
    const char *path
    )
{
    return (rmdir(path) == 0) ? 0 : -errno;
}

/* -------------------------------------------------------------------------- */
