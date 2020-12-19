//
// Created by danielsf97 on 2/4/20.
//

#ifndef P2PFS_FUSE_WRAPPER_H
#define P2PFS_FUSE_WRAPPER_H

#define FUSE_PASSTHROUGH_DEBUG

#ifndef FUSE_USE_VERSION
#define FUSE_USE_VERSION 35
#endif

#include "fuse35.h"
#include "fuse_utils.h"
#include <cstring>
#include <iostream>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <cstddef>
#include <cassert>

namespace fuse_wrapper
{
    typedef int(*t_getattr)(const char *, struct stat *, struct fuse_file_info *);
    typedef int(*t_readlink)(const char *, char *, size_t);
    typedef int(*t_mknod) (const char *, mode_t, dev_t);
    typedef int(*t_mkdir) (const char *, mode_t);
    typedef int(*t_unlink) (const char *);
    typedef int(*t_rmdir) (const char *);
    typedef int(*t_symlink) (const char *, const char *);
    typedef int(*t_rename) (const char *, const char *,  unsigned int);
    typedef int(*t_link) (const char *, const char *);
    typedef int(*t_chmod) (const char *, mode_t, struct fuse_file_info *);
    typedef int(*t_chown) (const char *, uid_t, gid_t, fuse_file_info *);
    typedef int(*t_truncate) (const char *, off_t, fuse_file_info *);
    typedef int(*t_open) (const char *, struct fuse_file_info *);
    typedef int(*t_read) (const char *, char *, size_t, off_t, struct fuse_file_info *);
    typedef int(*t_write) (const char *, const char *, size_t, off_t,struct fuse_file_info *);
    typedef int(*t_statfs) (const char *, struct statvfs *);
    typedef int(*t_flush) (const char *, struct fuse_file_info *);
    typedef int(*t_release) (const char *, struct fuse_file_info *);
    typedef int(*t_fsync) (const char *, int, struct fuse_file_info *);
    typedef int(*t_setxattr) (const char *, const char *, const char *, size_t, int);
    typedef int(*t_getxattr) (const char *, const char *, char *, size_t);
    typedef int(*t_listxattr) (const char *, char *, size_t);
    typedef int(*t_removexattr) (const char *, const char *);
    typedef int(*t_opendir) (const char *, struct fuse_file_info *);
    typedef int(*t_readdir) (const char *, void *, fuse_fill_dir_t, off_t, struct fuse_file_info *);
    typedef int(*t_releasedir) (const char *, struct fuse_file_info *);
    typedef int(*t_fsyncdir) (const char *, int, struct fuse_file_info *);
    typedef void*(*t_init) (struct fuse_conn_info *, struct fuse_config *cfg);
    typedef void(*t_destroy) (void *);
    typedef int(*t_access) (const char *, int);
    typedef int(*t_create) (const char *, mode_t, struct fuse_file_info *);
    typedef int(*t_lock) (const char *, struct fuse_file_info *, int cmd, struct flock *);
    typedef int(*t_utimens) (const char *, const struct timespec tv[2], struct fuse_file_info *fi);
    typedef int(*t_bmap) (const char *, size_t blocksize, uint64_t *idx);
    typedef int(*t_ioctl) (const char *, unsigned int cmd, void *arg, struct fuse_file_info *, unsigned int flags, void *data);
    typedef int(*t_poll) (const char *, struct fuse_file_info *, struct fuse_pollhandle *ph, unsigned *reventsp);
    typedef int(*t_write_buf) (const char *, struct fuse_bufvec *buf, off_t off, struct fuse_file_info *);
    typedef int(*t_read_buf) (const char *, struct fuse_bufvec **bufp, size_t size, off_t off, struct fuse_file_info *);
    typedef int (*t_flock) (const char *, struct fuse_file_info *, int op);
    typedef int (*t_fallocate) (const char *, int, off_t, off_t, struct fuse_file_info *);

