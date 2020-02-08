/* -------------------------------------------------------------------------- */

#define _XOPEN_SOURCE 500
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "fuse31.h"
#include "util.h"

#include "ops.h"

// sudo ./lsfs_haslab_exe /home/danielsf97/Desktop/Tese/P2P-Filesystem/Teste/InnerFolder/ /home/danielsf97/Desktop/Tese/P2P-Filesystem/Teste/InnerFolder2/

/* -------------------------------------------------------------------------- */

static void parse_args(
    int argc, char *argv[],
    const char **mount_point, const char **underlying_root
    )
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <mount_point> <underlying_root>\n", argv[0]);
        exit(1);
    }
    else
    {
        *mount_point     = argv[1];
        *underlying_root = argv[2];
    }
}

/* -------------------------------------------------------------------------- */

static void add_fuse_arg(struct fuse_args *args, const char *value)
{
    if (fuse_opt_add_arg(args, value) != 0)
        print_error_and_fail("fuse_opt_add_arg failed");
}

static struct fuse_args create_fuse_args(
    const char *arg0, const char *mount_point
    )
{
    struct fuse_args args = FUSE_ARGS_INIT(0, NULL);

    add_fuse_arg(&args, arg0);
    add_fuse_arg(&args, mount_point);
    add_fuse_arg(&args, "-oallow_other");
    add_fuse_arg(&args, "-odefault_permissions");

//#ifdef FUSE_PASSTHROUGH_DEBUG
    add_fuse_arg(&args, "-f");
    add_fuse_arg(&args, "-d");
//#endif

    return args;
}

/* -------------------------------------------------------------------------- */

int main(int argc, char *argv[])
{
    // parse args

    const char *mount_point;
    const char *underlying_root;

    parse_args(argc, argv, &mount_point, &underlying_root);

    // check that we are running as root

    fuse_pt_assert_super_user();

    // fuse main

    struct fuse_args args = create_fuse_args(argv[0], mount_point);

    const int result = fuse_main(
        args.argc, args.argv,
        &FUSE_HIGH_PT_OPS, (void *)underlying_root
        );

    fuse_opt_free_args(&args);

    // ---

    return result;
}

/* -------------------------------------------------------------------------- */
