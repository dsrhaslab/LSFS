set $dir=/home/danielsf97/Desktop/Tese/P2P-Filesystem/Teste/InnerFolder
set $meandirwidth=5
set $meanfilesize=1k
set $iosize=1k
set $ndirs=100
set $nfilesperwriter=1
#set $nwrites=1048576

set mode quit alldone

define fileset name=fileset1,path=$dir,size=$meanfilesize,entries=$nfilesperwriter
define fileset name=fileset2,path=$dir,size=$meanfilesize,entries=$nfilesperwriter
define fileset name=fileset3,path=$dir,size=$meanfilesize,entries=$nfilesperwriter
define fileset name=fileset4,path=$dir,size=$meanfilesize,entries=$nfilesperwriter


define process name="process1", instances=1
{
    thread name="thread1", memsize=4k, instances=1
    {
        flowop createfile name="create1", filesetname="fileset1", fd=1, indexed=1
        flowop write name="write1", fd=1, iosize=4k
        flowop closefile name="close1", fd=1

        flowop finishoncount name="finish1", value=1
    }

    thread name="thread2", memsize=4k, instances=1
    {
        flowop createfile name="create2", filesetname="fileset2", fd=1, indexed=2
        flowop write name="write2", fd=1, iosize=4k
        flowop closefile name="close2", fd=1

        flowop finishoncount name="finish2", value=1
    }

    thread name="thread3", memsize=4k, instances=1
    {
        flowop createfile name="create3", filesetname="fileset3", fd=1, indexed=3
        flowop write name="write3", fd=1, iosize=4k
        flowop closefile name="close3", fd=1

        flowop finishoncount name="finish3", value=1
    }

    thread name="thread4", memsize=4k, instances=1
    {
        flowop createfile name="create4", filesetname="fileset4", fd=1, indexed=4
        flowop write name="write4", fd=1, iosize=4k
        flowop closefile name="close4", fd=1

        flowop finishoncount name="finish4", value=1
    }
}

run
