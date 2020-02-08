#ifndef fuse_high_pt_header_ops_symlink_h_
#define fuse_high_pt_header_ops_symlink_h_

/* -------------------------------------------------------------------------- */

#include "fuse31.h"

#include <stddef.h>

/* -------------------------------------------------------------------------- */

int fuse_high_pt_ops_symlink(
    const char *from, const char *to
    );

int fuse_high_pt_ops_readlink(
    const char *path, char *buf, size_t size
    );

/* -------------------------------------------------------------------------- */

#endif /* fuse_high_pt_header_ops_symlink_h_ */
