/* -------------------------------------------------------------------------- */

#define _XOPEN_SOURCE 500
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "util.h"
#include "../lsfs_impl.h"

/* -------------------------------------------------------------------------- */

int lsfs_impl::_mknod(
    const char *path, mode_t mode, dev_t rdev
    )
{
    if (!fuse_pt_impersonate_calling_process_highlevel(&mode))
        return -errno;

    const int return_value = (mknod(path, mode, rdev) == 0) ? 0 : -errno;

    fuse_pt_unimpersonate();

    return return_value;
}

int lsfs_impl::_link(
    const char *from, const char *to
    )
{
    return (link(from, to) == 0) ? 0 : -errno;
}

int lsfs_impl::_unlink(
    const char *path
    )
{
    return (unlink(path) == 0) ? 0 : -errno;
}

int lsfs_impl::_rename(
    const char *from, const char *to, unsigned int flags
    )
{
    if (flags != 0)
        return -EINVAL;

    return (rename(from, to) == 0) ? 0 : -errno;
}

int lsfs_impl::_mkdir(
    const char *path, mode_t mode
    )
{
    if (!fuse_pt_impersonate_calling_process_highlevel(&mode))
        return -errno;

    const int return_value = (mkdir(path, mode) == 0) ? 0 : -errno;

    fuse_pt_unimpersonate();

    return return_value;
}

int lsfs_impl::_rmdir(
    const char *path
    )
{
    return (rmdir(path) == 0) ? 0 : -errno;
}

/* -------------------------------------------------------------------------- */
