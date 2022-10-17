#include <errno.h>
#include <sys/statvfs.h>

#include "util.h"
#include "lsfs_impl.h"

int lsfs_impl::_statfs(
    const char *path, struct statvfs *stbuf
    )
{
    //std::cout << "### SysCall: _statfs" << std::endl;

    return (statvfs(path, stbuf) == 0) ? 0 : -errno;
}
