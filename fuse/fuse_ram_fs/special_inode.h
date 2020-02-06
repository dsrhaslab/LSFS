//
// Created by danielsf97 on 2/5/20.
//

#ifndef P2PFS_SPECIAL_INODE_H
#define P2PFS_SPECIAL_INODE_H


enum SpecialInodeTypes {
    SPECIAL_INODE_TYPE_NO_BLOCK
};

class SpecialInode : public Inode {
private:
    enum SpecialInodeTypes m_type;
public:
    SpecialInode(enum SpecialInodeTypes type);
    ~SpecialInode() {};

    int WriteAndReply(fuse_req_t req, const char *buf, size_t size, off_t off);

    int ReadAndReply(fuse_req_t req, size_t size, off_t off);

    enum SpecialInodeTypes Type();
};


#endif //P2PFS_SPECIAL_INODE_H
