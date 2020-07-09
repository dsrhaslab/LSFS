# ---------------------------------------------------------------------------- #

# This file was generated by 'generate-micro-data.py'.

# 16 threads in a single process sequentially read 32 KiB blocks from separate
# preallocated 4 GiB files until one of the threads has read its whole file.

# ---------------------------------------------------------------------------- #

define fileset name="fileset1", path="/home/danielsf97/lsfs-mount/mount", entries=16, dirwidth=16, dirgamma=0, prealloc, filesize=1999872k, reuse

define process name="process1", instances=1
{
    thread name="thread1", memsize=32k, instances=1
    {
        flowop openfile name="open1", filesetname="fileset1", fd=1, indexed=1
        flowop read name="read1", fd=1, iosize=32k, iters=131072
        flowop closefile name="close1", fd=1

        flowop finishoncount name="finish1", value=1
    }

    thread name="thread2", memsize=32k, instances=1
    {
        flowop openfile name="open2", filesetname="fileset1", fd=1, indexed=2
        flowop read name="read2", fd=1, iosize=32k, iters=131072
        flowop closefile name="close2", fd=1

        flowop finishoncount name="finish2", value=1
    }

    thread name="thread3", memsize=32k, instances=1
    {
        flowop openfile name="open3", filesetname="fileset1", fd=1, indexed=3
        flowop read name="read3", fd=1, iosize=32k, iters=131072
        flowop closefile name="close3", fd=1

        flowop finishoncount name="finish3", value=1
    }

    thread name="thread4", memsize=32k, instances=1
    {
        flowop openfile name="open4", filesetname="fileset1", fd=1, indexed=4
        flowop read name="read4", fd=1, iosize=32k, iters=131072
        flowop closefile name="close4", fd=1

        flowop finishoncount name="finish4", value=1
    }

    thread name="thread5", memsize=32k, instances=1
    {
        flowop openfile name="open5", filesetname="fileset1", fd=1, indexed=5
        flowop read name="read5", fd=1, iosize=32k, iters=131072
        flowop closefile name="close5", fd=1

        flowop finishoncount name="finish5", value=1
    }

    thread name="thread6", memsize=32k, instances=1
    {
        flowop openfile name="open6", filesetname="fileset1", fd=1, indexed=6
        flowop read name="read6", fd=1, iosize=32k, iters=131072
        flowop closefile name="close6", fd=1

        flowop finishoncount name="finish6", value=1
    }

    thread name="thread7", memsize=32k, instances=1
    {
        flowop openfile name="open7", filesetname="fileset1", fd=1, indexed=7
        flowop read name="read7", fd=1, iosize=32k, iters=131072
        flowop closefile name="close7", fd=1

        flowop finishoncount name="finish7", value=1
    }

    thread name="thread8", memsize=32k, instances=1
    {
        flowop openfile name="open8", filesetname="fileset1", fd=1, indexed=8
        flowop read name="read8", fd=1, iosize=32k, iters=131072
        flowop closefile name="close8", fd=1

        flowop finishoncount name="finish8", value=1
    }

    thread name="thread9", memsize=32k, instances=1
    {
        flowop openfile name="open9", filesetname="fileset1", fd=1, indexed=9
        flowop read name="read9", fd=1, iosize=32k, iters=131072
        flowop closefile name="close9", fd=1

        flowop finishoncount name="finish9", value=1
    }

    thread name="thread10", memsize=32k, instances=1
    {
        flowop openfile name="open10", filesetname="fileset1", fd=1, indexed=10
        flowop read name="read10", fd=1, iosize=32k, iters=131072
        flowop closefile name="close10", fd=1

        flowop finishoncount name="finish10", value=1
    }

    thread name="thread11", memsize=32k, instances=1
    {
        flowop openfile name="open11", filesetname="fileset1", fd=1, indexed=11
        flowop read name="read11", fd=1, iosize=32k, iters=131072
        flowop closefile name="close11", fd=1

        flowop finishoncount name="finish11", value=1
    }

    thread name="thread12", memsize=32k, instances=1
    {
        flowop openfile name="open12", filesetname="fileset1", fd=1, indexed=12
        flowop read name="read12", fd=1, iosize=32k, iters=131072
        flowop closefile name="close12", fd=1

        flowop finishoncount name="finish12", value=1
    }

    thread name="thread13", memsize=32k, instances=1
    {
        flowop openfile name="open13", filesetname="fileset1", fd=1, indexed=13
        flowop read name="read13", fd=1, iosize=32k, iters=131072
        flowop closefile name="close13", fd=1

        flowop finishoncount name="finish13", value=1
    }

    thread name="thread14", memsize=32k, instances=1
    {
        flowop openfile name="open14", filesetname="fileset1", fd=1, indexed=14
        flowop read name="read14", fd=1, iosize=32k, iters=131072
        flowop closefile name="close14", fd=1

        flowop finishoncount name="finish14", value=1
    }

    thread name="thread15", memsize=32k, instances=1
    {
        flowop openfile name="open15", filesetname="fileset1", fd=1, indexed=15
        flowop read name="read15", fd=1, iosize=32k, iters=131072
        flowop closefile name="close15", fd=1

        flowop finishoncount name="finish15", value=1
    }

    thread name="thread16", memsize=32k, instances=1
    {
        flowop openfile name="open16", filesetname="fileset1", fd=1, indexed=16
        flowop read name="read16", fd=1, iosize=32k, iters=131072
        flowop closefile name="close16", fd=1

        flowop finishoncount name="finish16", value=1
    }
}

# ---------------------------------------------------------------------------- #

create files

system "sync"
system "echo 3 > /proc/sys/vm/drop_caches"

run 900

# ---------------------------------------------------------------------------- #
