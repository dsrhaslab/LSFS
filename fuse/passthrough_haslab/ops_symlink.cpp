/* -------------------------------------------------------------------------- */

#define _XOPEN_SOURCE 500
#include <errno.h>
#include <stddef.h>
#include <unistd.h>

#include "ops_symlink.h"

#include "fuse31.h"
#include "util.h"

/* -------------------------------------------------------------------------- */

int fuse_high_pt_ops_symlink(
    const char *from, const char *to
    )
{
    if (!fuse_pt_impersonate_calling_process_highlevel(NULL))
        return -errno;

    const int return_value = (symlink(from, to) == 0) ? 0 : -errno;

    fuse_pt_unimpersonate();

    return return_value;
}

int fuse_high_pt_ops_readlink(
    const char *path, char *buf, size_t size
    )
{
    if (size == 0)
        return -EINVAL;

    const ssize_t result = readlink(path, buf, size - 1);

    if (result < 0)
        return -errno;

    buf[result] = '\0';

    return 0;
}

/* -------------------------------------------------------------------------- */
