/* -------------------------------------------------------------------------- */

#define _XOPEN_SOURCE 500
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "util.h"
#include "lsfs/fuse_lsfs/lsfs_impl.h"
#include "metadata/metadata.h"

/* -------------------------------------------------------------------------- */

int lsfs_impl::_mknod(
    const char *path, mode_t mode, dev_t rdev
    )
{
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
    return (link(from, to) == 0) ? 0 : -errno;
}

int lsfs_impl::_unlink(
    const char *path
    )
{
    //std::cout << "### SysCall: _unlink  ==> Path: " << path  << std::endl;

    int res = 0;

    try{
        res = state->delete_file_or_dir(path);
        if(res != 0) return -errno;

        res = state->remove_child_from_parent_dir(path, false);

    }catch(EmptyViewException& e){
        e.what();
        errno = EAGAIN; //resource unavailable
        return -errno;
    }catch(ConcurrentWritesSameKeyException& e){
        e.what();
        errno = EPERM; //operation not permitted
        return -errno;
    }catch(TimeoutException& e){
        e.what();
        errno = EHOSTUNREACH;
        return -errno;
    }
    
    return res == 0? 0 : -errno;
}

int lsfs_impl::_rename(
    const char *from, const char *to, unsigned int flags
    )
{
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
    //std::cout << "### SysCall: _mkdir  ==> Path: " << path << std::endl;
    

    if (!fuse_pt_impersonate_calling_process_highlevel(&mode))
        return -errno;

    const struct fuse_context* ctx = fuse_get_context();
    struct stat stbuf;
    // init file stat
    nlink_t n_link = (strcmp(path, "/") == 0) ? 1 : 2;
    metadata_attr::initialize_metadata(&stbuf, S_IFDIR | mode, n_link, ctx->gid, ctx->uid);
    // create metadata object
    metadata_attr met_attr(stbuf);
    metadata to_send(met_attr);

    int res = 0;

    try{

        //if(((std::string)path).find("/testF/") == std::string::npos)
        res = state->put_dir_metadata(to_send, path);

        res = state->add_child_to_parent_dir(path, true);

        if(state->use_cache) state->add_to_dir_cache(path, to_send);

    }catch(EmptyViewException& e){
        e.what();
        errno = EAGAIN; //resource unavailable  
        res = -1;
    }catch(ConcurrentWritesSameKeyException& e){
        e.what();
        errno = EPERM; //operation not permitted
        res = -1;
    }catch(TimeoutException& e){
        e.what();
        errno = EHOSTUNREACH;
        res = -1;
    }

    fuse_pt_unimpersonate();

    return res == 0? 0 : -errno;
}

int lsfs_impl::_rmdir(
    const char *path
    )
{
    //std::cout << "### SysCall: _rmdir  ==> Path: " << path << std::endl;

    int res = 0;

    try{

        bool is_cached = false;

        if(state->use_cache){
            bool is_empty = state->is_dir_childs_empty(path, &is_cached);
            if(!is_empty){
                errno = ENOTEMPTY;
                return -errno;
            }
        }

        if(!is_cached){
            auto met = state->get_dir_metadata(path);
            if(met == nullptr)
                return -errno;
            
            if(!met->childs.is_empty()){
                errno = ENOTEMPTY;
                return -errno;
            }
        }

        res = state->delete_file_or_dir(path);
        if(res != 0) return -errno;

        if(state->use_cache) state->remove_from_dir_cache(path);

        res = state->remove_child_from_parent_dir(path, true);

    }catch(EmptyViewException& e){
        e.what();
        errno = EAGAIN; //resource unavailable  
        return -errno;
    }catch(ConcurrentWritesSameKeyException& e){
        e.what();
        errno = EPERM; //operation not permitted
        return -errno;
    }catch(TimeoutException& e){
        e.what();
        errno = EHOSTUNREACH;
        return -errno;
    }

    return res == 0? 0 : -errno;
}

/* -------------------------------------------------------------------------- */
