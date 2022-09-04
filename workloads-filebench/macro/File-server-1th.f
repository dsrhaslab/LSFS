set $WORKLOAD_PATH="test_filesystem/InnerFolder"
set $NR_THREADS=1

set $NR_FILES=5000
set $MEAN_DIR_WIDTH=20
set $IO_SIZE=4k
set $FILE_SIZE=cvar(type=cvar-gamma,parameters=mean:131072;gamma:1.5)

define fileset name=bigfileset, path=$WORKLOAD_PATH, size=$FILE_SIZE, entries=$NR_FILES, dirwidth=$MEAN_DIR_WIDTH, prealloc=80

define process name=fileserver,instances=1
{
    thread name=fileserverthread, memsize=10m, instances=$NR_THREADS
    {
        flowop createfile name=createfile1,filesetname=bigfileset,fd=1
        flowop writewholefile name=wrtfile1,srcfd=1,fd=1,iosize=$IO_SIZE
        flowop closefile name=closefile1,fd=1

        flowop openfile name=openfile1,filesetname=bigfileset,fd=1
        flowop appendfilerand name=appendfilerand1,iosize=$IO_SIZE,fd=1
        flowop closefile name=closefile2,fd=1

        flowop openfile name=openfile2,filesetname=bigfileset,fd=1
        flowop readwholefile name=readfile1,fd=1,iosize=$IO_SIZE
        flowop closefile name=closefile3,fd=1

        flowop deletefile name=deletefile1,filesetname=bigfileset

        flowop statfile name=statfile1,filesetname=bigfileset
    }
}

# ---------------------------------------------------------------------------- #

create files

system "sync"
system "echo 3 > /proc/sys/vm/drop_caches"

run 900

# ---------------------------------------------------------------------------- #
