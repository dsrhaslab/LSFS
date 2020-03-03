#ifndef fuse_pt_header_util_h_
#define fuse_pt_header_util_h_

/* -------------------------------------------------------------------------- */

#include "fuse35.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <regex>

/* -------------------------------------------------------------------------- */

static std::regex temp_extensions("(\\.swx$|\\.swp$|\\.inf$|/\\.|~$)");
static std::regex parent_dir_pattern("(.+)/[^/]+$");
static std::regex child_name_pattern("/([^/]+)$");

int fuse_pt_is_super_user(void);
void fuse_pt_assert_super_user(void);

pid_t fuse_pt_get_tid(void);

bool fuse_pt_impersonate_calling_process(
    uid_t uid, gid_t gid, mode_t umask,
    mode_t *ptr_mode
    );

void fuse_pt_unimpersonate(void);

inline bool fuse_pt_impersonate_calling_process_highlevel(
    mode_t *ptr_mode
    )
{
    const struct fuse_context *const ctx = fuse_get_context();

    return fuse_pt_impersonate_calling_process(
        ctx->uid, ctx->gid, ctx->umask, ptr_mode
        );
}

inline bool fuse_pt_impersonate_calling_process_lowlevel(
    fuse_req_t req, mode_t *ptr_mode
    )
{
    const struct fuse_ctx *const ctx = fuse_req_ctx(req);

    return fuse_pt_impersonate_calling_process(
        ctx->uid, ctx->gid, ctx->umask, ptr_mode
        );
}

/* -------------------------------------------------------------------------- */

void print_error_v(const char *fmt, va_list args);
void print_error_errno_v(int err, const char *fmt, va_list args);

void print_error(const char *fmt, ...);
void print_error_errno(const char *fmt, ...);

void print_error_and_fail(const char *fmt, ...);
void print_error_errno_and_fail(const char *fmt, ...);

/* -------------------------------------------------------------------------- */

#ifdef FUSE_PASSTHROUGH_DEBUG

#   define fuse_pt_log(format, ...) \
    fprintf(stderr, "[pid: %lu, tid: %lu] " format, \
    (long)getpid(), (long)fuse_pt_get_tid(), ##__VA_ARGS__)

#else

#   define fuse_pt_log(format, ...)

#endif

/* -------------------------------------------------------------------------- */

bool is_temp_file(std::string path);
std::unique_ptr<std::string> get_parent_dir(std::string path);
std::unique_ptr<std::string> get_child_name(std::string path);

/* -------------------------------------------------------------------------- */
#endif /* fuse_pt_header_util_h_ */
