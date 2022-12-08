#!/bin/python3

import argparse
import sys
import os
import prettytable
import statistics
import re
import pandas as pd
import math

def is_float(element):
    try:
        float(element)
        return True
    except ValueError:
        return False

def convert_size(size_bytes):
   if size_bytes == 0:
       return "0B"
   size_name = ("B", "KB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB")
   i = int(math.floor(math.log(size_bytes, 1024)))
   p = math.pow(1024, i)
   s = round(size_bytes / p, 2)
   return "%s %s" % (s, size_name[i])

if len(sys.argv) != 3:
    print("./x.py output_folder config")
    sys.exit()

results_folder = sys.argv[1]
conf = sys.argv[2]

if not os.path.isdir(results_folder):
    print("The specified output '", results_folder, "' is not a folder")

micro_meta = ["create", "stat", "delete"]
micro_read = ["randread", "seqread"]
micro_write = ["randwrite", "seqwrite"]
macro = ["fileserver", "varmail", "webserver"]

filebench_table = prettytable.PrettyTable()

filebench_table.field_names = ["workload", "config", "ops", "ops/s", "rd/wr", "mb/s", "ms/op"]

print ("> generating filebench table...")

for test_output_file in os.listdir(results_folder):

    parts = test_output_file.split("-")
    
    if conf == "passthrough": 
        workload = parts[1]
        config= ""

        if workload == "seq" or workload == "rand":
            workload = parts[1] + parts[2]
            config = parts[4]
    else:
        # run-WL-FS-fb.output
        workload = parts[1]
        config= ""

        if workload == "seq" or workload == "rand":
            workload = parts[1] + parts[2]
            config = parts[4]+"-"+parts[5]+"_"+parts[6]
        elif workload in micro_meta:
            if parts[3] == "cache_on":
                config = parts[3]+"-"+parts[4]
            else: 
                config = parts[3]
        elif workload in macro:
            if parts[2] == "anti_entropy":
                config = parts[2]+"-"+parts[3]
            else: 
                config = parts[2]
        

    results_file_fd = open(results_folder+"/"+test_output_file, "r")
    result_lines = results_file_fd.readlines()
    io_summaries = list(filter(lambda line: "IO Summary" in line, result_lines))

    count_summary = 0
    list_nr_ops = []
    list_ops_s = []
    list_rd_wr_1 = []
    list_rd_wr_2 = []
    list_mb_s = []
    list_ms_op = []
    for summary in io_summaries:
        sum_parts = summary.split()
        list_nr_ops.append(float(sum_parts[3]))
        list_ops_s.append(float(sum_parts[5]))
        list_rd_wr_1.append(float(sum_parts[7].split('/')[0]))
        list_rd_wr_2.append(float(sum_parts[3].split('/')[0]))
        list_mb_s.append(float(sum_parts[9].replace("mb/s", "")))
        list_ms_op.append(float(sum_parts[10].replace("ms/op", "")))
        count_summary = count_summary + 1

    # Prevents calculating stdev with less than 2 values
    if count_summary > 1:

        avg_ops = round(statistics.mean(list_nr_ops),3)
        stdev_ops = round(statistics.stdev(list_nr_ops), 3)
        avg_ops_s = round(statistics.mean(list_ops_s),3)
        stdev_ops_s = round(statistics.stdev(list_ops_s), 3)
        avg_rd = round(statistics.mean(list_rd_wr_1),3)
        stdev_rd = round(statistics.stdev(list_rd_wr_1), 3)
        avg_wr = round(statistics.mean(list_rd_wr_2),3)
        stdev_wr = round(statistics.stdev(list_rd_wr_2), 3)
        avg_mb_s = round(statistics.mean(list_mb_s),3)
        stdev_mb_s = round(statistics.stdev(list_mb_s), 3)
        avg_ms_ops = round(statistics.mean(list_ms_op),3)
        stdev_ms_ops = round(statistics.stdev(list_ms_op), 3)

        filebench_table.add_row([workload, 
                            config, 
                            "{} (+-{})".format(avg_ops, stdev_ops),
                            "{} (+-{})".format(avg_ops_s, stdev_ops_s), 
                            "{}/{} (+-{}/+-{})".format(avg_rd, avg_wr, stdev_rd, stdev_wr),
                            "{} (+-{})".format(avg_mb_s, stdev_mb_s),
                            "{} (+-{})".format(avg_ms_ops, stdev_ms_ops)])

filebench_table.sortby = "workload"
print(filebench_table)