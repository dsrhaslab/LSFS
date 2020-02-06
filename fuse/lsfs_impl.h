//
// Created by danielsf97 on 2/4/20.
//

#ifndef P2PFS_LSFS_IMPL_H
#define P2PFS_LSFS_IMPL_H

#include "fuse_ram_fs/inode.h"
#include <vector>
#include <queue>
#include "fuse.h"
#include "fuse.cpp"

class lsfs_impl : public fuse_wrapper::fuse<lsfs_impl>{

private:
    static const size_t kReadDirEntriesPerResponse = 255;
    static const size_t kReadDirBufSize = 384;
    static const size_t kInodeReclamationThreshold = 256;
    static const fsblkcnt_t kTotalBlocks = ~0;
    static const fsfilcnt_t kTotalInodes = ~0;
    static const unsigned long kFilesystemId = 0xc13f944870434d8f;
    static const size_t kMaxFilenameLength = 1024;

    static bool m_reclaimingInodes;
    static std::vector<Inode *> Inodes;
    static std::queue<fuse_ino_t> DeletedInodes;
    static struct statvfs m_stbuf;


private:
    static fuse_ino_t RegisterInode(Inode *inode_p, mode_t mode, nlink_t nlink, gid_t gid, uid_t uid);

public:
    lsfs_impl();
    ~lsfs_impl()= default;

//    void getattr (fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);

    static void init(void *userdata, struct fuse_conn_info *conn);
    static void destroy(void *userdata);
    static void lookup(fuse_req_t req, fuse_ino_t parent, const char *name);
    static void getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
    static void setattr(fuse_req_t req, fuse_ino_t ino, struct stat *attr, int to_set, struct fuse_file_info *fi);
    static void opendir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
    static void releasedir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
    static void fsyncdir(fuse_req_t req, fuse_ino_t ino, int datasync, struct fuse_file_info *fi);
    static void readdir(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi);
    static void open(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
    static void release(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
    static void fsync(fuse_req_t req, fuse_ino_t ino, int datasync, struct fuse_file_info *fi);
    static void mknod(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode, dev_t rdev);
    static void mkdir(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode);
    static void unlink(fuse_req_t req, fuse_ino_t parent, const char *name);
    static void rmdir(fuse_req_t req, fuse_ino_t parent, const char *name);
    static void forget(fuse_req_t req, fuse_ino_t ino, unsigned long nlookup);
    static void write(fuse_req_t req, fuse_ino_t ino, const char *buf, size_t size, off_t off, struct fuse_file_info *fi);
    static void flush(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
    static void read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi);
    static void rename(fuse_req_t req, fuse_ino_t parent, const char *name, fuse_ino_t newparent, const char *newname);
    static void link(fuse_req_t req, fuse_ino_t ino, fuse_ino_t newparent, const char *newname);
    static void symlink(fuse_req_t req, const char *link, fuse_ino_t parent, const char *name);
    static void readlink(fuse_req_t req, fuse_ino_t ino);
    static void statfs(fuse_req_t req, fuse_ino_t ino);
    static void setxattr(fuse_req_t req, fuse_ino_t ino, const char *name, const char *value, size_t size, int flags);
    static void getxattr(fuse_req_t req, fuse_ino_t ino, const char *name, size_t size);
    static void listxattr(fuse_req_t req, fuse_ino_t ino, size_t size);
    static void removexattr(fuse_req_t req, fuse_ino_t ino, const char *name);
    static void access(fuse_req_t req, fuse_ino_t ino, int mask);
    static void create(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode, struct fuse_file_info *fi);
    static void getlock(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi, struct flock *lock);

    static void UpdateUsedBlocks(ssize_t blocksAdded) { m_stbuf.f_bfree -= blocksAdded; m_stbuf.f_bavail -= blocksAdded;}
    static void UpdateUsedInodes(ssize_t inodesAdded) { m_stbuf.f_ffree -= inodesAdded; m_stbuf.f_favail -= inodesAdded;}
};


#endif //P2PFS_LSFS_IMPL_H
