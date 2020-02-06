//
// Created by danielsf97 on 2/5/20.
//

#ifndef P2PFS_DIRECTORY_H
#define P2PFS_DIRECTORY_H

class Directory : public Inode {
private:
    std::map<std::string, fuse_ino_t> m_children;
public:
    ~Directory() {}

    fuse_ino_t ChildInodeNumberWithName(const std::string &name);
    fuse_ino_t UpdateChild(const std::string &name, fuse_ino_t ino);
    int WriteAndReply(fuse_req_t req, const char *buf, size_t size, off_t off);
    int ReadAndReply(fuse_req_t req, size_t size, off_t off);

    const std::map<std::string, fuse_ino_t> &Children() { return m_children; }
};

#endif //P2PFS_DIRECTORY_H
