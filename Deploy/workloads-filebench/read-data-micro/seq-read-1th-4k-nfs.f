# ---------------------------------------------------------------------------- #

# This file was generated by 'generate-micro-data.py'.

# 1 thread in a single process sequentially reads 4 KiB blocks from a single
# preallocated 64 GiB file until the whole file has been read.

# ---------------------------------------------------------------------------- #

define fileset name="fileset1", path="/home/danielsf97/lsfs-mount/mount", entries=1, dirwidth=1, dirgamma=0, prealloc, filesize=200000000k, reuse

define process name="process1", instances=1
{
    thread name="thread1", memsize=4k, instances=1
    {
        flowop openfile name="open1", filesetname="fileset1", fd=1
        flowop read name="read1", fd=1, iosize=4k, iters=167772160
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
