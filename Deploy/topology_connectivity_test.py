#!usr/bin/python3

import networkx as nx
import matplotlib.pyplot as plt
import matplotlib.dates as mdates
import numpy as np
from collections import defaultdict
from subprocess import Popen
from string import Template
from math import floor
import json 
import time
import os
import regex as re
import datetime
import random
import shutil
import argparse
import yaml
import time
import pickle
import numpy

parser = argparse.ArgumentParser()
parser.add_argument('-e', help="Include Execution",
                    action='store_true')
parser.add_argument('-a', help="Include Analisis",
                    action='store_true')
parser.add_argument('-r', '--remote', help="Remote Execution",
                    action='store_true')
parser.add_argument('-d', help="Draw final graph",
                    action='store_true')
parser.add_argument("-c", "--config", help="config file to use (conf.yaml default)", default="conf.yaml")
args = vars(parser.parse_args())

####### EXECUTION ########

with open(args['config'], 'r') as stream:
    conf = yaml.safe_load(stream)

nr_peers = conf['main_confs']['nr_peers']
view_size = conf['main_confs']['view_size']
gossip_size = conf['main_confs']['gossip_size']
base_port = current_port = conf['main_confs']['base_port']
peer_instantiation_interval_sec = conf['main_confs']['peer_instantiation_interval_sec']
log_interval = conf['main_confs']['log_interval_sec']
message_passing_interval_sec = conf['main_confs']['message_passing_interval_sec']
draw_graph = conf['main_confs']['draw_graph']
graph_labels = conf['main_confs']['graph_labels']
results_directory = "results/"
logging_directory = "logt/"
rep_min = conf['main_confs']['rep_min']
rep_max = conf['main_confs']['rep_max']
nr_peers_known_to_torecover = view_size / 2
draw_final_graph = args.get("d")

last_id = 0

################################# Functions ########################################

def connected_directed(G):
   nodos = list(G.nodes)
   res = True
   for nodo1 in nodos:
      for nodo2 in nodos:
         if nodo1 != nodo2 and not nx.has_path(G, nodo1, nodo2):
            res = False

   return res

def get_number_connected_components(peers_data_in_specific_time_map):
   G = nx.DiGraph() #.Graph for undirected graphs
   online_nodes = list(peers_data_in_specific_time_map.keys())
   G.add_nodes_from(online_nodes)
   print(len(peers_data_in_specific_time_map))
   for peer, peer_data in peers_data_in_specific_time_map.items():
      view = peer_data['view']
      for peer2 in view:
         if str(peer2) in online_nodes:
            G.add_edge(str(peer), str(peer2))
   if draw_graph:
      nx.draw(G, pos = nx.spring_layout(G), with_labels = graph_labels)
      plt.show()

   return nx.number_strongly_connected_components(G)

def get_peer_percent_nr_groups_deviation(peers_data_in_specific_time_map, time):
   nr_peers = len(peers_data_in_specific_time_map)
   nr_groups_max = float(nr_peers) / rep_min
   if(nr_groups_max < 1): nr_groups_max = 1
   nr_groups_min = float(nr_peers) / rep_max
   if(nr_groups_min < 1): nr_groups_min = 1

   i = 0
   while 2**i < nr_groups_min: # 2**i = 2^i
      i+=1

   correct_nr_groups = 2**i
   if correct_nr_groups > nr_groups_max:
      print("ERROR found: nr_peers= " + str(nr_peers) + ", nr_groups_min=" + str(nr_groups_min) + ", nr_groups_max=" + str(nr_groups_max))

   nr_peers_with_wrong_estimation = 0 
   for peer, peer_data in peers_data_in_specific_time_map.items():
      nr_groups = peer_data['nr_groups']
      if nr_groups != correct_nr_groups:
         print("Bad estimation peer(" + str(time) + "): " + str(peer) + ", correct: " + str(correct_nr_groups) + ", estimation: " + str(nr_groups))
         nr_peers_with_wrong_estimation+=1

   return nr_peers_with_wrong_estimation/nr_peers*100

