#include "metadata_attr.h"

metadata_attr::metadata_attr(struct stat& stbuf): stbuf(stbuf){};

metadata_attr::metadata_attr(const metadata_attr& met): stbuf(met.stbuf){};


void metadata_attr::initialize_metadata(struct stat* stbuf, mode_t mode, nlink_t nlink, gid_t gid, uid_t uid){
    memset(stbuf, 0, sizeof(struct stat));
    stbuf->st_mode = mode;
    stbuf->st_gid = gid;
    stbuf->st_uid = uid;
    stbuf->st_nlink = nlink;
    timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    stbuf->st_atim = ts;
    stbuf->st_ctim = ts;
    stbuf->st_mtim = ts;
    stbuf->st_blksize = BLK_SIZE;
}
