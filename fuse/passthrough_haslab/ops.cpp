/* -------------------------------------------------------------------------- */

#include "ops.h"

#include "ops_createdelete.h"
#include "ops_dir.h"
#include "ops_file.h"
#include "ops_filesystem.h"
#include "ops_initdestroy.h"
#include "ops_metadata.h"
#include "ops_symlink.h"
#include "ops_xattr.h"

#include "fuse31.h"

/* -------------------------------------------------------------------------- */

const struct fuse_operations FUSE_HIGH_PT_OPS = {


    .getattr     = fuse_high_pt_ops_getattr, // ops_metadata.h
    .readlink    = fuse_high_pt_ops_readlink, // ops_symlink.h
    .mknod       = fuse_high_pt_ops_mknod, // ops_createdelete.h
    .mkdir       = fuse_high_pt_ops_mkdir, // ops_createdelete.h
    .unlink      = fuse_high_pt_ops_unlink, // ops_createdelete.h
    .rmdir       = fuse_high_pt_ops_rmdir, // ops_createdelete.h
    .symlink     = fuse_high_pt_ops_symlink, // ops_symlink.h
    .rename      = fuse_high_pt_ops_rename, // ops_createdelete.h
    .link        = fuse_high_pt_ops_link, // ops_createdelete.h
    .chmod       = fuse_high_pt_ops_chmod, // ops_metadata.h
    .chown       = fuse_high_pt_ops_chown, // ops_metadata.h
    .truncate    = fuse_high_pt_ops_truncate, // ops_metadata.h
    .open        = fuse_high_pt_ops_open, // ops_file.h
    .read        = fuse_high_pt_ops_read, // ops_file.h
    .write       = fuse_high_pt_ops_write, // ops_file.h
    .statfs      = fuse_high_pt_ops_statfs, // ops_filesystem.h
    .release     = fuse_high_pt_ops_release, // ops_file.h
    .fsync       = fuse_high_pt_ops_fsync, // ops_file.h
    .setxattr    = fuse_high_pt_ops_setxattr, // ops_xattr.h
    .getxattr    = fuse_high_pt_ops_getxattr, // ops_xattr.h
    .listxattr   = fuse_high_pt_ops_listxattr, // ops_xattr.h
    .removexattr = fuse_high_pt_ops_removexattr, // ops_xattr.h
    .opendir     = fuse_high_pt_ops_opendir, // ops_dir.h
    .readdir     = fuse_high_pt_ops_readdir, // ops_dir.h
    .releasedir  = fuse_high_pt_ops_releasedir, // ops_dir.h
    .fsyncdir    = fuse_high_pt_ops_fsyncdir, // ops_dir.h
    .init        = fuse_high_pt_ops_init, // ops_initdestroy.h
    .destroy     = fuse_high_pt_ops_destroy, // ops_initdestroy.h
    .create      = fuse_high_pt_ops_create, // ops_file.h
    .utimens     = fuse_high_pt_ops_utimens, // ops_metadata.h
    .write_buf   = fuse_high_pt_ops_write_buf, // ops_metadata.h
    .read_buf    = fuse_high_pt_ops_read_buf, // ops_metadata.h
    .fallocate   = fuse_high_pt_ops_fallocate, // ops_metadata.h
};

/* -------------------------------------------------------------------------- */
