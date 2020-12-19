/* -------------------------------------------------------------------------- */

#define _XOPEN_SOURCE 500
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "exceptions/custom_exceptions.h"

#include "util.h"
#include "lsfs/fuse_lsfs/lsfs_impl.h"
#include "metadata.h"

/* -------------------------------------------------------------------------- */

static int create_or_open(
        bool create,
        const char *path, mode_t mode,
        struct fuse_file_info *fi
)
{
    if (fi->flags & O_DIRECT)
    {
        // If O_DIRECT was specified, enable direct IO for the FUSE file system,
        // but ignore it when accessing the underlying file system. (If it was
        // not ignored, pread and pwrite could fail, as the buffer given by FUSE
        // may not be correctly aligned.)

        fi->flags &= ~O_DIRECT;
        fi->direct_io = 1;
    }

    int result;

    if (create)
        result = open(path, fi->flags, mode);
    else
        result = open(path, fi->flags);

    if (result == -1)
        return -errno;

    fi->fh = (uint64_t)result;

    return 0;
}

/* -------------------------------------------------------------------------- */

int lsfs_impl::_create(
        const char *path, mode_t mode,
        struct fuse_file_info *fi
)
{
    if (!fuse_pt_impersonate_calling_process_highlevel(&mode))
        return -errno;

    int return_value;

    if(!is_temp_file(path)) {
        const struct fuse_context* ctx = fuse_get_context();
        struct stat stbuf;
        // init file stat
        metadata::initialize_metadata(&stbuf, mode, 1, ctx->gid, ctx->uid);

        state->add_open_file(path, stbuf, FileAccess::CREATED);

        return_value = 0;
    }else{
        return_value = create_or_open(true, path, mode, fi);
    }

    fuse_pt_unimpersonate();

    return return_value;
}

int lsfs_impl::_open(
        const char *path,
        struct fuse_file_info *fi
)
{

    int return_value;

    if(!is_temp_file(path)) {
        try{
            if(!state->is_file_opened(path)){
                std::shared_ptr<metadata> met = state->get_metadata(path);
                if(met == nullptr){
                    return -errno;
                }else{
                    state->add_open_file(path, met->stbuf, FileAccess::ACCESSED);
                }
            }

            return_value = 0;

        }catch(TimeoutException &e){
            errno = EHOSTUNREACH;
            return_value = -errno;
        }

    }else{
        return_value = create_or_open(false, path, 0, fi);
    }

    return return_value;
}

int lsfs_impl::_flush(const char *path, struct fuse_file_info *fi){
    const int fd = (int)fi->fh;

    (void)path;

    if(!is_temp_file(path)) {
        return state->flush_open_file(path);
    }else{
        return 0;
    }
}

int lsfs_impl::_release(
        const char *path,
        struct fuse_file_info *fi
)
{
    const int fd = (int)fi->fh;

    (void)path;

    if(!is_temp_file(path)) {
        return state->flush_and_release_open_file(path);
    }else{
        return (close((int)fi->fh) == 0) ? 0 : -errno;
    }
}

int lsfs_impl::_fsync(
        const char *path, int isdatasync,
        struct fuse_file_info *fi
)
{
    const int fd = (int)fi->fh;

    (void)path;

    int result;

    if(!is_temp_file(path)) {
        return state->flush_open_file(path);
    }else{
        result = isdatasync ? fdatasync(fd) : fsync(fd);
    }

    return (result == 0) ? 0 : -errno;
}

