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

#include "util.h"
#include "../lsfs_impl.h"

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

    const int return_value = create_or_open(true, path, mode, fi);

    const int fd = (int)fi->fh;

    if (path){
        logger->info("CREATE " + std::string(path) + " FD:" + std::to_string(fd));
        logger->flush();
    }else{
        logger->info("CREATE FD:" + std::to_string(fd));
        logger->flush();
    }

    fuse_pt_unimpersonate();

    return return_value;
}

int lsfs_impl::_open(
    const char *path,
    struct fuse_file_info *fi
    )
{
    const int return_value = create_or_open(false, path, 0, fi);

    const int fd = (int)fi->fh;

    if (path){
        logger->info("OPEN " + std::string(path) + " FD:" + std::to_string(fd));
        logger->flush();
    }else{
        logger->info("OPEN FD:" + std::to_string(fd));
        logger->flush();
    }

    return return_value;

    return 0;
}

int lsfs_impl::_release(
    const char *path,
    struct fuse_file_info *fi
    )
{
    const int fd = (int)fi->fh;

    if (path){
        logger->info("RELEASE " + std::string(path) + " FD:" + std::to_string(fd));
        logger->flush();
    }else{
        logger->info("RELEASE FD:" + std::to_string(fd));
        logger->flush();
    }

    (void)path;

    return (close((int)fi->fh) == 0) ? 0 : -errno;
}

int lsfs_impl::_fsync(
    const char *path, int isdatasync,
    struct fuse_file_info *fi
    )
{
    const int fd = (int)fi->fh;

    if (path){
        logger->info("FSYNC " + std::string(path));
        logger->flush();
    }else{
        logger->info("FSYNC " + std::to_string(fd));
        logger->flush();
    }

    (void)path;

    const int result = isdatasync ? fdatasync(fd) : fsync(fd);

    return (result == 0) ? 0 : -errno;
}

int lsfs_impl::_read(
    const char *path, char *buf, size_t size, off_t offset,
    struct fuse_file_info *fi
    )
{
    if (path){
        logger->info("READ " + std::string(path) + " SIZE:" + std::to_string(size) + " OFFSET" + std::to_string(offset));
        logger->flush();
    }else{
        logger->info("READ SIZE:" + std::to_string(size) + " OFFSET" + std::to_string(offset));
        logger->flush();
    }

    (void)path;


    size_t actual_size = size;
    if(!is_temp_file(path)){
        logger->info("READ - Não é temporário");
        logger->flush();

        const int result_non_temp = open_and_read_size(path, &actual_size);

        if(result_non_temp == 0){
            //dataflasks get
            long version = get_version(path);
            std::shared_ptr<std::string> data = df_client->get(1, path, version);
            data->copy(buf, actual_size);
            return actual_size;
        }
    }


//    if(!is_temp_file(path)){
//        logger->info("READ - Não é temporário");
//        logger->flush();
//
//        //dataflasks get
//        long version = get_version(path);
//        std::shared_ptr<const char[]> data = df_client->get(1, path, version);
//        strncpy(buf, data.get(), size);
//        return strlen(buf);
//
//    }else{
        const int result = pread((int)fi->fh, buf, size, offset);

        if (result == -1)
            return -errno;

        return result;
//    }
}

int lsfs_impl::_write(
    const char *path, const char *buf, size_t size, off_t offset,
    struct fuse_file_info *fi
    )
{
    if (path){
        logger->info("WRITE " + std::string(path) + " SIZE:" + std::to_string(size) + " OFFSET" + std::to_string(offset) + " BUF:" + buf);
        logger->flush();
    }else{
        logger->info("WRITE SIZE:" + std::to_string(size) + " OFFSET" + std::to_string(offset));
        logger->flush();
    }

    (void)path;

    char** write_buf = const_cast<char **>(&buf);

    char size_attr [20];
    char* size_ptr = size_attr;
    sprintf(size_attr, "%lu", size);
    int result;
    try {
        if (!is_temp_file(path)) {
            logger->info("WRITE - Não é temporário");
            logger->flush();

            //dataflasks send
            long version = increment_version_and_get(path);
            df_client->put(path, version, buf, size);
            write_buf = &size_ptr;
            pwrite((int) fi->fh, *write_buf, strlen(size_ptr) + 1, offset);
            result = size; // tem sempre de se retornar o size suposto senão eram realizados mais pedidos
        } else {
            result = pwrite((int) fi->fh, *write_buf, size, offset);
        }
    }catch(const char* msg){
        // empty view -> nothing to do
        errno = EAGAIN; //resource unavailable
        return -errno;
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
//    if (path){
//        logger->info("READBUF " + std::string(path) + " SIZE:" + std::to_string(size) + " OFFSET" + std::to_string(offset));
//        logger->flush();
//    }else{
//        logger->info("READBUF SIZE:" + std::to_string(size) + " OFFSET" + std::to_string(offset));
//        logger->flush();
//    }
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
//            logger->info("WRITEBUF " + file_path + " OFFSET " + std::to_string(offset));
//            logger->flush();
//
//            if(!is_temp_file(file_path)){
//                logger->info("WRITEBUF - Não é temporário");
//                logger->flush();
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
//    logger->info("WRITEBUF OFFSET " + std::to_string(offset) + " FH: " + std::to_string(fd));
//    logger->flush();
//
//    return return_value;
//}

/* -------------------------------------------------------------------------- */
