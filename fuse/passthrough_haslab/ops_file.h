#ifndef fuse_high_pt_header_ops_file_h_
#define fuse_high_pt_header_ops_file_h_

/* -------------------------------------------------------------------------- */

#include "fuse31.h"

#include <fcntl.h>
#include <stddef.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* -------------------------------------------------------------------------- */

int fuse_high_pt_ops_create(
    const char *path, mode_t mode,
    struct fuse_file_info *fi
    );

int fuse_high_pt_ops_open(
    const char *path,
    struct fuse_file_info *fi
    );

int fuse_high_pt_ops_release(
    const char *path,
    struct fuse_file_info *fi
    );

int fuse_high_pt_ops_fsync(
    const char *path, int isdatasync,
    struct fuse_file_info *fi
    );

int fuse_high_pt_ops_read(
    const char *path, char *buf, size_t size, off_t offset,
    struct fuse_file_info *fi
    );

int fuse_high_pt_ops_write(
    const char *path, const char *buf, size_t size, off_t offset,
    struct fuse_file_info *fi
    );

int fuse_high_pt_ops_read_buf(
    const char *path, struct fuse_bufvec **bufp, size_t size, off_t offset,
    struct fuse_file_info *fi
    );

int fuse_high_pt_ops_write_buf(
    const char *path, struct fuse_bufvec *buf, off_t offset,
    struct fuse_file_info *fi
    );

/* -------------------------------------------------------------------------- */

#endif /* fuse_high_pt_header_ops_file_h_ */
