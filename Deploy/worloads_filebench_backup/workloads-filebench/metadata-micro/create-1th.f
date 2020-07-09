# ---------------------------------------------------------------------------- #

# 1 thread in a single process creates a total of 4 million 4 KiB files.

# ---------------------------------------------------------------------------- #

set $num_threads=1
set $file_size=4k
set $num_files=4000000
set $dir_width=1000
set $num_iters_per_thread=4000000

define flowop name=createandclose
{
    flowop createfile name="create1", filesetname="fileset1", fd=1
    flowop write name="write1", fd=1, iosize=$file_size
    flowop closefile name="close1", fd=1
}

define fileset name="fileset1", path="<INSERT_DIR_HERE>", entries=$num_files, dirwidth=$dir_width, dirgamma=0, filesize=$file_size

define process name="process1", instances=1
{
    thread name="thread1", memsize=$file_size, instances=$num_threads
    {
        flowop createandclose name="createandclose1", iters=$num_iters_per_thread

        flowop finishoncount name="finish1", value=1
    }
}

# ---------------------------------------------------------------------------- #

create files

system "sync"
system "echo 3 > /proc/sys/vm/drop_caches"

run 900

# ---------------------------------------------------------------------------- #
