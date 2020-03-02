/* -------------------------------------------------------------------------- */

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "util.h"


/* -------------------------------------------------------------------------- */

pid_t fuse_pt_get_tid(void)
{
    return (pid_t)syscall(SYS_gettid);
}

int fuse_pt_is_super_user(void)
{
    uid_t ruid, euid, suid;
    gid_t rgid, egid, sgid;

    if (getresuid(&ruid, &euid, &suid) != 0)
        return -1;

    if (getresgid(&rgid, &egid, &sgid) != 0)
        return -1;

    return
        ruid == 0 && euid == 0 && suid == 0 &&
        rgid == 0 && egid == 0 && sgid == 0;
}

void fuse_pt_assert_super_user(void)
{
    switch (fuse_pt_is_super_user())
    {
    case 0:
        print_error_and_fail("not superuser");

    case 1:
        break;

    default:
        print_error_errno_and_fail("failed to check if user is superuser");
    }
}

bool fuse_pt_impersonate_calling_process(
    uid_t uid, gid_t gid, mode_t umask,
    mode_t *ptr_mode
    )
{
    // From 'man 2 setfsuid': "setfsuid() is nowadays unneeded and should be
    // avoided in new applications (likewise for setfsgid(2))".

    // The seteuid/setegid wrappers may not be used, as they set the effective
    // UID/GID for the process (not only for the current thread). Therefore, the
    // setresuid/setresgid system calls must be used directly.

    // Note that the effective GID must be set before setting the effective UID,
    // as setting the former may result in the loss of the CAP_SETGID
    // capability.

    if (syscall(SYS_setresgid, (gid_t)(-1), gid, (gid_t)(-1)) != 0)
        return false;

    if (syscall(SYS_setresuid, (uid_t)(-1), uid, (uid_t)(-1)) != 0)
        return false;

    if (ptr_mode)
        *ptr_mode &= ~umask;

    return true;
}

void fuse_pt_unimpersonate(void)
{
    // This resets the current thread's effective UID to that of the root user.
    // This is necessary because the same thread may be subsequently used for
    // more operations, which may require root privileges. There is no need to
    // reset the thread's effective GID.

    if (syscall(SYS_setresuid, (uid_t)(-1), 0, (uid_t)(-1)) != 0)
        print_error_errno_and_fail("syscall(SYS_setresuid, -1, 0, -1) failed");
}

/* -------------------------------------------------------------------------- */

void print_error_v(const char *fmt, va_list args)
{
    fprintf(stderr, "fuse_passthrough: fatal error: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
}

void print_error_errno_v(int err, const char *fmt, va_list args)
{
    fprintf(stderr, "fuse_passthrough: fatal error: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, ": %s (errno = %d)\n", strerror(err), err);
}

void print_error(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    print_error_v(fmt, args);

    va_end(args);
}

void print_error_errno(const char *fmt, ...)
{
    const int err = errno;

    va_list args;
    va_start(args, fmt);

    print_error_errno_v(err, fmt, args);

    va_end(args);
}

void print_error_and_fail(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    print_error_v(fmt, args);

    va_end(args);

    exit(1);
}

void print_error_errno_and_fail(const char *fmt, ...)
{
    const int err = errno;

    va_list args;
    va_start(args, fmt);

    print_error_errno_v(err, fmt, args);

    va_end(args);

    exit(1);
}

/* -------------------------------------------------------------------------- */

bool is_temp_file(std::string path){
    std::smatch match;
    return std::regex_search(path, match, temp_extensions);
}

std::unique_ptr<std::string> get_parent_dir(std::string path){
    std::smatch match;
    auto res = std::regex_search(path, match, parent_dir_pattern);

    if(match.size() > 0){
        return std::make_unique<std::string>(std::string(match[1].str()));
    }

    return nullptr;
}