    template <class T> class fuse
    {
    public:
        fuse()
        {
            memset (&T::operations_, 0, sizeof (struct fuse_operations));
            load_operations_();
        }

        // no copy
        fuse(const fuse&) = delete;
        fuse& operator=(const fuse&) = delete;

        ~fuse() = default;

        static void parse_args( int argc, char *argv[], const char **mount_point, const char **underlying_root)
        {
            if (argc != 3)
            {
                fprintf(stderr, "Usage: %s <mount_point> <underlying_root>\n", argv[0]);
                exit(1);
            }
            else
            {
                *mount_point     = argv[1];
                *underlying_root = argv[2];
            }
        }

        static void add_fuse_arg(struct fuse_args *args, const char *value)
        {
            if (fuse_opt_add_arg(args, value) != 0)
                print_error_and_fail("fuse_opt_add_arg failed");
        }

        static struct fuse_args create_fuse_args(
            const char *arg0, const char *mount_point
            )
        {
            struct fuse_args args = FUSE_ARGS_INIT(0, NULL);

            add_fuse_arg(&args, arg0);
            add_fuse_arg(&args, mount_point);
            add_fuse_arg(&args, "-oallow_other");
            add_fuse_arg(&args, "-odefault_permissions");

        #ifdef FUSE_PASSTHROUGH_DEBUG
            add_fuse_arg(&args, "-f");
            //add_fuse_arg(&args, "-d");
        #endif

            return args;
        }

        auto run(int argc, char *argv [], void *userdata /*not used by now*/)
        {
            // parse args

            const char *mount_point;
            const char *underlying_root;

            parse_args(argc, argv, &mount_point, &underlying_root);

            // check that we are running as root

            fuse_pt_assert_super_user();

            // fuse main

            struct fuse_args args = create_fuse_args(argv[0], mount_point);

            const int result = fuse_main(
                args.argc, args.argv,
                operations(), (void *)underlying_root //userdata
                );

            fuse_opt_free_args(&args);

            // ---

            return result;
        }

        auto operations() { return &operations_; }

    private:

        static void load_operations_()
        {
            operations_.getattr = T::_getattr;
            operations_.readlink = T::_readlink;
            operations_.mknod = T::_mknod;
            operations_.mkdir = T::_mkdir;
            operations_.unlink = T::_unlink;
            operations_.rmdir = T::_rmdir;
            operations_.symlink = T::_symlink;
            operations_.rename = T::_rename;
            operations_.link = T::_link;
            operations_.chmod = T::_chmod;
            operations_.chown = T::_chown;
            operations_.truncate = T::_truncate;
            operations_.open = T::_open;
            operations_.read = T::_read;
            operations_.write = T::_write;
            operations_.statfs = T::_statfs;
            operations_.flush = T::_flush;
            operations_.release = T::_release;
            operations_.fsync = T::_fsync;
            operations_.setxattr = T::_setxattr;
            operations_.getxattr = T::_getxattr;
            operations_.listxattr = T::_listxattr;
            operations_.removexattr = T::_removexattr;
            operations_.opendir = T::_opendir;
            operations_.readdir = T::_readdir;
            operations_.releasedir = T::_releasedir;
            operations_.fsyncdir = T::_fsyncdir;
            operations_.init = T::_init;
            operations_.destroy = T::_destroy;
            operations_.access = T::_access;
            operations_.create = T::_create;
            operations_.lock = T::_lock;
            operations_.utimens = T::_utimens;
            operations_.bmap = T::_bmap;
            operations_.ioctl = T::_ioctl;
            operations_.poll = T::_poll;
            operations_.write_buf = T::_write_buf;
            operations_.read_buf = T::_read_buf;
            operations_.flock = T::_flock;
            operations_.fallocate = T::_fallocate;
        }

        static struct fuse_operations operations_;

        static t_getattr _getattr ;
        static t_readlink _readlink;
        static t_mknod _mknod;
        static t_mkdir _mkdir;
        static t_unlink _unlink;
        static t_rmdir _rmdir;
        static t_symlink _symlink;
        static t_rename _rename;
        static t_link _link;
        static t_chmod _chmod;
        static t_chown _chown;
        static t_truncate _truncate;
        static t_open _open;
        static t_read _read;
        static t_write _write;
        static t_statfs _statfs;
        static t_flush _flush;
        static t_release _release;
        static t_fsync _fsync;
        static t_setxattr _setxattr;
        static t_getxattr _getxattr;
        static t_listxattr _listxattr;
        static t_removexattr _removexattr;
        static t_opendir _opendir;
        static t_readdir _readdir;
        static t_releasedir _releasedir;
        static t_fsyncdir _fsyncdir;
        static t_init _init;
        static t_destroy _destroy;
        static t_access _access;
        static t_create _create;
        static t_lock _lock;
        static t_utimens _utimens;
        static t_bmap _bmap;
        static t_ioctl _ioctl;
        static t_poll _poll;
        static t_write_buf _write_buf;
        static t_read_buf _read_buf;
        static t_flock _flock;
        static t_fallocate _fallocate;
    } ;
}

#endif //P2PFS_FUSE_WRAPPER_H
