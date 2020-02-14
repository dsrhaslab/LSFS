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

extern std::unique_ptr<client> df_client;

/* -------------------------------------------------------------------------- */

int lsfs_impl::_mknod(
    const char *path, mode_t mode, dev_t rdev
    )
{
    logger->info("MKNOD" + std::string(path));
    logger->flush();

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
    logger->info("LINK From:" + std::string(from) + " To:" + std::string(to));
    logger->flush();

    return (link(from, to) == 0) ? 0 : -errno;
}

int lsfs_impl::_unlink(
    const char *path
    )
{
    logger->info("UNLINK " + std::string(path));
    logger->flush();

    return (unlink(path) == 0) ? 0 : -errno;
}

int lsfs_impl::_rename(
    const char *from, const char *to, unsigned int flags
    )
{
    logger->info("RENAME From:" + std::string(from) + " To:" + std::string(to));
    logger->flush();

    if (flags != 0)
        return -EINVAL;

    return (rename(from, to) == 0) ? 0 : -errno;
}

int lsfs_impl::_mkdir(
    const char *path, mode_t mode
    )
{
    logger->info("MKDIR " + std::string(path));
    logger->flush();

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
    logger->info("RMDIR " + std::string(path));
    logger->flush();

    return (rmdir(path) == 0) ? 0 : -errno;
}

/* -------------------------------------------------------------------------- */
