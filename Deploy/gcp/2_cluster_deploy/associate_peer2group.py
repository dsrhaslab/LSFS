#!/usr/bin/python3

import sys

if len(sys.argv) != 3:
    print("error: run python3 associate_peer2group <nr_peers> <nr_groups> ")
    sys.exit(-1)

nr_peers = int(sys.argv[1])
nr_groups = int(sys.argv[2])

peers_pos_l = []
val_peer_aux = 1/(nr_peers-1)
val_peer = float('%.8s'%(val_peer_aux))
val_group_aux = 1/(nr_groups)
val_group = float('%.8s'%(val_group_aux))
i = 0
while(i < nr_peers):
    val = i * val_peer
    peer_pos = float('%.8s'%(val))
    #print(peer_pos)
    j=1
    while(j <= nr_groups):
        #print(j*val_group)
        if peer_pos <= (j*val_group):
            peer_id = i + 1
            peer_group = j
            print("peer{} {}".format(peer_id, peer_group))
            break
        j=j+1
    i=i+1

