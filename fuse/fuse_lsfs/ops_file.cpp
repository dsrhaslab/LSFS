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
#include "fuse/fuse_lsfs/lsfs_impl.h"
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

//        // create metadata object
//        metadata to_send(stbuf);
//        // serialize metadata object
//        int res = put_metadata(to_send, path);
//        if(res == -1){
//            return -errno;
//        }
//        res = add_child_to_parent_dir(path, false);
//        if(res == -1){
//            return -errno;
//        }

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
//        long version = get_version(path);
        try{

//            long version = df_client->get_latest_version(path);
//            if(version == -1){
//                errno = ENOENT;
//                return_value = -errno;
//            }
//            else{
//                return_value = 0;
//            }

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
//        struct stat stbuf;
//        result = lsfs_impl::_getattr(path, &stbuf,NULL);
//        if(result != 0){
//            return -errno; //res = -errno
//        }
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

        //Verificar em que bloco se posiciona o offset
        // -> caso coincida no meio de um bloco escrito
        // -> ver a versão desse bloco e ir busca-lo
        size_t nr_b_blks = offset / BLK_SIZE;
        size_t off_blk = offset % BLK_SIZE;
        bytes_count = 0;
        size_t current_blk = nr_b_blks - 1;

        try {
            // alinhar leitura de acordo com o block size
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

            // O que falta ler é o mínimo entre o que supostamente me falta ler
            // e o tamanho do ficheiro a partir do ponto que me encontro a ler
            size_t missing_read_size = std::min((size - bytes_count),(file_size - offset - bytes_count));
            if(missing_read_size > 0){
                bytes_count += state->read_fixed_size_blocks_to_buffer_limited_paralelization(&buf[bytes_count], missing_read_size, BLK_SIZE, path, current_blk);
            }

//            while (bytes_count < size && bytes_count < (file_size - offset)) {
//                current_blk++;
//                std::string blk_path;
//                blk_path.reserve(100);
//                blk_path.append(path).append(":").append(std::to_string(current_blk));
////                long version = get_version(blk_path);
////                if (version == -1){
////                    errno = ENOENT;
////                    return -errno;
////                }
//                std::shared_ptr<std::string> data = df_client->get(blk_path /*, &version*/);
//
//                size_t max_read = (bytes_count + BLK_SIZE) > size ? (size - bytes_count) : BLK_SIZE;
//                size_t actually_read = (file_size - offset) > max_read ? max_read : (file_size - offset);
//
//                data->copy(&buf[bytes_count], actually_read);
//                bytes_count += actually_read;
//            }

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

        //Verificar em que bloco se posiciona o offset
        // -> caso coincida no meio de um bloco escrito
        // -> ver a versão desse bloco e ir busca-lo
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
                        //preencher o resto do bloco (a partir do off_blk)
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

//            while (read_off < size) {
//                size_t write_size = (read_off + BLK_SIZE) > size ? (size - read_off) : BLK_SIZE;
//                current_blk++;
//                std::string blk_path;
//                blk_path.reserve(100);
//                blk_path.append(path).append(":").append(std::to_string(current_blk));
//                int res = state->put_block(blk_path, &buf[read_off], write_size, true);
//                if(res == -1){
//                    return -errno;
//                }
//                read_off += BLK_SIZE;
//            }

            //TODO SET NEW GETATTR
            stbuf.st_size = next_size;
            stbuf.st_blocks = current_blk + 1;
            clock_gettime(CLOCK_REALTIME, &(stbuf.st_ctim));
            stbuf.st_mtim = stbuf.st_ctim;
//            metadata to_send(stbuf);
//            // serialize metadata object
//            std::string metadata_str = metadata::serialize_to_string(to_send);
//            //dataflasks send
//            long version = df_client->get_latest_version(path);
//            df_client->put(path, version + 1, metadata_str.data(), metadata_str.size());

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

//int lsfs_impl::_read_buf(
//    const char *path, struct fuse_bufvec **bufp, size_t size, off_t offset,
//    struct fuse_file_info *fi
//    )
//{
//
//    (void)path;
//
//    struct fuse_bufvec *const src = static_cast<fuse_bufvec *const>(malloc(sizeof(struct fuse_bufvec)));
//
//    if (!src)
//        return -ENOMEM;
//
//    *src = FUSE_BUFVEC_INIT(size);
//
//    src->buf[0].flags = static_cast<fuse_buf_flags>(FUSE_BUF_IS_FD | FUSE_BUF_FD_SEEK);
//    src->buf[0].fd    = (int)fi->fh;
//    src->buf[0].pos   = offset;
//
//    *bufp = src;
//
//    return 0;
//}

/*
 * Write contents of buffer to an open file
 * Similar to the write() method, but data is supplied in a generic buffer. Use fuse_buf_copy() to transfer data to the destination.
 * Unless FUSE_CAP_HANDLE_KILLPRIV is disabled, this method is expected to reset the setuid and setgid bits.
 * (o bufvec tem o size e tem o file descriptor onde estão os dados)
*/
//int lsfs_impl::_write_buf(
//    const char *path, struct fuse_bufvec *buf, off_t offset,
//    struct fuse_file_info *fi
//    )
//{
//    (void)path;
//
//    const int fd = (int)fi->fh;
//
//    struct fuse_bufvec dst = FUSE_BUFVEC_INIT(fuse_buf_size(buf));
//
//    { //Treat the case when is not a temporary file
//        std::scoped_lock<std::mutex> lk(fhs_mutex);
//        auto it = file_handlers.find(fd);
//        if(it != file_handlers.end()){
//            std::string file_path = it->second;
//
//            if(!is_temp_file(file_path)){
//
//                dst.buf[0].flags = static_cast<fuse_buf_flags>(/*FUSE_BUF_IS_FD |*/ FUSE_BUF_FD_SEEK);
//                dst.buf[0].mem   = (char*) malloc(sizeof(char) * buf->buf[0].size);
//                dst.buf[0].pos   = offset;
//
//                auto return_value = fuse_buf_copy(&dst, buf, FUSE_BUF_SPLICE_NONBLOCK);
//
//                //dataflasks send
//                df_client->put(1, 1, static_cast<const char *>(dst.buf[0].mem));
//
//                //free memory
//                free(dst.buf[0].mem);
//
//                return return_value;
//            }
//        }
//    }
//
//    // Treat every other case (PassThrough)
//
//    dst.buf[0].flags = static_cast<fuse_buf_flags>(FUSE_BUF_IS_FD | FUSE_BUF_FD_SEEK);
//    dst.buf[0].fd    = fd;
//    dst.buf[0].pos   = offset;
//
//    auto return_value = fuse_buf_copy(&dst, buf, FUSE_BUF_SPLICE_NONBLOCK);
//
//    return return_value;
//}

/* -------------------------------------------------------------------------- */
