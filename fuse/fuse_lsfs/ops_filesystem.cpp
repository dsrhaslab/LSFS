/* -------------------------------------------------------------------------- */

#include <errno.h>
#include <sys/statvfs.h>

#include "util.h"
#include "fuse/fuse_lsfs/lsfs_impl.h"

/* -------------------------------------------------------------------------- */

int lsfs_impl::_statfs(
    const char *path, struct statvfs *stbuf
    )
{
    return (statvfs(path, stbuf) == 0) ? 0 : -errno;
}

/* -------------------------------------------------------------------------- */
