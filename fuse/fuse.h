//
// Created by danielsf97 on 2/4/20.
//

#ifndef P2PFS_FUSE_H
#define P2PFS_FUSE_H

#ifndef FUSE_USE_VERSION
#define FUSE_USE_VERSION 35
#endif

#include <fuse/fuse_lowlevel.h>
#include <cstring>
#include <iostream>

namespace fuse_wrapper
{
    typedef void(* t_init)(void *userdata, struct fuse_conn_info *conn);
    typedef void(* t_destroy )(void *userdata);
    typedef void(* t_lookup )(fuse_req_t req, fuse_ino_t parent, const char *name);
    typedef void(* t_forget )(fuse_req_t req, fuse_ino_t ino, uint64_t nlookup);
    typedef void(* t_getattr )(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
    typedef void(* t_setattr )(fuse_req_t req, fuse_ino_t ino, struct stat *attr, int to_set, struct fuse_file_info *fi);
    typedef void(* t_readlink )(fuse_req_t req, fuse_ino_t ino);
    typedef void(* t_mknod )(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode, dev_t rdev);
    typedef void(* t_mkdir )(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode);
    typedef void(* t_unlink )(fuse_req_t req, fuse_ino_t parent, const char *name);
    typedef void(* t_rmdir )(fuse_req_t req, fuse_ino_t parent, const char *name);
    typedef void(* t_symlink )(fuse_req_t req, const char *link, fuse_ino_t parent, const char *name);
    typedef void(* t_rename )(fuse_req_t req, fuse_ino_t parent, const char *name, fuse_ino_t newparent, const char *newname, unsigned int flags);
    typedef void(* t_link )(fuse_req_t req, fuse_ino_t ino, fuse_ino_t newparent, const char *newname);
    typedef void(* t_open )(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
    typedef void(* t_read )(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi);
    typedef void(* t_write )(fuse_req_t req, fuse_ino_t ino, const char *buf, size_t size, off_t off, struct fuse_file_info *fi);
    typedef void(* t_flush )(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
    typedef void(* t_release )(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
    typedef void(* t_fsync )(fuse_req_t req, fuse_ino_t ino, int datasync, struct fuse_file_info *fi);
    typedef void(* t_opendir )(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
    typedef void(* t_readdir )(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi);
    typedef void(* t_releasedir )(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
    typedef void(* t_fsyncdir )(fuse_req_t req, fuse_ino_t ino, int datasync, struct fuse_file_info *fi);
    typedef void(* t_statfs )(fuse_req_t req, fuse_ino_t ino);
    typedef void(* t_setxattr )(fuse_req_t req, fuse_ino_t ino, const char *name, const char *value, size_t size, int flags);
    typedef void(* t_getxattr )(fuse_req_t req, fuse_ino_t ino, const char *name, size_t size);
    typedef void(* t_listxattr )(fuse_req_t req, fuse_ino_t ino, size_t size);
    typedef void(* t_removexattr )(fuse_req_t req, fuse_ino_t ino, const char *name);
    typedef void(* t_access )(fuse_req_t req, fuse_ino_t ino, int mask);
    typedef void(* t_create )(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode, struct fuse_file_info *fi);
    typedef void(* t_getlk )(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi, struct flock *lock);
    typedef void(* t_setlk )(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi, struct flock *lock, int sleep);
    typedef void(* t_bmap )(fuse_req_t req, fuse_ino_t ino, size_t blocksize, uint64_t idx);
    typedef void(* t_ioctl )(fuse_req_t req, fuse_ino_t ino, int cmd, void *arg, struct fuse_file_info *fi, unsigned flags, const void *in_buf, size_t in_bufsz, size_t out_bufsz);
    typedef void(* t_poll )(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi, struct fuse_pollhandle *ph);
    typedef void(* t_write_buf )(fuse_req_t req, fuse_ino_t ino, struct fuse_bufvec *bufv, off_t off, struct fuse_file_info *fi);
    typedef void(* t_retrieve_reply )(fuse_req_t req, void *cookie, fuse_ino_t ino, off_t offset, struct fuse_bufvec *bufv);
    typedef void(* t_forget_multi )(fuse_req_t req, size_t count, struct fuse_forget_data *forgets);
    typedef void(* t_flock )(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi, int op);
    typedef void(* t_fallocate )(fuse_req_t req, fuse_ino_t ino, int mode, off_t offset, off_t length, struct fuse_file_info *fi);
    typedef void(* t_readdirplus )(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi);
    typedef void(* t_copy_file_range )(fuse_req_t req, fuse_ino_t ino_in, off_t off_in, struct fuse_file_info *fi_in, fuse_ino_t ino_out, off_t off_out, struct fuse_file_info *fi_out, size_t len, int flags);
    typedef void(* t_lseek )(fuse_req_t req, fuse_ino_t ino, off_t off, int whence, struct fuse_file_info *fi);

    template <class T> class fuse
    {
    public:
        fuse()
        {
            memset (&T::operations_, 0, sizeof (struct fuse_lowlevel_ops));
            load_operations_();
        }

        // no copy
        fuse(const fuse&) = delete;
        fuse& operator=(const fuse&) = delete;

        ~fuse() = default;

        char **copy_args(int argc, char * argv[]) {
            char **new_argv = new char*[argc];
            for (int i = 0; i < argc; ++i) {
                int len = (int) strlen(argv[i]) + 1;
                new_argv[i] = new char[len];
                strncpy(new_argv[i], argv[i], len);
            }
            return new_argv;
        }

        void delete_args(int argc, char **argv) {
            for (int i = 0; i < argc; ++i) {
                delete argv[i];
            }
            delete argv;
        }

        auto run(int argc, char *argv [], void *userdata)
        {
            char **fuse_argv = copy_args(argc, argv);

            struct fuse_args args = FUSE_ARGS_INIT(argc, fuse_argv);
            struct fuse_chan *ch;
            char *mountpoint;
            int err = -1;

            if (fuse_parse_cmdline(&args, &mountpoint, NULL, NULL) != -1) {
                if (mountpoint == NULL) {
                    std::cerr << "USAGE: fuse-cpp-ramfs MOUNTPOINT" << std::endl;
                } else if ((ch = fuse_mount(mountpoint, &args)) != NULL) {
                    struct fuse_session *se;

                    se = fuse_lowlevel_new(&args, operations(),
                                           sizeof(operations_), userdata);
                    if (se != NULL) {
                        if (fuse_set_signal_handlers(se) != -1) {
                            fuse_session_add_chan(se, ch);
                            err = fuse_session_loop(se);
                            fuse_remove_signal_handlers(se);
                            fuse_session_remove_chan(ch);
                        }
                        fuse_session_destroy(se);
                    }
                    fuse_unmount(mountpoint, ch);
                }
            }
            fuse_opt_free_args(&args);

            delete_args(argc, fuse_argv);

            return err ? 1 : 0;
        }

        auto operations() { return &operations_; }

    private:

        static void load_operations_()
        {
            operations_.init           = T::init;
            operations_.destroy        = T::destroy;
            operations_.lookup         = T::lookup;
            operations_.forget         = T::forget;
            operations_.getattr        = T::getattr;
            operations_.setattr        = T::setattr;
            operations_.readlink       = T::readlink;
            operations_.mknod          = T::mknod;
            operations_.mkdir          = T::mkdir;
            operations_.unlink         = T::unlink;
            operations_.rmdir          = T::rmdir;
            operations_.symlink        = T::symlink;
            operations_.rename         = T::rename;
            operations_.link           = T::link;
            operations_.open           = T::open;
            operations_.read           = T::read;
            operations_.write          = T::write;
            operations_.flush          = T::flush;
            operations_.release        = T::release;
            operations_.fsync          = T::fsync;
            operations_.opendir        = T::opendir;
            operations_.readdir        = T::readdir;
            operations_.releasedir     = T::releasedir;
            operations_.fsyncdir       = T::fsyncdir;
            operations_.statfs         = T::statfs;
            operations_.setxattr       = T::setxattr;
            operations_.getxattr       = T::getxattr;
            operations_.listxattr      = T::listxattr;
            operations_.removexattr    = T::removexattr;
            operations_.access         = T::access;
            operations_.create         = T::create;
            operations_.getlk          = T::getlk;
            operations_.setlk          = T::setlk;
            operations_.bmap           = T::bmap;
            operations_.ioctl          = T::ioctl;
            operations_.poll           = T::poll;
            operations_.write_buf      = T::write_buf;
            operations_.retrieve_reply = T::retrieve_reply;
            operations_.forget_multi   = T::forget_multi;
            operations_.flock          = T::flock;
            operations_.fallocate      = T::fallocate;
        }

        static struct fuse_lowlevel_ops operations_;

        static t_init init;
        static t_destroy destroy;
        static t_lookup lookup;
        static t_forget forget;
        static t_getattr getattr;
        static t_setattr setattr;
        static t_readlink readlink;
        static t_mknod mknod;
        static t_mkdir mkdir;
        static t_unlink unlink;
        static t_rmdir rmdir;
        static t_symlink symlink;
        static t_rename rename;
        static t_link link;
        static t_open open;
        static t_read read;
        static t_write write;
        static t_flush flush;
        static t_release release;
        static t_fsync fsync;
        static t_opendir opendir;
        static t_readdir readdir;
        static t_releasedir releasedir;
        static t_fsyncdir fsyncdir;
        static t_statfs statfs;
        static t_setxattr setxattr;
        static t_getxattr getxattr;
        static t_listxattr listxattr;
        static t_removexattr removexattr;
        static t_access access;
        static t_create create;
        static t_getlk getlk;
        static t_setlk setlk;
        static t_bmap bmap;
        static t_ioctl ioctl;
        static t_poll poll;
        static t_write_buf write_buf;
        static t_retrieve_reply retrieve_reply;
        static t_forget_multi forget_multi;
        static t_flock flock;
        static t_fallocate fallocate;
        static t_readdirplus readdirplus;
        static t_copy_file_range copy_file_range;
        static t_lseek lseek;
    } ;
}

#endif //P2PFS_FUSE_H
