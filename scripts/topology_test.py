#!usr/bin/python3

import networkx as nx
import matplotlib.pyplot as plt
import numpy as np
from collections import defaultdict
from subprocess import Popen
from string import Template
import json 
import time
  

####### EXECUTION ########

nr_peers = 50
exec_time = 6 * 60
base_port = 12350
labels = False

bootstrapping = '../cmake-build-debug/./bootstrapper'
peer_program = '../cmake-build-debug/./peer' 

peer_commands = [[peer_program, str(x), str(exec_time)] for x in range(base_port, base_port + nr_peers)]

boot_proc = Popen(bootstrapping)

# procs = [ Popen(command) for command in peer_commands]

procs = []
for command in peer_commands:
   time.sleep(0.1)
   procs.append(Popen(command))

for p in procs:
   p.wait()

boot_proc.terminate()

####### ANALISIS ########

filenames = ['../logging/' + str(port) + '.json' for port in range(base_port, base_port + nr_peers)]
graph_data = {}

for filename in filenames:
    with open(filename, "r") as file:
        data = json.load(file)
        graph_data[data['peer']] = data['view']

G = nx.DiGraph() #.Graph for undirected graphs
G.add_nodes_from(list(graph_data.keys()))
for peer, view in graph_data.items():
    for peer2 in view:
        G.add_edge(peer, peer2)


nx.draw(G, pos = nx.spring_layout(G), with_labels = labels)
plt.show()

def connected_directed(G):
   nodos = list(G.nodes)
   res = True
   for nodo1 in nodos:
      for nodo2 in nodos:
         if nodo1 != nodo2 and not nx.has_path(G, nodo1, nodo2):
            print(str(nodo1) + "  " + str(nodo2))
            res = False

   return res

print(nx.average_clustering(G))
#print(nx.is_connected(G)) #implemented only for directed graphs
print(connected_directed(G))