//
// Created by danielsf97 on 2/5/20.
//

#ifndef P2PFS_FILE_H
#define P2PFS_FILE_H


#include <fuse/fuse_lowlevel.h>
#include "inode.h"

class File : public Inode {
private:
    void *m_buf;

public:
    File() :
            m_buf(NULL) {}

    ~File();

    int WriteAndReply(fuse_req_t req, const char *buf, size_t size, off_t off);
    int ReadAndReply(fuse_req_t req, size_t size, off_t off);

//    size_t Size();
};


#endif //P2PFS_FILE_H
