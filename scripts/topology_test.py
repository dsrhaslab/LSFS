#!usr/bin/python3

import networkx as nx
import matplotlib.pyplot as plt
import matplotlib.dates as mdates
import numpy as np
from collections import defaultdict
from subprocess import Popen
from string import Template
import json 
import time
import os
import regex as re
import datetime
import random
import shutil
import argparse
import yaml

####### EXECUTION ########

with open("conf.yaml", 'r') as stream:
    conf = yaml.safe_load(stream)

nr_peers = conf['main_confs']['nr_peers']
view_size = conf['main_confs']['view_size']
gossip_size = conf['main_confs']['gossip_size']
current_port = conf['main_confs']['base_port']
peer_instantiation_interval = conf['main_confs']['peer_instantiation_interval']
draw_graph = conf['main_confs']['draw_graph']
graph_labels = conf['main_confs']['graph_labels']

bootstrapping = '../cmake-build-debug/./bootstrapper'
peer_program = '../cmake-build-debug/./peer' 
logging_directory = '../logging/'

parser = argparse.ArgumentParser()
parser.add_argument('-e', help="Include Execution",
                    action='store_true')
parser.add_argument('-a', help="Include Analisis",
                    action='store_true')
args = vars(parser.parse_args())

################################# Functions ########################################

def connected_directed(G):
   nodos = list(G.nodes)
   res = True
   for nodo1 in nodos:
      for nodo2 in nodos:
         if nodo1 != nodo2 and not nx.has_path(G, nodo1, nodo2):
            res = False

   return res

def get_number_connected_components(peer_view_map):
   G = nx.DiGraph() #.Graph for undirected graphs
   G.add_nodes_from(peer_view_map.keys())
   for peer, view in peer_view_map.items():
      for peer2 in view:
         G.add_edge(str(peer), str(peer2))
   if draw_graph:
      nx.draw(G, pos = nx.spring_layout(G), with_labels = graph_labels)
      plt.show()

   return nx.number_strongly_connected_components(G)

def plot_graph(connected_components_data):
   times = list(connected_components_data.keys())
   times = sorted(times)
   connected_components = [connected_components_data[time] for time in times]
   plt.plot(times, connected_components)
   plt.xticks(rotation=90)
   plt.show()

def remove_all_previous_logs():
   logging_content = [os.path.join(logging_directory, o) for o in os.listdir(logging_directory)]
   for content in logging_content:
      if os.path.isdir(content):
         shutil.rmtree(content)
      else:
         os.remove(content)

def sleep_and_churn_num_peers(time_to_sleep_min, num_peers, op_type, procs):
   global current_port
   time.sleep(time_to_sleep_min * 60)

   if(op_type == 'random'):
      options = ['add', 'remove']
      op_type = random.choice(options)

   if(op_type == 'remove'):
      if num_peers <= len(procs): 
         num_peers = num_peers 
      else: 
         num_peers = len(procs)
         
      selected_procs_indexes = random.sample(range(len(procs)), num_peers)
      selected_procs_indexes = sorted(selected_procs_indexes, reverse=True) #we must pop in inverse order
      for proc_idx in selected_procs_indexes:
         proc = procs.pop(proc_idx)
         proc.terminate()
   elif(op_type == 'add'):
      peer_commands = [[peer_program, str(port), str(view_size), str(gossip_size)] for port in range(current_port, current_port + num_peers)]
      current_port += num_peers

      for command in peer_commands:
         time.sleep(peer_instantiation_interval)
         procs.append(Popen(command))

def sleep_and_churn_percentage(time_to_sleep_min, percentage, op_type, procs):
   nr_active_procs = len(procs)
   nr_target_peers = round(nr_active_procs * percentage)
   sleep_and_churn_num_peers(time_to_sleep_min=time_to_sleep_min, num_peers=nr_target_peers
                              , op_type=op_type, procs=procs)

