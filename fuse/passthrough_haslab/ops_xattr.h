#ifndef fuse_high_pt_header_ops_xattr_h_
#define fuse_high_pt_header_ops_xattr_h_

/* -------------------------------------------------------------------------- */

#include "fuse31.h"

#include <stddef.h>

/* -------------------------------------------------------------------------- */

int fuse_high_pt_ops_getxattr(
    const char *path,
    const char *name, char *value, size_t size
    );

int fuse_high_pt_ops_setxattr(
    const char *path,
    const char *name, const char *value, size_t size,
    int flags
    );

int fuse_high_pt_ops_listxattr(
    const char *path,
    char *list, size_t size
    );

int fuse_high_pt_ops_removexattr(
    const char *path,
    const char *name
    );

/* -------------------------------------------------------------------------- */

#endif /* fuse_high_pt_header_ops_xattr_h_ */
