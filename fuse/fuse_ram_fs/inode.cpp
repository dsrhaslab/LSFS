//
// Created by danielsf97 on 2/5/20.
//

#include <cstdio>
#include <cstdlib>
#include <string>
#include <cerrno>
#include <map>
#include <tuple>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <fuse/fuse_lowlevel.h>
#include <sys/xattr.h>

#include "inode.h"

using namespace std;

Inode::~Inode() {}

/** Fix until FUSE 3 is available on all platforms. */
#ifndef FUSE_SET_ATTR_CTIME
#define FUSE_SET_ATTR_CTIME   (1 << 10)
#endif

int Inode::ReplyEntry(fuse_req_t req) {
    m_nlookup++;
    return fuse_reply_entry(req, &m_fuseEntryParam);
}

int Inode::ReplyCreate(fuse_req_t req, struct fuse_file_info *fi) {
    m_nlookup++;
    return fuse_reply_create(req, &m_fuseEntryParam, fi);
}

int Inode::ReplyAttr(fuse_req_t req) {
    return fuse_reply_attr(req, &(m_fuseEntryParam.attr), 1.0);
}

int Inode::ReplySetAttr(fuse_req_t req, struct stat *attr, int to_set) {
    if (to_set & FUSE_SET_ATTR_MODE) {
        m_fuseEntryParam.attr.st_mode = attr->st_mode;
    }
    if (to_set & FUSE_SET_ATTR_UID) {
        m_fuseEntryParam.attr.st_uid = attr->st_uid;
    }
    if (to_set & FUSE_SET_ATTR_GID) {
        m_fuseEntryParam.attr.st_gid = attr->st_gid;
    }
    if (to_set & FUSE_SET_ATTR_SIZE) {
        m_fuseEntryParam.attr.st_size = attr->st_size;
    }
    if (to_set & FUSE_SET_ATTR_ATIME) {
        m_fuseEntryParam.attr.st_atim = attr->st_atim;
    }
    if (to_set & FUSE_SET_ATTR_MTIME) {
        m_fuseEntryParam.attr.st_mtim = attr->st_mtim;
    }
    if (to_set & FUSE_SET_ATTR_CTIME) {
        m_fuseEntryParam.attr.st_ctim = attr->st_ctim;
    }


    // TODO: What do we do if this fails? Do we care? Log the event?
    clock_gettime(CLOCK_REALTIME, &(m_fuseEntryParam.attr.st_ctim));

    return fuse_reply_attr(req, &(m_fuseEntryParam.attr), 1.0);
}

void Inode::Forget(fuse_req_t req, unsigned long nlookup) {
    m_nlookup -= nlookup;

    fuse_reply_none(req);
}

int Inode::SetXAttrAndReply(fuse_req_t req, const string &name, const void *value, size_t size, int flags, uint32_t position) {
    if (m_xattr.find(name) == m_xattr.end()) {
        if (flags & XATTR_CREATE) {
            return fuse_reply_err(req, EEXIST);
        }

    } else {
        if (flags & XATTR_REPLACE) {
            return fuse_reply_err(req, ENODATA);
        }
    }

    // TODO: What about overflow with size + position?
    size_t newExtent = size + position;

    // Expand the space for the value if required.
    if (m_xattr[name].second < newExtent) {
        void *newBuf = realloc(m_xattr[name].first, newExtent);
        if (newBuf == NULL) {
            return fuse_reply_err(req, E2BIG);
        }

        m_xattr[name].first = newBuf;

        // TODO: How does the user truncate the value? I.e., if they want to replace part, they'll send in
        // a position and a small size, right? If they want to make the whole thing shorter, then what?
        m_xattr[name].second = newExtent;
    }

    // Copy the data.
    memcpy((char *) m_xattr[name].first + position, value, size);

    return fuse_reply_err(req, 0);
}

int Inode::GetXAttrAndReply(fuse_req_t req, const string &name, size_t size, uint32_t position) {
    if (m_xattr.find(name) == m_xattr.end()) {
        return fuse_reply_err(req, ENODATA);
    }

    // The requestor wanted the size. TODO: How does position figure into this?
    if (size == 0) {
        return fuse_reply_xattr(req, m_xattr[name].second);
    }

    // TODO: What about overflow with size + position?
    size_t newExtent = size + position;

    // TODO: Is this the case where "the size is to small for the value"?
    if (m_xattr[name].second < newExtent) {
        return fuse_reply_err(req, ERANGE);
    }

    // TODO: It's fine for someone to just read part of a value, right (i.e. size is less than m_xattr[name].second)?
    return fuse_reply_buf(req, (char *) m_xattr[name].first + position, size);
}

