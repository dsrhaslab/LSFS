set $WORKLOAD_PATH=/test_filesystem/InnerFolder
set $NR_THREADS=1
set $WORKLOAD_RUNTIME=900

set $NR_FILES=10000
set $MEAN_DIR_WIDTH=20
set $IO_SIZE=1m
set $FILE_SIZE=cvar(type=cvar-gamma,parameters=mean:131072;gamma:1.5)
set $MEAN_APPEND_SIZE=16k

# ------------------------------------------------------#

define fileset name=bigfileset,path=$WORKLOAD_PATH,size=$FILE_SIZE,entries=$NR_FILES,dirwidth=$MEAN_DIR_WIDTH,prealloc=80

define process name=filereader,instances=1
{
  thread name=filereaderthread,memsize=10m,instances=$NR_THREADS
  {
    flowop createfile name=createfile1,filesetname=bigfileset,fd=1
    flowop writewholefile name=wrtfile1,srcfd=1,fd=1,iosize=$IO_SIZE
    flowop closefile name=closefile1,fd=1

    flowop openfile name=openfile1,filesetname=bigfileset,fd=1
    flowop appendfilerand name=appendfilerand1,iosize=$MEAN_APPEND_SIZE,fd=1
    flowop closefile name=closefile2,fd=1

    flowop openfile name=openfile2,filesetname=bigfileset,fd=1
    flowop readwholefile name=readfile1,fd=1,iosize=$IO_SIZE
    flowop closefile name=closefile3,fd=1
    
    flowop deletefile name=deletefile1,filesetname=bigfileset
    flowop statfile name=statfile1,filesetname=bigfileset
  }
}

# ------------------------------------------------------#

system "sync"
system "echo 3 > /proc/sys/vm/drop_caches"

run $WORKLOAD_RUNTIME