/* -------------------------------------------------------------------------- */

#define _XOPEN_SOURCE 500
#include <errno.h>
#include <stddef.h>
#include <unistd.h>

#include "util.h"
#include "fuse/fuse_lsfs/lsfs_impl.h"

/* -------------------------------------------------------------------------- */

int lsfs_impl::_symlink(
    const char *from, const char *to
    )
{
    logger->info("SYMLINK FROM:" + std::string(from) + " TO:" + std::string(to));
    logger->flush();

    if (!fuse_pt_impersonate_calling_process_highlevel(NULL))
        return -errno;

    const int return_value = (symlink(from, to) == 0) ? 0 : -errno;

    fuse_pt_unimpersonate();

    return return_value;
}

int lsfs_impl::_readlink(
    const char *path, char *buf, size_t size
    )
{
    logger->info("READLINK " + std::string(path) + " SIZE:" + std::to_string(size));
    logger->flush();

    if (size == 0)
        return -EINVAL;

    const ssize_t result = readlink(path, buf, size - 1);

    if (result < 0)
        return -errno;

    buf[result] = '\0';

    return 0;
}

/* -------------------------------------------------------------------------- */
