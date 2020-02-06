//
// Created by danielsf97 on 2/5/20.
//

#ifndef P2PFS_SYMLINK_H
#define P2PFS_SYMLINK_H


class SymLink : public Inode {
private:
    std::string m_link;

public:
    SymLink(const std::string &link) :
            m_link(link) {}

    ~SymLink() {};

    int WriteAndReply(fuse_req_t req, const char *buf, size_t size, off_t off);
    int ReadAndReply(fuse_req_t req, size_t size, off_t off);

    void Initialize(fuse_ino_t ino, mode_t mode, nlink_t nlink, gid_t gid, uid_t uid);


    const std::string &Link() { return m_link; }
};


#endif //P2PFS_SYMLINK_H
