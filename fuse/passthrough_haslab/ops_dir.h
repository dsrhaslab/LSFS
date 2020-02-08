#ifndef fuse_high_pt_header_ops_dir_h_
#define fuse_high_pt_header_ops_dir_h_

/* -------------------------------------------------------------------------- */

#include "fuse31.h"

#include <unistd.h>

/* -------------------------------------------------------------------------- */

int fuse_high_pt_ops_opendir(
    const char *path,
    struct fuse_file_info *fi
    );

int fuse_high_pt_ops_releasedir(
    const char *path,
    struct fuse_file_info *fi
    );

int fuse_high_pt_ops_fsyncdir(
    const char *path, int isdatasync,
    struct fuse_file_info *fi
    );

int fuse_high_pt_ops_readdir(
    const char *path, void *buf, fuse_fill_dir_t filler,
    off_t offset, struct fuse_file_info *fi,
    enum fuse_readdir_flags flags
    );

/* -------------------------------------------------------------------------- */

#endif /* fuse_high_pt_header_ops_dir_h_ */
