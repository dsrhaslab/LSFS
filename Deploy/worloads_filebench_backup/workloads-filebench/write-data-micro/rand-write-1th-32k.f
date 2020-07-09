# ---------------------------------------------------------------------------- #

# This file was generated by 'generate-micro-data.py'.

# 1 thread in a single process randomly writes 32 KiB blocks to a single
# preallocated 64 GiB file until 64 GiB have been written.

# ---------------------------------------------------------------------------- #

define fileset name="fileset1", path="/home/danielsf97/lsfs-mount/mount", entries=1, dirwidth=1, dirgamma=0, filesize=64g, prealloc

define process name="process1", instances=1
{
    thread name="thread1", memsize=32k, instances=1
    {
        flowop openfile name="open1", filesetname="fileset1", fd=1
        flowop write name="write1", fd=1, iosize=32k, iters=2097152, random
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
