#ifndef fuse_high_pt_header_ops_createdelete_h_
#define fuse_high_pt_header_ops_createdelete_h_

/* -------------------------------------------------------------------------- */

#include "fuse31.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* -------------------------------------------------------------------------- */

int fuse_high_pt_ops_mknod(
    const char *path, mode_t mode, dev_t rdev
    );

int fuse_high_pt_ops_link(
    const char *from, const char *to
    );

int fuse_high_pt_ops_unlink(
    const char *path
    );

int fuse_high_pt_ops_rename(
    const char *from, const char *to, unsigned int flags
    );

int fuse_high_pt_ops_mkdir(
    const char *path, mode_t mode
    );

int fuse_high_pt_ops_rmdir(
    const char *path
    );

/* -------------------------------------------------------------------------- */

#endif /* fuse_high_pt_header_ops_createdelete_h_ */
