
set $WORKLOAD_PATH=test_filesystem/InnerFolder
set $NR_THREADS=1
set $WORKLOAD_RUNTIME=100

set $NR_FILES=1
set $MEAN_DIR_WIDTH=1
set $IO_SIZE=4k
set $FILE_SIZE=64g
set $NR_ITERATIONS=312500000

# ------------------------------------------------------#

set mode quit firstdone

define fileset name="fileset-1", path=$WORKLOAD_PATH, entries=$NR_FILES, dirwidth=$MEAN_DIR_WIDTH, dirgamma=0,
               filesize=$FILE_SIZE, prealloc, reuse, paralloc

define process name="process-1", instances=1
{
    thread name="thread-1", memsize=$IO_SIZE, instances=$NR_THREADS
    {
        flowop openfile name="open-1", filesetname="fileset-1", fd=1, indexed=1
        flowop read name="read-1", fd=1, iosize=$IO_SIZE, iters=$NR_ITERATIONS, random
        flowop closefile name="close-1", fd=1

        flowop finishoncount name="finish-1", value=1
    }
}

# ---------------------------------------------------------------------------- #

create files

system "sync"
system "echo 3 > /proc/sys/vm/drop_caches"

run $WORKLOAD_RUNTIME

# ---------------------------------------------------------------------------- #
