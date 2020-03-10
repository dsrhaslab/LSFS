/* -------------------------------------------------------------------------- */

#define _ATFILE_SOURCE
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 500
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <exceptions/custom_exceptions.h>

#include "util.h"
#include "../lsfs_impl.h"
#include "metadata.h"

/* -------------------------------------------------------------------------- */

int lsfs_impl::_getattr(
    const char *path, struct stat *stbuf,
    struct fuse_file_info *fi
    )
{
    int result;
    if (strcmp(path, "/") != 0) {
        logger->info("GETATTR " + std::string(path));
        logger->flush();

        if(!is_temp_file(path)){
            std::unique_ptr<metadata> res = get_metadata(path);
            if(res == nullptr){
                return -errno;
            }

            // copy metadata received to struct stat
            memcpy(stbuf, &res->stbuf, sizeof(struct stat));

            result = 0;

            logger->info("GETATTR - Não é temporário " + std::to_string(stbuf->st_size));
            logger->flush();
        }
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
    if (path){
        logger->info("CHMOD " + std::string(path));
        logger->flush();
    }else{
        logger->info("CHMOD");
        logger->flush();
    }

    const int result = fi ? fchmod((int)fi->fh, mode) : chmod(path, mode);

    return (result == 0) ? 0 : -errno;
}

int lsfs_impl::_chown(
    const char *path, uid_t uid, gid_t gid,
    struct fuse_file_info *fi
    )
{
    if (path){
        logger->info("CHOWN " + std::string(path));
        logger->flush();
    }else{
        logger->info("CHOWN");
        logger->flush();
    }

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
    if (path){
        logger->info("UTIMENS " + std::string(path));
        logger->flush();
    }else{
        logger->info("UTIMENS");
        logger->flush();
    }

    int result;

    if (fi)
        result = futimens((int)fi->fh, ts);
    else
        result = utimensat(AT_FDCWD, path, ts, AT_SYMLINK_NOFOLLOW);

    return (result == 0) ? 0 : -errno;
}

int lsfs_impl::_truncate(
    const char *path, off_t size,
    struct fuse_file_info *fi
    )
{
    if (path){
        logger->info("TRUNCATE " + std::string(path) + " SIZE" + std::to_string(size));
        logger->flush();
    }else{
        logger->info("TRUNCATE SIZE" + std::to_string(size));
        logger->flush();
    }

    int result;
    
    if(!is_temp_file(path)) {
        
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
        std::string metadata_str = metadata::serialize_to_string(to_send);
        //dataflasks send
        try{
            long version = df_client->get_latest_version(path);
            df_client->put(path, version + 1, metadata_str.data(), metadata_str.size());
            result = 0;
        }catch(EmptyViewException& e){
            // empty view -> nothing to do
            e.what();
            errno = EAGAIN; //resource unavailable
            result = -errno;
        }catch(ConcurrentWritesSameKeyException& e){
            e.what();
            errno = EPERM; //operation not permitted
            result = -errno;
        }catch(TimeoutException& e){
            e.what();
            errno = EHOSTUNREACH; //host not reachable
            result = -errno;
        }
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
    if (path){
        logger->info("FALLOCATE " + std::string(path) + " LENGTH:" + std::to_string(length) + " OFFSET:" + std::to_string(offset));
        logger->flush();
    }else{
        logger->info("FALLOCATE LENGTH:" + std::to_string(length) + " OFFSET:" + std::to_string(offset));
        logger->flush();
    }

    (void)path;

    return (fallocate((int)fi->fh, mode, offset, length) == 0) ? 0 : -errno;
}

/* -------------------------------------------------------------------------- */
