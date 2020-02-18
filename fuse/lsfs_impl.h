//
// Created by danielsf97 on 2/4/20.
//

#ifndef P2PFS_LSFS_IMPL_H
#define P2PFS_LSFS_IMPL_H

#include <vector>
#include <queue>
#include "fuse_wrapper.h"
#include "fuse_wrapper.cpp"
#include "../df_client/client.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstddef>
#include <sys/statvfs.h>
#include <spdlog/logger.h>

extern std::mutex version_tracker_mutex;
extern std::unordered_map<std::string, long> version_tracker; // path => version
extern std::unique_ptr<client> df_client;
extern std::shared_ptr<spdlog::logger> logger;

long increment_version_and_get(std::string path);
long get_version(std::string path);
int open_and_read_size(const char *path, size_t* size);

class lsfs_impl : public fuse_wrapper::fuse<lsfs_impl>{

public:
    lsfs_impl();
    ~lsfs_impl() = default;

    /* ---------------------------- ops_createdelete.cpp ----------------------*/

    static int _mknod(
            const char *path, mode_t mode, dev_t rdev
    );

    static int _link(
            const char *from, const char *to
    );

    static int _unlink(
            const char *path
    );

    static int _rename(
            const char *from, const char *to, unsigned int flags
    );

    static int _mkdir(
            const char *path, mode_t mode
    );

    static int _rmdir(
            const char *path
    );

/* -------------------------------------------------------------------------- */

/* ------------------------------ ops_dir ----------------------------------- */

    static int _opendir(
            const char *path,
            struct fuse_file_info *fi
    );

    static int _releasedir(
            const char *path,
            struct fuse_file_info *fi
    );

    static int _fsyncdir(
            const char *path, int isdatasync,
            struct fuse_file_info *fi
    );

    static int _readdir(
            const char *path, void *buf, fuse_fill_dir_t filler,
            off_t offset, struct fuse_file_info *fi,
            enum fuse_readdir_flags flags
    );

/* -------------------------------------------------------------------------- */

/* -------------------------------- ops_file -------------------------------- */

    static int _create(
            const char *path, mode_t mode,
            struct fuse_file_info *fi
    );

    static int _open(
            const char *path,
            struct fuse_file_info *fi
    );

    static int _release(
            const char *path,
            struct fuse_file_info *fi
    );

    static int _fsync(
            const char *path, int isdatasync,
            struct fuse_file_info *fi
    );

    static int _read(
            const char *path, char *buf, size_t size, off_t offset,
            struct fuse_file_info *fi
    );

    static int _write(
            const char *path, const char *buf, size_t size, off_t offset,
            struct fuse_file_info *fi
    );

//    static int _read_buf(
//            const char *path, struct fuse_bufvec **bufp, size_t size, off_t offset,
//            struct fuse_file_info *fi
//    );
//
//    static int _write_buf(
//            const char *path, struct fuse_bufvec *buf, off_t offset,
//            struct fuse_file_info *fi
//    );

/* -------------------------------------------------------------------------- */

/* ---------------------------- ops_filesystem ------------------------------ */

    static int _statfs(
            const char *path, struct statvfs *stbuf
    );

/* -------------------------------------------------------------------------- */

/* ----------------------------- ops_initdestroy ----------------------------- */

    static void* _init(
            struct fuse_conn_info *conn,
            struct fuse_config *cfg
    );

    static void _destroy(
            void *private_data
    );

/* -------------------------------------------------------------------------- */

/* ------------------------------- ops_metadata ----------------------------- */

    static int _getattr(
            const char *path, struct stat *stbuf,
            struct fuse_file_info *fi
    );

    static int _chmod(
            const char *path, mode_t mode,
            struct fuse_file_info *fi
    );

    static int _chown(
            const char *path, uid_t uid, gid_t gid,
            struct fuse_file_info *fi
    );

    static int _utimens(
            const char *path, const struct timespec ts[2],
            struct fuse_file_info *fi
    );

    static int _truncate(
            const char *path, off_t size,
            struct fuse_file_info *fi
    );

    static int _fallocate(
            const char *path, int mode, off_t offset, off_t length,
            struct fuse_file_info *fi
    );

/* -------------------------------------------------------------------------- */

/* ------------------------------- ops_symlink ------------------------------ */

    static int _symlink(
            const char *from, const char *to
    );

    static int _readlink(
            const char *path, char *buf, size_t size
    );

/* -------------------------------------------------------------------------- */

/* ------------------------------ ops_xattr --------------------------------- */

//    static int _getxattr(
//            const char *path,
//            const char *name, char *value, size_t size
//    );
//
//    static int _setxattr(
//            const char *path,
//            const char *name, const char *value, size_t size,
//            int flags
//    );
//
//    static int _listxattr(
//            const char *path,
//            char *list, size_t size
//    );
//
//    static int _removexattr(
//            const char *path,
//            const char *name
//    );

/* -------------------------------------------------------------------------- */
};


#endif //P2PFS_LSFS_IMPL_H