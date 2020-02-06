//
// Created by danielsf97 on 2/5/20.
//

#include "file.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <queue>
#include <map>
#include <cstring>
#ifdef __APPLE__
#include <osxfuse/fuse/fuse_lowlevel.h>
#else
#include <fuse/fuse_lowlevel.h>
#endif

#include "inode.h"
#include "../lsfs_impl.h"
#include "file.h"

File::~File() {
    free(m_buf);
}

int File::WriteAndReply(fuse_req_t req, const char *buf, size_t size, off_t off) {
    // Allocate more memory if we don't have space.
    size_t newSize = off + size;
    size_t originalCapacity = Inode::BufBlockSize * m_fuseEntryParam.attr.st_blocks;
    if (newSize > originalCapacity) {
        size_t newBlocks = newSize/Inode::BufBlockSize + (newSize % Inode::BufBlockSize != 0);
        void *newBuf = realloc(m_buf, newBlocks * Inode::BufBlockSize);
        // If we ran out of memory, let the caller know that no bytes were
        // written.
        if (newBuf == NULL) {
            return fuse_reply_write(req, 0);;
        }

        // Update our buffer size
        m_buf = newBuf;
        lsfs_impl::UpdateUsedBlocks(newBlocks - m_fuseEntryParam.attr.st_blocks);
        m_fuseEntryParam.attr.st_blocks = newBlocks;
    }

    // Write to the buffer. TODO: Check if SRC and DST overlap.
    // We assume that there can be no buffer overflow since realloc has already
    // been called with the new size and offset. If realloc failed, we wouldn't
    // be here.
    memcpy((char *) m_buf + off, buf, size);
    if (newSize > m_fuseEntryParam.attr.st_size) {
        m_fuseEntryParam.attr.st_size = newSize;
    }

    // TODO: What do we do if this fails? Do we care? Log the event?

    clock_gettime(CLOCK_REALTIME, &(m_fuseEntryParam.attr.st_ctim));
    m_fuseEntryParam.attr.st_mtim = m_fuseEntryParam.attr.st_ctim;

    return fuse_reply_write(req, size);
}

int File::ReadAndReply(fuse_req_t req, size_t size, off_t off) {
    // Don't start the read past our file size
    if (off > m_fuseEntryParam.attr.st_size) {
        return fuse_reply_buf(req, (const char *) m_buf, 0);
    }

    // Update access time. TODO: This could get very intensive. Some
    // filesystems buffer this with options at mount time. Look into this.
    // TODO: What do we do if this fails? Do we care? Log the event?

    clock_gettime(CLOCK_REALTIME, &(m_fuseEntryParam.attr.st_atim));

    // Handle reading past the file size as well as inside the size.
    size_t bytesRead = off + size > m_fuseEntryParam.attr.st_size ? m_fuseEntryParam.attr.st_size - off : size;

    // TODO: There are all sorts of other replies. What about them?
    return fuse_reply_buf(req, (const char *) m_buf + off, bytesRead);
}