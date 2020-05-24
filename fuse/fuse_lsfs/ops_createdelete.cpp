/* -------------------------------------------------------------------------- */

#define _XOPEN_SOURCE 500
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "util.h"
#include "fuse/fuse_lsfs/lsfs_impl.h"
#include "metadata.h"

/* -------------------------------------------------------------------------- */

int lsfs_impl::_mknod(
    const char *path, mode_t mode, dev_t rdev
    )
{
//    logger->info("MKNOD" + std::string(path));
//    logger->flush();

    if (!fuse_pt_impersonate_calling_process_highlevel(&mode))
        return -errno;

    const int return_value = (mknod(path, mode, rdev) == 0) ? 0 : -errno;

    fuse_pt_unimpersonate();

    return return_value;
}

int lsfs_impl::_link(
    const char *from, const char *to
    )
{
//    logger->info("LINK From:" + std::string(from) + " To:" + std::string(to));
//    logger->flush();

    return (link(from, to) == 0) ? 0 : -errno;
}

int lsfs_impl::_unlink(
    const char *path
    )
{
//    logger->info("UNLINK " + std::string(path));
//    logger->flush();

    return (unlink(path) == 0) ? 0 : -errno;
}

int lsfs_impl::_rename(
    const char *from, const char *to, unsigned int flags
    )
{
//    logger->info("RENAME From:" + std::string(from) + " To:" + std::string(to));
//    logger->flush();

    if (flags != 0)
        return -EINVAL;

    return (rename(from, to) == 0) ? 0 : -errno;
}

/*
 *
 * Historically, the first Unix filesystem created two entries in every
 * directory: . pointing to the directory itself, and .. pointing to its parent.
 * This provided an easy way to traverse the filesystem, both for applications
 * and for the OS itself.
 *
 * Thus each directory has a link count of 2+n where n is the number of subdirectories.
 * The links are the entry for that directory in its parent, the directory's 
 * own . entry, and the .. entry in each subdirectory. For example, suppose this is
 * the content of the subtree rooted at /parent, all directories:
 *
 * /parent
 * /parent/dir
 * /parent/dir/sub1
 * /parent/dir/sub2
 * /parent/dir/sub3
 *
 * Then dir has a link count of 5: the dir entry in /parent, the . entry in /parent/dir,
 * and the three .. entries in each of /parent/dir/sub1, /parent/dir/sub2 and /parent/dir/sub3.
 * Since /parent/dir/sub1 has no subdirectory, its link count is 2 (the sub1 entry in
 * /parent/dir and the . entry in /parent/dir/sub1).
 *
 * To minimize the amount of special-casing for the root directory, which doesn't have a
 * “proper” parent, the root directory contains a .. entry pointing to itself. This way it,
 * too, has a link count of 2 plus the number of subdirectories, the 2 being /. and /...
 *
 * Later filesystems have tended to keep track of parent directories in memory and usually
 * don't need . and .. to exist as actual entries; typical modern unix systems treat . and ..
 * as special values as part of the filesystem-type-independent filesystem code. Some
 * filesystems still include . and .. entries, or pretend to even though nothing appears on the disk.
 *
 * Most filesystems still report a link count of 2+n for directories regardless of
 * whether . and .. entries exist, but there are exceptions, for example btrfs doesn't do this.
 *
 * */

int lsfs_impl::_mkdir(
    const char *path, mode_t mode
    )
{
//    logger->info("MKDIR " + std::string(path));
//    logger->flush();
    
    if (!fuse_pt_impersonate_calling_process_highlevel(&mode))
        return -errno;

    const struct fuse_context* ctx = fuse_get_context();
    struct stat stbuf;
    // init file stat
    nlink_t n_link = (strcmp(path, "/") == 0) ? 1 : 2;
    metadata::initialize_metadata(&stbuf, S_IFDIR | mode, n_link, ctx->gid, ctx->uid);
    // create metadata object
    metadata to_send(stbuf);
    // put metadata
    int res = state->put_metadata(to_send, path);
    if(res == -1){
        return -errno;
    }
    res = state->add_child_to_parent_dir(path, true);
    if(res != 0){
        return res;
    }

    state->add_or_refresh_working_directory(path, to_send);

//    const int return_value = (mkdir(path, mode) == 0) ? 0 : -errno;

    fuse_pt_unimpersonate();

    return 0;
}

int lsfs_impl::_rmdir(
    const char *path
    )
{
//    logger->info("RMDIR " + std::string(path));
//    logger->flush();

    return (rmdir(path) == 0) ? 0 : -errno;
}

/* -------------------------------------------------------------------------- */
