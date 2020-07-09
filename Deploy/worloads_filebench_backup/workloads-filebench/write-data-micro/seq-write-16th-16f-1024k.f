# ----------------------------------------------------------------------------

# This file was generated by 'generate-micro-data.py'.

# 16 threads in a single process sequentially write 1 MiB blocks to separate new
# files until one of the threads has written 4 GiB.

# ---------------------------------------------------------------------------- #

define fileset name="fileset1", path="/home/danielsf97/lsfs-mount/mount", entries=16, dirwidth=16, dirgamma=0, filesize=38g

define process name="process1", instances=1
{
    thread name="thread1", memsize=1m, instances=1
    {
        flowop createfile name="create1", filesetname="fileset1", fd=1, indexed=1
        flowop write name="write1", fd=1, iosize=1m, iters=40960
        flowop closefile name="close1", fd=1

        flowop finishoncount name="finish1", value=1
    }

    thread name="thread2", memsize=1m, instances=1
    {
        flowop createfile name="create2", filesetname="fileset1", fd=1, indexed=2
        flowop write name="write2", fd=1, iosize=1m, iters=40960
        flowop closefile name="close2", fd=1

        flowop finishoncount name="finish2", value=1
    }

    thread name="thread3", memsize=1m, instances=1
    {
        flowop createfile name="create3", filesetname="fileset1", fd=1, indexed=3
        flowop write name="write3", fd=1, iosize=1m, iters=40960
        flowop closefile name="close3", fd=1

        flowop finishoncount name="finish3", value=1
    }

    thread name="thread4", memsize=1m, instances=1
    {
        flowop createfile name="create4", filesetname="fileset1", fd=1, indexed=4
        flowop write name="write4", fd=1, iosize=1m, iters=40960
        flowop closefile name="close4", fd=1

        flowop finishoncount name="finish4", value=1
    }

    thread name="thread5", memsize=1m, instances=1
    {
        flowop createfile name="create5", filesetname="fileset1", fd=1, indexed=5
        flowop write name="write5", fd=1, iosize=1m, iters=40960
        flowop closefile name="close5", fd=1

        flowop finishoncount name="finish5", value=1
    }

    thread name="thread6", memsize=1m, instances=1
    {
        flowop createfile name="create6", filesetname="fileset1", fd=1, indexed=6
        flowop write name="write6", fd=1, iosize=1m, iters=40960
        flowop closefile name="close6", fd=1

        flowop finishoncount name="finish6", value=1
    }

    thread name="thread7", memsize=1m, instances=1
    {
        flowop createfile name="create7", filesetname="fileset1", fd=1, indexed=7
        flowop write name="write7", fd=1, iosize=1m, iters=40960
        flowop closefile name="close7", fd=1

        flowop finishoncount name="finish7", value=1
    }

    thread name="thread8", memsize=1m, instances=1
    {
        flowop createfile name="create8", filesetname="fileset1", fd=1, indexed=8
        flowop write name="write8", fd=1, iosize=1m, iters=40960
        flowop closefile name="close8", fd=1

        flowop finishoncount name="finish8", value=1
    }

    thread name="thread9", memsize=1m, instances=1
    {
        flowop createfile name="create9", filesetname="fileset1", fd=1, indexed=9
        flowop write name="write9", fd=1, iosize=1m, iters=40960
        flowop closefile name="close9", fd=1

        flowop finishoncount name="finish9", value=1
    }

    thread name="thread10", memsize=1m, instances=1
    {
        flowop createfile name="create10", filesetname="fileset1", fd=1, indexed=10
        flowop write name="write10", fd=1, iosize=1m, iters=40960
        flowop closefile name="close10", fd=1

        flowop finishoncount name="finish10", value=1
    }

    thread name="thread11", memsize=1m, instances=1
    {
        flowop createfile name="create11", filesetname="fileset1", fd=1, indexed=11
        flowop write name="write11", fd=1, iosize=1m, iters=40960
        flowop closefile name="close11", fd=1

        flowop finishoncount name="finish11", value=1
    }

    thread name="thread12", memsize=1m, instances=1
    {
        flowop createfile name="create12", filesetname="fileset1", fd=1, indexed=12
        flowop write name="write12", fd=1, iosize=1m, iters=40960
        flowop closefile name="close12", fd=1

        flowop finishoncount name="finish12", value=1
    }

    thread name="thread13", memsize=1m, instances=1
    {
        flowop createfile name="create13", filesetname="fileset1", fd=1, indexed=13
        flowop write name="write13", fd=1, iosize=1m, iters=40960
        flowop closefile name="close13", fd=1

        flowop finishoncount name="finish13", value=1
    }

    thread name="thread14", memsize=1m, instances=1
    {
        flowop createfile name="create14", filesetname="fileset1", fd=1, indexed=14
        flowop write name="write14", fd=1, iosize=1m, iters=40960
        flowop closefile name="close14", fd=1

        flowop finishoncount name="finish14", value=1
    }

    thread name="thread15", memsize=1m, instances=1
    {
        flowop createfile name="create15", filesetname="fileset1", fd=1, indexed=15
        flowop write name="write15", fd=1, iosize=1m, iters=40960
        flowop closefile name="close15", fd=1

        flowop finishoncount name="finish15", value=1
    }

    thread name="thread16", memsize=1m, instances=1
    {
        flowop createfile name="create16", filesetname="fileset1", fd=1, indexed=16
        flowop write name="write16", fd=1, iosize=1m, iters=40960
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
