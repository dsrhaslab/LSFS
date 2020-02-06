//
// Created by danielsf97 on 2/4/20.
//

#include "lsfs_impl.h"

int main(int argc, char *argv[])
{

    lsfs_impl fs;

    int status = fs.run(argc, argv, NULL);

    return status;
}