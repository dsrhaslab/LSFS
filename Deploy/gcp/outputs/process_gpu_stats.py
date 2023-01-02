# importing the required module
import sys
import os
import re

if len(sys.argv) != 2:
    print("error: run python3 process_gpu_stats.py <output_file> .. ")
    sys.exit(-1)

arg_file = sys.argv[1]

gpu_utilization = []
memory_utilization = []

def clear_output(path):
    tmp_lines = []
    with open(path) as f:
        lines = f.readlines()
        for l in lines:
            if not re.search(".*timestamp,.*", l):
                tmp_lines.append(l)
    
    for l in tmp_lines:
        val = re.match(".*, ([0-9]*.[0-9]*) %, ([0-9]*.[0-9]*) %.*", l)
        gpu_utilization.append(int(val.group(1)))
        memory_utilization.append(int(val.group(2)))


def calculate_mean(list_utilization):
    su_uti = sum(list_utilization)
    return su_uti/len(list_utilization)


clear_output(arg_file)
mean_gpu_utilization = calculate_mean(gpu_utilization)
mean_memory_utilization = calculate_mean(memory_utilization)

print("Mean GPU % utilization = {} %".format(round(mean_gpu_utilization, 2)))
print("Mean Memory % utilization = {} %".format(round(mean_memory_utilization, 2)))