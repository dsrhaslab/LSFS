#!/usr/bin/python3

import sys
import os
import re
import matplotlib.pyplot as plt
import statistics

print(sys.argv)

if len(sys.argv) != 2:
    print("error: run python3 graphdraw.py <folder1> <folder2> .. ")
    sys.exit(-1)

arg_dstat_folders = []
for i in range(1, len(sys.argv)):
    arg_dstat_folders.append(sys.argv[i])


for val in arg_dstat_folders:
    if not os.path.exists(val):
        print("error: folder '%s' does not exist!" % val)
        sys.exit(-1)

# valid_workloads = ["varmail", "rand-read", "fileserver", "seq-read", "rand-write", "seq-write", "create", "delete", "stat", "webserver"]

# if not arg_workload in valid_workloads:
#     print("error: arg_workload '%s' invalid!" % arg_workload)
#     print("valid workloads:", valid_workloads)
#     sys.exit(-1)


def import_dstat_from_file(path):
    count_host_lines = 0
    accumulate_dstat = []
    with open(path) as f:
        lines = f.readlines()
        for l in lines:
            if count_host_lines < 2:
                if re.search(".*Host.*", l):
                    count_host_lines = count_host_lines + 1
                elif not re.search(".*Dstat .*", l) and not re.search(".*Author.*", l) and not re.search(".*Cmdline.*", l) and not re.search(".*system.*", l) and not re.search(".*time.*", l) and not len(l) < 10:
                    accumulate_dstat.append(l)

    v_time_list = []
    v_usr_list = []
    v_read_list = []
    v_writ_list = []
    v_cach_list = []
    v_used_list = []
    v_free_list = []

    seconds_counter = 0

    for dstat_line in accumulate_dstat:

        if seconds_counter % 10 == 0:

            parts = dstat_line.split(",")
            
            v_time = parts[0]; v_usr = float(parts[1])
            v_sys = float(parts[2]); v_idl = float(parts[3])
            v_wai = float(parts[4]); v_stl = float(parts[5])
            v_read = float(parts[6]); v_writ = float(parts[7])
            v_used = float(parts[8]); v_free = float(parts[9])
            v_buff = float(parts[10]); v_cach = float(parts[11])
            v_in = float(parts[12]); v_out = float(parts[13])

            # Use seconds counter instead of the actual registered date
            # v_datetime = datetime.strptime(v_time, '%d-%m %H:%M:%S')

            v_time_list.append(seconds_counter)
            v_usr_list.append(v_usr)
            v_used_list.append(v_used)
            v_writ_list.append(v_writ)
            v_read_list.append(v_read)
            v_cach_list.append(v_cach)
            v_free_list.append(v_free)

        seconds_counter = seconds_counter + 1
    
    try:
        mean_cpu = round(statistics.mean(v_usr_list),3)
        mean_ram = round(statistics.mean(v_used_list),3)
    except statistics.StatisticsError:
        mean_cpu = -1
        mean_ram = -1

    obj = {
        "cpu": mean_cpu,
        "ram": mean_ram
    }
    return obj


mean_cpu_p = 0
mean_ram_p = 0
mean_cpu_c = 0
mean_ram_c = 0


for val in arg_dstat_folders:
    n_peers = 0
    for path in os.listdir(val):
        if re.search(".*peer.*.csv", path):
            obj = import_dstat_from_file("%s/%s" % (val, path))

            if obj["cpu"] >= 0 and obj["ram"] >= 0:
                mean_cpu_p = mean_cpu_p + obj["cpu"]
                mean_ram_p = mean_ram_p + obj["ram"]
                n_peers = n_peers + 1

        elif re.search(".*client.*.csv", path):
            obj = import_dstat_from_file("%s/%s" % (val, path))

            mean_cpu_c = mean_cpu_c + obj["cpu"]
            mean_ram_c = mean_ram_c + obj["ram"]
    
    mean_cpu_p = round(mean_cpu_p / n_peers, 2)
    mean_ram_p = (mean_ram_p / n_peers) / 1000000
    
    mean_ram_c = mean_ram_c  / 1000000
    
    mean_ram_p = round(mean_ram_p, 2)
    
    mean_cpu_c = round(mean_cpu_c, 2)
    mean_ram_c = round(mean_ram_c, 2)

    print("Client:")
    print("   CPU: {} %\n   RAM: {} MiB\n".format(mean_cpu_c, mean_ram_c))
    print("Peers : {}".format(n_peers))
    print("   CPU: {} %\n   RAM: {} MiB\n".format(mean_cpu_p, mean_ram_p))


