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
#include "lsfs_impl.h"
#include "metadata/metadata.h"


int lsfs_impl::_getattr(
    const char *path, struct stat *stbuf,
    struct fuse_file_info *fi
    )
{
    //std::cout << "### SysCall: _getattr  ===> Path: " << path << std::endl;

    int res = 0;

    if(!is_temp_file(path)){

        bool got_metadata = false;
        got_metadata = state->get_metadata_if_file_opened(path, stbuf);
        
        if(!got_metadata && state->use_cache){
            got_metadata = state->get_metadata_if_dir_cached(path, stbuf);
        }

        if(!got_metadata){
            try{
                std::unique_ptr<metadata_attr> met_attr = state->get_metadata_stat(path);
                if(met_attr == nullptr)
                    return -errno;

                // copy metadata received to struct stat
                memcpy(stbuf, &met_attr->stbuf, sizeof(struct stat));

                 
            } catch (EmptyViewException& e) {
                e.what();
                errno = EAGAIN; //resource unavailable
                return -errno;
            } catch(TimeoutException& e){
                e.what();
                errno = EHOSTUNREACH;
                return -errno;
            }
        }

    }else{
        res = fi ? fstat((int)fi->fh, stbuf) : lstat(path, stbuf);
    }

    return (res == 0) ? 0 : -errno;
}

int lsfs_impl::_chmod(
    const char *path, mode_t mode,
    struct fuse_file_info *fi
    )
{
    //std::cout << "### SysCall: _chmod" << std::endl;

    const int result = fi ? fchmod((int)fi->fh, mode) : chmod(path, mode);

    return (result == 0) ? 0 : -errno;
}

int lsfs_impl::_chown(
    const char *path, uid_t uid, gid_t gid,
    struct fuse_file_info *fi
    )
{
    //std::cout << "### SysCall: _chown" << std::endl;
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
    //std::cout << "### SysCall: _utimens" << std::endl;
    int result;

    if(!is_temp_file(path)) {

        bool updated = state->update_file_time_if_opened(path, ts);
        if(!updated) {
            // get file info
            struct stat stbuf;
            int res = lsfs_impl::_getattr(path, &stbuf, NULL);
            if (res != 0) {
                return -errno; 
            }

            stbuf.st_atim = ts[0];
            stbuf.st_mtim = ts[1];

            metadata to_send(stbuf);
            // serialize metadata object

            try{
                res = state->put_file_metadata(to_send, path);
                if (res == -1) 
                    return -errno;
                
            } catch (EmptyViewException& e) {
                e.what();
                errno = EAGAIN; //resource unavailable
                return -errno;
            } catch (ConcurrentWritesSameKeyException& e) {
                e.what();
                errno = EPERM; //operation not permitted
                return -errno;
            } catch(TimeoutException& e){
                e.what();
                errno = EHOSTUNREACH;
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
    //std::cout << "### SysCall: _truncate" << std::endl;

    int result;
    
    if(!is_temp_file(path)) {

        bool updated = state->update_file_size_if_opened(path, size);
        if(!updated){
            // get file info
            struct stat stbuf;
            int res = lsfs_impl::_getattr(path, &stbuf,NULL);
            if(res != 0){
                return -errno; 
            }

            int nr_b_blks = size / BLK_SIZE;
            size_t off_blk = size % BLK_SIZE;


            stbuf.st_size = size;
            stbuf.st_blocks = (off_blk != 0) ? (nr_b_blks + 1) : nr_b_blks;
            clock_gettime(CLOCK_REALTIME, &(stbuf.st_ctim));
            stbuf.st_mtim = stbuf.st_ctim;
            metadata to_send(stbuf);
            // serialize metadata object

            try{
                res = state->put_file_metadata(to_send, path);
                if(res == -1)
                    return -errno;
                
            } catch (EmptyViewException& e) {
                e.what();
                errno = EAGAIN; //resource unavailable
                return -errno;
            } catch (ConcurrentWritesSameKeyException& e) {
                e.what();
                errno = EPERM; //operation not permitted
                return -errno;
            } catch(TimeoutException& e){
                e.what();
                errno = EHOSTUNREACH;
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
    //std::cout << "### SysCall: _fallocate" << std::endl;

    (void)path;

    return (fallocate((int)fi->fh, mode, offset, length) == 0) ? 0 : -errno;
}
