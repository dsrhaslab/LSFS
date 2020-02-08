/* -------------------------------------------------------------------------- */

#define _XOPEN_SOURCE 500
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "ops_file.h"

#include "fuse31.h"
#include "util.h"

/* -------------------------------------------------------------------------- */

static int create_or_open(
    bool create,
    const char *path, mode_t mode,
    struct fuse_file_info *fi
    )
{
    if (fi->flags & O_DIRECT)
    {
        // If O_DIRECT was specified, enable direct IO for the FUSE file system,
        // but ignore it when accessing the underlying file system. (If it was
        // not ignored, pread and pwrite could fail, as the buffer given by FUSE
        // may not be correctly aligned.)

        fi->flags &= ~O_DIRECT;
        fi->direct_io = 1;
    }

    int result;

    if (create)
        result = open(path, fi->flags, mode);
    else
        result = open(path, fi->flags);

    if (result == -1)
        return -errno;

    fi->fh = (uint64_t)result;

    return 0;
}

/* -------------------------------------------------------------------------- */

int fuse_high_pt_ops_create(
    const char *path, mode_t mode,
    struct fuse_file_info *fi
    )
{
    if (!fuse_pt_impersonate_calling_process_highlevel(&mode))
        return -errno;

    const int return_value = create_or_open(true, path, mode, fi);

    fuse_pt_unimpersonate();

    return return_value;
}

int fuse_high_pt_ops_open(
    const char *path,
    struct fuse_file_info *fi
    )
{
    return create_or_open(false, path, 0, fi);
}

int fuse_high_pt_ops_release(
    const char *path,
    struct fuse_file_info *fi
    )
{
    (void)path;

    return (close((int)fi->fh) == 0) ? 0 : -errno;
}

int fuse_high_pt_ops_fsync(
    const char *path, int isdatasync,
    struct fuse_file_info *fi
    )
{
    (void)path;

    const int fd = (int)fi->fh;

    const int result = isdatasync ? fdatasync(fd) : fsync(fd);

    return (result == 0) ? 0 : -errno;
}

int fuse_high_pt_ops_read(
    const char *path, char *buf, size_t size, off_t offset,
    struct fuse_file_info *fi
    )
{
    (void)path;

    const int result = pread((int)fi->fh, buf, size, offset);

    if (result == -1)
        return -errno;

    return result;
}

int fuse_high_pt_ops_write(
    const char *path, const char *buf, size_t size, off_t offset,
    struct fuse_file_info *fi
    )
{
    (void)path;

    const int result = pwrite((int)fi->fh, buf, size, offset);

    if (result == -1)
        return -errno;

    return result;
}

int fuse_high_pt_ops_read_buf(
    const char *path, struct fuse_bufvec **bufp, size_t size, off_t offset,
    struct fuse_file_info *fi
    )
{
    (void)path;

    struct fuse_bufvec *const src = static_cast<fuse_bufvec *const>(malloc(sizeof(struct fuse_bufvec)));

    if (!src)
        return -ENOMEM;

    *src = FUSE_BUFVEC_INIT(size);

    src->buf[0].flags = static_cast<fuse_buf_flags>(FUSE_BUF_IS_FD | FUSE_BUF_FD_SEEK);
    src->buf[0].fd    = (int)fi->fh;
    src->buf[0].pos   = offset;

    *bufp = src;

    return 0;
}

int fuse_high_pt_ops_write_buf(
    const char *path, struct fuse_bufvec *buf, off_t offset,
    struct fuse_file_info *fi
    )
{
    (void)path;

    struct fuse_bufvec dst = FUSE_BUFVEC_INIT(fuse_buf_size(buf));

    dst.buf[0].flags = static_cast<fuse_buf_flags>(FUSE_BUF_IS_FD | FUSE_BUF_FD_SEEK);
    dst.buf[0].fd    = (int)fi->fh;
    dst.buf[0].pos   = offset;

    return fuse_buf_copy(&dst, buf, FUSE_BUF_SPLICE_NONBLOCK);
}

/* -------------------------------------------------------------------------- */