int Inode::ListXAttrAndReply(fuse_req_t req, size_t size) {

    size_t listSize = 0;
    for(map<string, pair<void *, size_t> >::iterator it = m_xattr.begin(); it != m_xattr.end(); it++) {
        listSize += (it->first.size() + 1);
    }

    // The requestor wanted the size
    if (size == 0) {
        return fuse_reply_xattr(req, listSize);
    }

    // "If the size is too small for the list, the ERANGE error should be sent"
    if (size < listSize) {
        return fuse_reply_err(req, ERANGE);
    }

    // TODO: Is EIO really the best error to return if we ran out of memory?
    void *buf = malloc(listSize);
    if (buf == NULL) {
        return fuse_reply_err(req, EIO);
    }

    size_t position = 0;
    for(map<string, pair<void *, size_t> >::iterator it = m_xattr.begin(); it != m_xattr.end(); it++) {
        // Copy the name as well as the null termination character.
        memcpy((char *) buf + position, it->first.c_str(), it->first.size() + 1);
        position += (it->first.size() + 1);
    }

    int retval = fuse_reply_buf(req, (char *) buf, position);
    free(buf);

    return retval;
}

int Inode::RemoveXAttrAndReply(fuse_req_t req, const string &name) {
    map<string, pair<void *, size_t> >::iterator it = m_xattr.find(name);
    if (it == m_xattr.end()) {
        return fuse_reply_err(req, ENODATA);
    }

    m_xattr.erase(it);

    return fuse_reply_err(req, 0);
}

int Inode::ReplyAccess(fuse_req_t req, int mask, gid_t gid, uid_t uid) {
    // If all the user wanted was to know if the file existed, it does.
    if (mask == F_OK) {
        return fuse_reply_err(req, 0);
    }

    // Check other
    if ((m_fuseEntryParam.attr.st_mode & mask) == mask) {
        return fuse_reply_err(req, 0);
    }
    mask <<= 3;

    // Check group. TODO: What about other groups the user is in?
    if ((m_fuseEntryParam.attr.st_mode & mask) == mask) {
        // Go ahead if the user's main group is the same as the file's
        if (gid == m_fuseEntryParam.attr.st_gid) {
            return fuse_reply_err(req, 0);
        }

        // Now check the user's other groups. TODO: Where is this function?! not on this version of FUSE?
        // int numGroups = fuse_req_getgroups(req, 0, NULL);

    }
    mask <<= 3;

    // Check owner.
    if ((uid == m_fuseEntryParam.attr.st_uid) && (m_fuseEntryParam.attr.st_mode & mask) == mask) {
        return fuse_reply_err(req, 0);
    }

    return fuse_reply_err(req, EACCES);
}

void Inode::Initialize(fuse_ino_t ino, mode_t mode, nlink_t nlink, gid_t gid, uid_t uid) {

    // TODO: Still not sure if I should use m_fuseEntryParam = {}
    memset(&m_fuseEntryParam, 0, sizeof(m_fuseEntryParam));
    m_fuseEntryParam.ino = ino;
    m_fuseEntryParam.attr_timeout = 1.0;
    m_fuseEntryParam.entry_timeout = 1.0;
    m_fuseEntryParam.attr.st_mode = mode;
    m_fuseEntryParam.attr.st_gid = gid;
    m_fuseEntryParam.attr.st_uid = uid;

    // Note this found on the Internet regarding nlink on dirs:
    // "For the root directory it is at least three; /, /., and /... Make a directory /foo and /foo/.. will have the same inode number as /, incrementing st_nlink.
    //
    // Cheers, Ralph."
    m_fuseEntryParam.attr.st_nlink = nlink;

    m_fuseEntryParam.attr.st_blksize = Inode::BufBlockSize;

    timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    m_fuseEntryParam.attr.st_atim = ts;
    m_fuseEntryParam.attr.st_ctim = ts;
    m_fuseEntryParam.attr.st_mtim = ts;
}