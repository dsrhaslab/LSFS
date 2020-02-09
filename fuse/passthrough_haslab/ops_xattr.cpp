/* -------------------------------------------------------------------------- */

#include <errno.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/xattr.h>

#include "util.h"
#include "../lsfs_impl.h"

/* -------------------------------------------------------------------------- */

int lsfs_impl::_getxattr(
    const char *path,
    const char *name, char *value, size_t size
    )
{
    return (lgetxattr(path, name, value, size) == 0) ? 0 : -errno;
}

int lsfs_impl::_setxattr(
    const char *path,
    const char *name, const char *value, size_t size,
    int flags
    )
{
    return (lsetxattr(path, name, value, size, flags) == 0) ? 0 : -errno;
}

int lsfs_impl::_listxattr(
    const char *path,
    char *list, size_t size
    )
{
    return (llistxattr(path, list, size) == 0) ? 0 : -errno;
}

int lsfs_impl::_removexattr(
    const char *path,
    const char *name
    )
{
    return (lremovexattr(path, name) == 0) ? 0 : -errno;
}

/* -------------------------------------------------------------------------- */