################################# Execution ########################################
if args.get("e") == True:

   remove_all_previous_logs()

   peer_commands = [[peer_program, str(port), str(view_size), str(gossip_size)] for port in range(current_port, current_port + nr_peers)]
   current_port += nr_peers

   boot_proc = Popen(bootstrapping)

   procs = []
   for command in peer_commands:
      time.sleep(peer_instantiation_interval)
      procs.append(Popen(command))

   ###### Introducing Churn ####
   exec_time_min = conf['main_confs']['exec_time_min']
   time_passed_min = 0

   churn_conf_type = conf['simulation']['churn_conf_type']
   if churn_conf_type == 'constant':
      const_churn_data = conf['simulation']['constant']
      churn_interval_min = const_churn_data['churn_interval_min']
      op_type = churn_op_type = const_churn_data['churn_op_type']
      last_op_type = 'add' # for the alternate case
      while time_passed_min + churn_interval_min < exec_time_min:
         if churn_op_type == 'alternate':
            if last_op_type == 'add': 
               op_type = 'remove' 
            else: 
               op_type = 'add'
            last_op_type = op_type
            print('!!!!!!!!!!!!!!!!!!!!!!!!!!!!')
            print(op_type)

         if 'percentage' in const_churn_data:
            percentage = const_churn_data['percentage'] / 100
            sleep_and_churn_percentage(time_to_sleep_min=churn_interval_min, percentage=percentage, op_type=op_type, procs=procs)
         elif 'num_target_peers' in const_churn_data:
            num_target_peers = const_churn_data['num_target_peers']
            sleep_and_churn_num_peers(time_to_sleep_min=churn_interval_min, num_peers=num_peers, op_type=op_type, procs=procs)
         time_passed_min += churn_interval_min
            
      # if 'percentage' in const_churn_data:
      #    percentage = const_churn_data['percentage'] / 100
      #    while time_passed_min + churn_interval_min < exec_time_min:
      #       sleep_and_churn_percentage(time_to_sleep_min=churn_interval_min, percentage=percentage, churn_op_type=churn_op_type, procs=procs)
      #       time_passed_min += churn_interval_min
      # elif 'num_target_peers' in const_churn_data:
      #    num_target_peers = const_churn_data['num_target_peers']
      #    while time_passed_min + churn_interval_min < exec_time_min:
      #       sleep_and_churn_num_peers(time_to_sleep_min=churn_interval_min, num_peers=num_peers, churn_op_type=churn_op_type, procs=procs)
      #       time_passed_min += churn_interval_min
   elif churn_conf_type == 'manual_configuration':
      manual_churn_data = conf['simulation']['manual_configuration']
      for churn_op in manual_churn_data:
         op_type = churn_op['churn_op_type']
         time_to_sleep_min =  churn_op['time_min'] - time_passed_min

         if time_passed_min + time_to_sleep_min < exec_time_min:
            if 'percentage' in churn_op:
               percentage = churn_op['percentage'] / 100
               sleep_and_churn_percentage(time_to_sleep_min=time_to_sleep_min, percentage=percentage, op_type=op_type, procs=procs)
            elif 'num_target_peers' in churn_op:
               num_peers = churn_op['num_target_peers']
               sleep_and_churn_num_peers(time_to_sleep_min=time_to_sleep_min, num_peers=num_peers, op_type=op_type, procs=procs)
            time_passed_min += time_to_sleep_min
         else:
            break

   #sleep rest of time
   time_to_sleep_min = exec_time_min - time_passed_min
   if time_to_sleep_min > 0:
      time.sleep(time_to_sleep_min * 60)

   #############################

   for p in procs:
      p.terminate()

   for p in procs:
      p.wait()

   boot_proc.terminate()

################################# Analisis #########################################

if args.get("a") == True:

   graph_data = defaultdict(dict)

   dirs = [os.path.join(logging_directory, o) for o in os.listdir(logging_directory) 
                     if os.path.isdir(os.path.join(logging_directory,o))]

   for directory in dirs:
      peer = (re.findall(r'\d+$', directory))[0]
      filenames = [os.path.join(directory, o) for o in os.listdir(directory)]
      for filename in filenames:
         with open(filename, "r") as file:
            data = json.load(file)
            view = list(data['view'])
            # h,m,s = data['time'].split(':')
            # time = datetime.timedelta(hours=int(h),minutes=int(m),seconds=int(s))
            time = data['time']
            graph_data[time][peer] = view

   connected_components_data = {}

   times = sorted(list(graph_data.keys()))
   
   for time in times:
      connected_components_data[time] = get_number_connected_components(graph_data[time])

   plot_graph(connected_components_data)
