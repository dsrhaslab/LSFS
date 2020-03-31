# ---------------------------------------------------------------------------- #

# This file was generated by 'generate-micro-data.py'.

# 16 threads in a single process sequentially read 4 KiB blocks from the same
# preallocated 64 GiB file until one of the threads has read the whole file.

# ---------------------------------------------------------------------------- #

define fileset name="fileset1", path="<INSERT_DIR_HERE>", entries=1, dirwidth=1, dirgamma=0, filesize=64g, prealloc

define process name="process1", instances=1
{
    thread name="thread1", memsize=4k, instances=16
    {
        flowop openfile name="open1", filesetname="fileset1", fd=1
        flowop read name="read1", fd=1, iosize=4k, iters=16777216
        flowop closefile name="close1", fd=1

        flowop finishoncount name="finish1", value=1
    }
}

# ---------------------------------------------------------------------------- #

create files

system "sync"
system "echo 3 > /proc/sys/vm/drop_caches"

run 900

# ---------------------------------------------------------------------------- #
