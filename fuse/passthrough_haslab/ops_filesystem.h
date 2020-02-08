#ifndef fuse_high_pt_header_ops_filesystem_h_
#define fuse_high_pt_header_ops_filesystem_h_

/* -------------------------------------------------------------------------- */

#include "fuse31.h"

#include <sys/statvfs.h>

/* -------------------------------------------------------------------------- */

int fuse_high_pt_ops_statfs(
    const char *path, struct statvfs *stbuf
    );

/* -------------------------------------------------------------------------- */

#endif /* fuse_high_pt_header_ops_filesystem_h_ */
