/* -------------------------------------------------------------------------- */

#define _ATFILE_SOURCE
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 500
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "exceptions/custom_exceptions.h"

#include "util.h"
#include "lsfs/fuse_lsfs/lsfs_impl.h"
#include "metadata.h"

/* -------------------------------------------------------------------------- */

int lsfs_impl::_getattr(
    const char *path, struct stat *stbuf,
    struct fuse_file_info *fi
    )
{
    std::cout << "### SysCall: _getattr" << std::endl;

    int result;

    if(!is_temp_file(path)){

        bool got_metadata = false;
        got_metadata = state->get_metadata_if_file_opened(path, stbuf);
        if(!got_metadata){
            got_metadata = state->get_metadata_if_dir_opened(path, stbuf);
        }
        if(!got_metadata){
            std::unique_ptr<metadata> res = state->get_metadata(path);
            if(res == nullptr){
                return -errno;
            }

            // copy metadata received to struct stat
            memcpy(stbuf, &res->stbuf, sizeof(struct stat));
        }

        result = 0;
    }else{
        result = fi ? fstat((int)fi->fh, stbuf) : lstat(path, stbuf);
    }

    return (result == 0) ? 0 : -errno;
}

int lsfs_impl::_chmod(
    const char *path, mode_t mode,
    struct fuse_file_info *fi
    )
{
    std::cout << "### SysCall: _chmod" << std::endl;

    const int result = fi ? fchmod((int)fi->fh, mode) : chmod(path, mode);

    return (result == 0) ? 0 : -errno;
}

int lsfs_impl::_chown(
    const char *path, uid_t uid, gid_t gid,
    struct fuse_file_info *fi
    )
{
    std::cout << "### SysCall: _chown" << std::endl;
    const int result =
        fi ?
        fchown((int)fi->fh, uid, gid) :
        lchown(path, uid, gid);

    return (result == 0) ? 0 : -errno;
}

int lsfs_impl::_utimens(
    const char *path, const struct timespec ts[2],
    struct fuse_file_info *fi
    )
{
    std::cout << "### SysCall: _utimens" << std::endl;
    int result;

    if(!is_temp_file(path)) {

        bool updated = state->update_file_time_if_opened(path, ts);
        if(!updated) {
            // get file info
            struct stat stbuf;
            int res = lsfs_impl::_getattr(path, &stbuf, NULL);
            if (res != 0) {
                return -errno; //res = -errno
            }

            stbuf.st_atim = ts[0];
            stbuf.st_mtim = ts[1];

            metadata to_send(stbuf);
            // serialize metadata object

            bool is_dir = S_ISDIR(stbuf.st_mode);
            res = state->put_metadata(to_send, path, (is_dir == false));
            if (res == -1) {
                return -errno;
            }
        }

        result = 0;
    }else{
        if (fi)
            result = futimens((int)fi->fh, ts);
        else
            result = utimensat(AT_FDCWD, path, ts, AT_SYMLINK_NOFOLLOW);
    }

    return (result == 0) ? 0 : -errno;
}

int lsfs_impl::_truncate(
    const char *path, off_t size,
    struct fuse_file_info *fi
    )
{
    std::cout << "### SysCall: _truncate" << std::endl;

    int result;
    
    if(!is_temp_file(path)) {

        bool updated = state->update_file_size_if_opened(path, size);
        if(!updated){
            // get file info
            struct stat stbuf;
            int res = lsfs_impl::_getattr(path, &stbuf,NULL);
            if(res != 0){
                return -errno; //res = -errno
            }

            int nr_b_blks = size / BLK_SIZE;
            size_t off_blk = size % BLK_SIZE;


            stbuf.st_size = size;
            stbuf.st_blocks = (off_blk != 0) ? (nr_b_blks + 1) : nr_b_blks;
            clock_gettime(CLOCK_REALTIME, &(stbuf.st_ctim));
            stbuf.st_mtim = stbuf.st_ctim;
            metadata to_send(stbuf);
            // serialize metadata object
            res = state->put_metadata(to_send, path, true);
            if(res == -1){
                return -errno;
            } 
        }

        result = 0;
    }else{
        result = fi ? ftruncate((int)fi->fh, size) : truncate(path, size);
    }

    return (result == 0) ? 0 : -errno;
}

int lsfs_impl::_fallocate(
    const char *path, int mode, off_t offset, off_t length,
    struct fuse_file_info *fi
    )
{
    std::cout << "### SysCall: _fallocate" << std::endl;

    (void)path;

    return (fallocate((int)fi->fh, mode, offset, length) == 0) ? 0 : -errno;
}

/* -------------------------------------------------------------------------- */