def plot_graph(connected_components_data):
   times = list(connected_components_data.keys())
   times = sorted(times)
   connected_components = [connected_components_data[time] for time in times]
   plt.plot(times, connected_components)
   plt.xticks(rotation=90)

   ensure_dir(results_directory)
   filename_no_extension = os.path.splitext(args['config'])[0]
   ensure_dir(results_directory + filename_no_extension + '/')
   plt.savefig(results_directory + filename_no_extension + '/graph.pdf')
   if draw_final_graph:
      plt.show()
   plt.close()

def plot_nr_groups_divergence(peer_percent_nr_groups_estimation_deviation):
   times = list(peer_percent_nr_groups_estimation_deviation.keys())
   times = sorted(times)
   percents = [peer_percent_nr_groups_estimation_deviation[time] for time in times]
   plt.plot(times, percents)
   plt.xticks(rotation=90)

   ensure_dir(results_directory)
   filename_no_extension = os.path.splitext(args['config'])[0]
   ensure_dir(results_directory + filename_no_extension + '/')
   plt.savefig(results_directory + filename_no_extension + '/groups.pdf')
   if draw_final_graph:
      plt.show()   
   plt.close()

def time_to_sec_diff(start_time, time_list):
   (st_h_str, st_m_str, st_s_str) = tuple(start_time.split(':'))
   (st_h, st_m, st_s) = (int(st_h_str), int(st_m_str), int(st_s_str))
   res = []

   for time in time_list:
      (t_h_str, t_m_str, t_s_str) = tuple(time.split(':'))
      (t_h, t_m, t_s) = (int(t_h_str), int(t_m_str), int(t_s_str))
      sec_diff = int((t_h - st_h) * 3600 + (t_m - st_m) * 60 + (t_s - st_s))
      res.append(sec_diff)

   return res

def time_diff_sec(start_time, time):
   (st_h_str, st_m_str, st_s_str) = tuple(start_time.split(':'))
   (st_h, st_m, st_s) = (int(st_h_str), int(st_m_str), int(st_s_str))

   (t_h_str, t_m_str, t_s_str) = tuple(time.split(':'))
   (t_h, t_m, t_s) = (int(t_h_str), int(t_m_str), int(t_s_str))

   return int((t_h - st_h) * 3600 + (t_m - st_m) * 60 + (t_s - st_s))

def remove_all_previous_logs():
   ensure_dir(logging_directory)
   logging_content = [os.path.join(logging_directory, o) for o in os.listdir(logging_directory)]
   for content in logging_content:
      if os.path.isdir(content):
         shutil.rmtree(content, ignore_errors=True)
      else:
         os.remove(content)

def ensure_dir(file_path):
    directory = os.path.dirname(file_path)
    if not os.path.exists(directory):
        os.makedirs(directory)

