/* -------------------------------------------------------------------------- */

#define _XOPEN_SOURCE 500
#include <float.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "util.h"
#include "lsfs_impl.h"
#include "lsfs/fuse_common/fuse_utils.h"
#include "df_util/util.h"

/* -------------------------------------------------------------------------- */

static void change_root(const char *new_root)
{
    if (chroot(new_root) != 0)
        print_error_errno_and_fail("chroot(\"%s\") failed", new_root);

    if (chdir("/") != 0)
        print_error_errno_and_fail("chdir(\"/\") failed");
}

/* -------------------------------------------------------------------------- */

void*  lsfs_impl::_init(
    struct fuse_conn_info *conn,
    struct fuse_config *cfg
    )
{
    // clear umask

    umask(0);

    // change root and current working directory

    change_root((const char *)fuse_get_context()->private_data);

    // configure fuse
    conn->max_readahead = state->max_parallel_read_size;
    conn->max_background = 1;

    cfg->entry_timeout    = 0;
    cfg->attr_timeout     = 0;
    cfg->negative_timeout = 0;

    // log stuff

    fuse_pt_log("conn->max_write = %u\n", conn->max_write);
    fuse_pt_log("conn->max_read = %u\n", conn->max_read);
    fuse_pt_log("conn->max_readahead = %u\n", conn->max_readahead);
    fuse_pt_log("conn->max_background = %u\n", conn->max_background);
    fuse_pt_log("conn->congestion_threshold = %u\n", conn->congestion_threshold);


    std::thread cache_maintainer_thr([](){
        while(true){
            state->refresh_dir_cache();
            std::this_thread::sleep_for(std::chrono::milliseconds(state->refresh_cache_time));
            bool cache_full = state->check_if_cache_full();
            if(cache_full) state->remove_old_dirs();
        }
    });

    // ensure root directory exists
    const char* root_path = "/";
    try{

        client_reply_handler::Response response = client_reply_handler::Response::Init;    
        std::unique_ptr<kv_store_key_version> last_v = df_client->get_latest_version(root_path, &response);

         if(response == client_reply_handler::Response::NoData || response == client_reply_handler::Response::Deleted){
            //filesystem not initialize
            int res = _mkdir(root_path, 0777);
            if(res != 0){
                exit(1);
            }
        }
    
    } catch (EmptyViewException& e) {
        e.what();
        exit(1);
    } catch(TimeoutException& e){
        e.what();
        exit(1);
    }

    state->clear_all_dir_cache();


    return NULL;
}

void lsfs_impl::_destroy(
    void *private_data
    )
{
    (void)private_data;
}

/* -------------------------------------------------------------------------- */
