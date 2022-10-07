set $WORKLOAD_PATH=/test_filesystem/InnerFolder
set $NR_THREADS=16
set $WORKLOAD_RUNTIME=10

set $NR_FILES=1000
set $MEAN_DIR_WIDTH=1000000
set $IO_SIZE=1m
set $FILE_SIZE=cvar(type=cvar-gamma,parameters=mean:16384;gamma:1.5)
set $MEAN_APPEND_SIZE=16k

# ------------------------------------------------------#

define fileset name=bigfileset,path=$WORKLOAD_PATH,size=$FILE_SIZE,entries=$NR_FILES,dirwidth=$MEAN_DIR_WIDTH,prealloc=80

define process name=filereader,instances=1
{
  thread name=filereaderthread,memsize=10m,instances=$NR_THREADS
  {
    flowop deletefile name=deletefile1,filesetname=bigfileset

    flowop createfile name=createfile2,filesetname=bigfileset,fd=1
    flowop appendfilerand name=appendfilerand2,iosize=$MEAN_APPEND_SIZE,fd=1
    flowop fsync name=fsyncfile2,fd=1
    flowop closefile name=closefile2,fd=1

    flowop openfile name=openfile3,filesetname=bigfileset,fd=1
    flowop readwholefile name=readfile3,fd=1,iosize=$IO_SIZE
    flowop appendfilerand name=appendfilerand3,iosize=$MEAN_APPEND_SIZE,fd=1
    flowop fsync name=fsyncfile3,fd=1
    flowop closefile name=closefile3,fd=1
    
    flowop openfile name=openfile4,filesetname=bigfileset,fd=1
    flowop readwholefile name=readfile4,fd=1,iosize=$IO_SIZE
    flowop closefile name=closefile4,fd=1
  }
}

# ------------------------------------------------------#

system "sync"
system "echo 3 > /proc/sys/vm/drop_caches"

run $WORKLOAD_RUNTIME