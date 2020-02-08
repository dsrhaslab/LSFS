#ifndef fuse_high_pt_header_ops_initdestroy_h_
#define fuse_high_pt_header_ops_initdestroy_h_

/* -------------------------------------------------------------------------- */

#include "fuse31.h"

/* -------------------------------------------------------------------------- */

void *fuse_high_pt_ops_init(
    struct fuse_conn_info *conn,
    struct fuse_config *cfg
    );

void fuse_high_pt_ops_destroy(
    void *private_data
    );

/* -------------------------------------------------------------------------- */

#endif /* fuse_high_pt_header_ops_initdestroy_h_ */