int lsfs_impl::_read(
        const char *path, char *buf, size_t size, off_t offset,
        struct fuse_file_info *fi
)
{
    (void)path;

    size_t bytes_count;

    if (!is_temp_file(path)) {

        // get file info
        struct stat stbuf;
        int res = lsfs_impl::_getattr(path, &stbuf,NULL);
        if(res != 0){
            return -errno; //res = -errno
        }
        size_t file_size = stbuf.st_size;

        // Verify in which block the offset is placed
        // -> if in the middle of a written block
        // -> check block version and get it
        size_t nr_b_blks = offset / BLK_SIZE;
        size_t off_blk = offset % BLK_SIZE;
        bytes_count = 0;
        size_t current_blk = nr_b_blks - 1;

        try {
            // align read with block size
            if (off_blk != 0) {
                //middle of block -> i doubt ever happen
                size_t off_ini_blk = nr_b_blks * BLK_SIZE;
                char ini_buf [BLK_SIZE];
                int bytes_read = lsfs_impl::_read(path, ini_buf, BLK_SIZE, off_ini_blk, NULL);
                if (bytes_read < 0) return -errno;
                else {
                    size_t ini_size = (size <= bytes_read)? size : std::min((int)(BLK_SIZE - off_blk), bytes_read);
                    memcpy(&buf[bytes_count], &ini_buf[off_blk], ini_size * sizeof(char));
                    bytes_count += ini_size;
                }

            }

            // What's left to read is the minimum between what I'm supposed to read
            // and the file size from the point I'm reading
            size_t missing_read_size = std::min((size - bytes_count),(file_size - offset - bytes_count));
            if(missing_read_size > 0){
                bytes_count += state->read_fixed_size_blocks_to_buffer_limited_paralelization(&buf[bytes_count], missing_read_size, BLK_SIZE, path, current_blk);
            }
        }catch (EmptyViewException& e) {
            // empty view -> nothing to do
            e.what();
            errno = EAGAIN; //resource unavailable
            return -errno;
        }catch(TimeoutException& e){
            errno = EHOSTUNREACH; // Not Reachable Host
            return -errno;
        }
    }else{
        bytes_count = pread((int)fi->fh, buf, size, offset);
    }

    if (bytes_count == -1)
        return -errno;

    return bytes_count;
}

int lsfs_impl::_write(
        const char *path, const char *buf, size_t size, off_t offset,
        struct fuse_file_info *fi
)
{
    (void)path;
    int result;

    if (!is_temp_file(path)) {

        //TODO fazer caching com stack de getattr's

        // get file info
        struct stat stbuf;

        int res = lsfs_impl::_getattr(path, &stbuf,NULL);
        if(res != 0){
            return -errno; //res = -errno
        }

        size_t current_size = stbuf.st_size;
        size_t next_size = current_size > (offset + size)? current_size : (offset + size);

        // Verify in which block the offset is placed
        // -> if in the middle of a written block
        // -> check block version and get it
        size_t nr_b_blks = offset / BLK_SIZE;
        size_t off_blk = offset % BLK_SIZE;
        size_t read_off = 0;
        size_t current_blk = nr_b_blks - 1;

        char put_buf [BLK_SIZE];

        try {
            if (off_blk != 0) {
                //middle of block -> i doubt ever happen
                size_t off_ini_blk = nr_b_blks * BLK_SIZE;
                if (offset <= current_size) {
                    // read current block from the start (off_ini_blk)
                    size_t bytes_read = lsfs_impl::_read(path, put_buf, BLK_SIZE, off_ini_blk, NULL);
                    if (bytes_read < 0) return -errno;
                    else {
                        //fill the rest of the block (from off_blk)
                        size_t first_block_size = (size > BLK_SIZE)? BLK_SIZE : size;
                        memcpy(&put_buf[off_blk], &buf[read_off], (first_block_size - off_blk) * sizeof(char));
                        read_off += (first_block_size - off_blk);
                        current_blk++;
                        std::string blk_path;
                        blk_path.reserve(100);
                        blk_path.append(path).append(":").append(std::to_string(current_blk));
                        int res = state->put_block(blk_path, buf, size, true);
                        if(res == -1){
                            return -errno;
                        }
                    }
                } else {
                    errno = EPERM;
                    return -errno;
                }
            }

            res = state->put_fixed_size_blocks_from_buffer_limited_paralelization(&buf[read_off], size-read_off, BLK_SIZE, path, current_blk, true);
            if(res == -1){
                return -errno;
            }else{
                current_blk += (size / BLK_SIZE) + (size % BLK_SIZE == 0 ? 0 : 1);
            }

            stbuf.st_size = next_size;
            stbuf.st_blocks = current_blk + 1;
            clock_gettime(CLOCK_REALTIME, &(stbuf.st_ctim));
            stbuf.st_mtim = stbuf.st_ctim;

            state->update_open_file_metadata(path, stbuf);

            result = size;
        } catch (EmptyViewException& e) {
            // empty view -> nothing to do
            e.what();
            errno = EAGAIN; //resource unavailable
            return -errno;
        } catch (ConcurrentWritesSameKeyException& e) {
            // empty view -> nothing to do
            e.what();
            errno = EPERM; //operation not permitted
            return -errno;
        } catch(TimeoutException& e){
            errno = EHOSTUNREACH;
            return -errno;
        }
    }else{
        result = pwrite((int) fi->fh, buf, size, offset);
    }

    if (result == -1)
        return -errno;

    return result;
}

/* -------------------------------------------------------------------------- */