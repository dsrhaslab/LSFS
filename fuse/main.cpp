//
// Created by danielsf97 on 2/4/20.
//

#include "fuse/fuse_lsfs/lsfs_impl.h"

// sudo ./lsfs_exe /home/danielsf97/Desktop/Tese/P2P-Filesystem/Teste/InnerFolder/ /home/danielsf97/Desktop/Tese/P2P-Filesystem/Teste/InnerFolder2/

int main(int argc, char *argv[])
{

    lsfs_impl fs;

    int status = fs.run(argc, argv, NULL);

    return status;
}