def calculate_mean_recover_time(graph_data):
   #graph_data : {time => {node => {'view' => [node viz]}}}
   initial_nodes = [port for port in range(base_port, base_port + nr_peers)]
   new_nodes_recover_time = {}
   final_recover_time = {}
   ended_without_recover = {}

   ordered_times = sorted(list(graph_data.keys()))
   ordered_times_len = len(ordered_times)

   for time_idx in range(ordered_times_len):
      time = ordered_times[time_idx]
      time_data = graph_data[time]
      time_data_follw = graph_data[ordered_times[time_idx + 1]] if time_idx + 1 < ordered_times_len else None
      new_nodes_known_to = defaultdict(int)

      # remoção dos nodos que são deitados a baixo
      toDelete = []
      for node in new_nodes_recover_time:
         if node not in time_data:
            if time_data_follw and node in time_data_follw:
               continue
            ended_without_recover[node] = new_nodes_recover_time[node]
            toDelete.append(node)
      for node in toDelete:
         del new_nodes_recover_time[node]

      # add new nodes
      for node in time_data.keys():
         if node not in initial_nodes:
            initial_nodes.append(node)
            new_nodes_recover_time[node] = 0

      # calculate for each node alive number of nodes known to
      for node, node_time_data in time_data.items():
         node_time_view = node_time_data['view']
         for node2 in node_time_view:
            if str(node2) in new_nodes_recover_time:
               new_nodes_known_to[str(node2)] += 1

      # increase time to active nodes
      for node in list(new_nodes_recover_time.keys()):
         new_nodes_recover_time[node] += log_interval

      for node, nr_known_to in new_nodes_known_to.items():
         if nr_known_to >= nr_peers_known_to_torecover:
            final_recover_time[node] = new_nodes_recover_time[node]
            del new_nodes_recover_time[node]

   nr_peers_that_recover = len(final_recover_time)
   if (nr_peers_that_recover != 0):
      mean_time_to_recover = sum(list(final_recover_time.values())) / nr_peers_that_recover
   else: 
      mean_time_to_recover = "No peers have recovered!"

   nr_peers_ended_no_recover = len(ended_without_recover)
   if (nr_peers_ended_no_recover != 0):
      avg_time_peers_ended_no_recover = sum(list(ended_without_recover.values())) / nr_peers_ended_no_recover
   else:
      avg_time_peers_ended_no_recover = "Every peer who ended have recovered!"

   # logging results
   ensure_dir(results_directory)
   filename_no_extension = os.path.splitext(args['config'])[0]
   ensure_dir(results_directory + filename_no_extension + '/')

   results_txt_file = open(results_directory + filename_no_extension + "/results.txt", 'w')
   results_txt_file.write(json.dumps(new_nodes_recover_time) + '\n')
   results_txt_file.write(json.dumps(final_recover_time) + '\n')
   results_txt_file.write(json.dumps(ended_without_recover) + '\n')
   results_txt_file.write("Mean time to recover: " + str(mean_time_to_recover) + '\n')
   results_txt_file.write("Nr peers who ended without recovering: " + str(nr_peers_ended_no_recover) + '\n')
   results_txt_file.write("Avg time lived peers ended without recovering: " + str(avg_time_peers_ended_no_recover) + '\n')
   results_txt_file.close()
         

################################# Analisis #########################################

if args.get("a") == True:

   graph_data = defaultdict(lambda: defaultdict(dict))

   # reading start time
   start_file = open(logging_directory + "start_time", 'r')
   start_time = start_file.readline()
   start_file.close()

   log_files =  [os.path.join(logging_directory, o) for o in os.listdir(logging_directory)
                     if( not re.match(r'start_time', o)) ]

   for filename in log_files:
      peer = (re.findall(r'(\d+).txt$', filename))[0]
      print(filename)
      print(peer)
      with open(filename, "r") as file:
         line = file.readline()
         while(line):

            try:
               data = json.loads(line)
               view = list(data['view'])
               print(view)
               time = data['time']
               time_sec = time_diff_sec(start_time, time)
               nr_groups = data['nr_groups']
               graph_data[time_sec][peer]['view'] = view
               graph_data[time_sec][peer]['nr_groups'] = nr_groups
            except Exception:
               print("LOADING JSON ERROR!")
            line = file.readline()        

   calculate_mean_recover_time(graph_data)

   connected_components_data = {}
   peer_percent_nr_groups_estimation_deviation = {}

   times_in_secs = sorted(list(graph_data.keys()))

   for time in times_in_secs:
      connected_components_data[time] = get_number_connected_components(graph_data[time])
      peer_percent_nr_groups_estimation_deviation[time] = get_peer_percent_nr_groups_deviation(graph_data[time], time)

   #pickle.dump(connected_components_data, open(results_directory + "graph_data.p", "wb"))

   plot_graph(connected_components_data)
   plot_nr_groups_divergence(peer_percent_nr_groups_estimation_deviation)
