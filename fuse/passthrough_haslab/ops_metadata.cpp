/* -------------------------------------------------------------------------- */

#define _ATFILE_SOURCE
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 500
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "util.h"
#include "../lsfs_impl.h"

/* -------------------------------------------------------------------------- */

int lsfs_impl::_getattr(
    const char *path, struct stat *stbuf,
    struct fuse_file_info *fi
    )
{
    if (strcmp(path, "/") != 0) {
        logger->info("GETATTR " + std::string(path));
        logger->flush();

        const int result = fi ? fstat((int)fi->fh, stbuf) : lstat(path, stbuf);

        return (result == 0) ? 0 : -errno;
    }

    const int result = fi ? fstat((int)fi->fh, stbuf) : lstat(path, stbuf);



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

    const int result = fi ? ftruncate((int)fi->fh, size) : truncate(path, size);

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
