
set $WORKLOAD_PATH=/test_filesystem/InnerFolder
set $NR_THREADS=1
set $WORKLOAD_RUNTIME=900

set $NR_FILES=75000
set $MEAN_DIR_WIDTH=250
set $IO_SIZE=4k

# ------------------------------------------------------ #

set mode quit firstdone

define flowop name=createwriteclose
{
    flowop createfile name="createfile-1", filesetname="fileset-1", fd=1
    flowop write name="write-1", fd=1, iosize=$IO_SIZE
    flowop closefile name="closefile-1", fd=1
}

define fileset name="fileset-1", path=$WORKLOAD_PATH, entries=$NR_FILES, dirwidth=$MEAN_DIR_WIDTH, dirgamma=0, filesize=$IO_SIZE

define process name="process-1", instances=1
{
    thread name="thread-1", memsize=$IO_SIZE, instances=$NR_THREADS
    {
        flowop createwriteclose name="createwriteclose-1", iters=$NR_FILES

        flowop finishoncount name="finishoncount-1", value=1
    }
}

# ---------------------------------------------------------------------------- #

create files

system "sync"
system "echo 3 > /proc/sys/vm/drop_caches"

run $WORKLOAD_RUNTIME

# ---------------------------------------------------------------------------- #
