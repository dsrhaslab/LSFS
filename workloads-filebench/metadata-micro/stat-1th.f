
set $WORKLOAD_PATH="/test_filesystem/InnerFolder"
set $NR_THREADS=1

set $NR_FILES=10000
set $MEAN_DIR_WIDTH=1000
set $IO_SIZE=4k

# ------------------------------------------------------#

define fileset name="fileset1", path=$WORKLOAD_PATH, entries=$NR_FILES, dirwidth=$MEAN_DIR_WIDTH, dirgamma=0, filesize=$IO_SIZE, prealloc

define process name="process1", instances=1
{
    thread name="thread1", memsize=$IO_SIZE, instances=$NR_THREADS
    {
        flowop statfile name="stat1", filesetname="fileset1", iters=$NR_FILES

        flowop finishoncount name="finish1", value=1
    }
}

# ---------------------------------------------------------------------------- #

create files

system "sync"
system "echo 3 > /proc/sys/vm/drop_caches"

run 600

# ---------------------------------------------------------------------------- #
