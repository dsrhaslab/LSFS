/* -------------------------------------------------------------------------- */

#define _XOPEN_SOURCE 500
#include <float.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "ops_initdestroy.h"

#include "fuse31.h"
#include "util.h"

/* -------------------------------------------------------------------------- */

static void change_root(const char *new_root)
{
    if (chroot(new_root) != 0)
        print_error_errno_and_fail("chroot(\"%s\") failed", new_root);

    if (chdir("/") != 0)
        print_error_errno_and_fail("chdir(\"/\") failed");
}

/* -------------------------------------------------------------------------- */

void *fuse_high_pt_ops_init(
    struct fuse_conn_info *conn,
    struct fuse_config *cfg
    )
{
    // clear umask

    umask(0);

    // change root and current working directory

    change_root((const char *)fuse_get_context()->private_data);

    // configure fuse

    conn->want |= FUSE_CAP_WRITEBACK_CACHE;

    conn->want |= FUSE_CAP_SPLICE_MOVE;
    conn->want |= FUSE_CAP_SPLICE_READ;
    conn->want |= FUSE_CAP_SPLICE_WRITE;

    cfg->direct_io    = 1; // to avoid double-caching
    cfg->kernel_cache = 1;
    cfg->nullpath_ok  = 1;
    cfg->use_ino      = 1;

    cfg->entry_timeout    = DBL_MAX;
    cfg->attr_timeout     = DBL_MAX;
    cfg->negative_timeout = DBL_MAX;

    // log stuff

    fuse_pt_log("conn->max_write = %u\n", conn->max_write);
    fuse_pt_log("conn->max_read = %u\n", conn->max_read);
    fuse_pt_log("conn->max_readahead = %u\n", conn->max_readahead);
    fuse_pt_log("conn->max_background = %u\n", conn->max_background);
    fuse_pt_log("conn->congestion_threshold = %u\n", conn->congestion_threshold);

    // ---

    return NULL;
}

void fuse_high_pt_ops_destroy(
    void *private_data
    )
{
    (void)private_data;
}

/* -------------------------------------------------------------------------- */
