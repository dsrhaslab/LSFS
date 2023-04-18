import sys
import json
import math
import statistics
import numpy


if len(sys.argv) != 2:
    print("error: run python3 churn_peers_killed_process.py <input_file> ")
    sys.exit(-1)

arg_file = sys.argv[1]

def get_data():
    with open(arg_file, "r") as f:
        return json.load(f)
    
data = get_data()

nr_groups = 16
nr_iterations = 4
groups = [0] * nr_groups

for it in data["churn"]:
    for peer in it["Iteration"]:
        peer_data = peer.split(' ')
        peer_group = int(peer_data[1]) - 1
        groups[peer_group] = groups[peer_group] + 1


groups_per_iter = [[0 for i in range(nr_groups)] for i in range(nr_iterations)]


i = 0
for it in data["churn"]:
    for peer in it["Iteration"]:
        peer_data = peer.split(' ')
        peer_group = int(peer_data[1]) - 1
        groups_per_iter[i][peer_group] = groups_per_iter[i][peer_group] + 1
    i = i + 1

i = 0 
for it in groups_per_iter:
    i = i + 1
    mean = round(statistics.mean(it),3)
    stdev = round(statistics.stdev(it),3)
    print("Iteration {}: \n  Mean: {} \n  Deviation: {}".format(i, mean, stdev))

## Variables 
# Deviation
# stdev = numpy.std(groups)
# Mean
mean = round(statistics.mean(groups),3)
stdev = round(statistics.stdev(groups),3)

print("\nOverall: \n  Distribution: {}\n  Mean: {} \n  Deviation: {}".format(groups, mean, stdev))