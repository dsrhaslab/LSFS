define fileset name="testF",entries=10000,filesize=16k,prealloc,path="/home/branco/Mestrado/5ano/daniel-LSFS/LSFS/test_filesystem/InnerFolder"

  define process name="readerP",instances=2 {
    thread name="readerT",instances=3 {
      flowop openfile name="openOP",filesetname="testF"
      flowop readwholefile name="readOP",filesetname="testF"
      flowop closefile name="closeOP"
    }
  }


create files

system "sync"
system "echo 3 > /proc/sys/vm/drop_caches"


run 300