/* -------------------------------------------------------------------------- */

#include <errno.h>
#include <sys/statvfs.h>

#include "ops_filesystem.h"

#include "fuse31.h"
#include "util.h"

/* -------------------------------------------------------------------------- */

int fuse_high_pt_ops_statfs(
    const char *path, struct statvfs *stbuf
    )
{
    return (statvfs(path, stbuf) == 0) ? 0 : -errno;
}

/* -------------------------------------------------------------------------- */
