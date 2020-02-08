#ifndef fuse_high_pt_header_ops_metadata_h_
#define fuse_high_pt_header_ops_metadata_h_

/* -------------------------------------------------------------------------- */

#include "fuse31.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* -------------------------------------------------------------------------- */

int fuse_high_pt_ops_getattr(
    const char *path, struct stat *stbuf,
    struct fuse_file_info *fi
    );

int fuse_high_pt_ops_chmod(
    const char *path, mode_t mode,
    struct fuse_file_info *fi
    );

int fuse_high_pt_ops_chown(
    const char *path, uid_t uid, gid_t gid,
    struct fuse_file_info *fi
    );

int fuse_high_pt_ops_utimens(
    const char *path, const struct timespec ts[2],
    struct fuse_file_info *fi
    );

int fuse_high_pt_ops_truncate(
    const char *path, off_t size,
    struct fuse_file_info *fi
    );

int fuse_high_pt_ops_fallocate(
    const char *path, int mode, off_t offset, off_t length,
    struct fuse_file_info *fi
    );

/* -------------------------------------------------------------------------- */

#endif /* fuse_high_pt_header_ops_metadata_h_ */
