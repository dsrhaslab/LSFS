#!/usr/bin/python3

import sys
import random

if len(sys.argv) != 4:
    print("error: run python3 peer_2_kill <nr_peers> <nr_groups> <nr_peers_to_kill>")
    sys.exit(-1)

nr_peers = int(sys.argv[1])
nr_groups = int(sys.argv[2])
nr_peers_to_kill = int(sys.argv[3])

peers_group_map = {}
 
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
            #print("peer{} {}".format(peer_id, peer_group))
            peer_name = peer_id
            peers_group_map[peer_name] = str(peer_group) + " " + str(peer_pos)
            break
        j=j+1
    i=i+1


random_peers = []
while len(random_peers) < nr_peers_to_kill:
    n = random.randint(1, nr_peers)
    if n not in random_peers:
        random_peers.append(n)

for peer_id in random_peers:
    print("peer{} {}".format(peer_id, peers_group_map[peer_id]))
