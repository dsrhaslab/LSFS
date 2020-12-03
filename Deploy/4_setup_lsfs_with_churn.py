import configparser
from ansible.parsing.dataloader import DataLoader
from ansible.inventory.manager import InventoryManager
from time import sleep
import subprocess
import os
import regex as re
from collections import defaultdict
from queue import Queue 
import math
import numpy
import json
import argparse
import datetime
import pause
import logging

####### Global Static Variables ########

mount_folder = "/home/danielsf97/lsfs-mount/mount"
churn_interval   = 60 # 1 em 1 minuto
churn_iterations_per_phase = 15
time_between_phases = 60
nr_of_phases = 1
churn_percentage = 25 # por cento
nr_of_nodes         = 4
warmup_interval     = 120 #1 * 60 # segundos
mount_interval      = 120
replication_factor_min = 5
replication_factor_max = 10
free_database_space_on_new_node_launch = False
churn_log_file = "churn_log_file.txt"

####### Global Dynamic Variables #######

bootstrapper_pod_ip = None
master_addr = None
client_addr = None
current_peer_id = None;
peer_ids = []
nr_of_groups = None
groups_usage_queue = Queue() # [5, 6, 7, 8, 9, 10, 1, 2, 3, 4] ex with 10 groups
groups = defaultdict(Queue) # group_nr => Queue((node_id, node_pos, machine_nr))

########### Argument Parse ###########

def str2bool(v):
    if isinstance(v, bool):
       return v
    if v.lower() in ('yes', 'true', 't', 'y', '1'):
        return True
    elif v.lower() in ('no', 'false', 'f', 'n', '0'):
        return False
    else:
        raise argparse.ArgumentTypeError('Boolean value expected.')

parser = argparse.ArgumentParser()
parser.add_argument('-i', '--init', help="Init Network",
                    action='store_true')
parser.add_argument('--warmup_int', help="Warmup Interval")
parser.add_argument('--mount_int', help="Mount Interval")
parser.add_argument('--replication_factor_min', help="Replication factor min")
parser.add_argument('--replication_factor_max', help="Replication factor max")
parser.add_argument('--nr_nodes', help="Number of Nodes")
parser.add_argument('--churn_int', help="Churn Interval")
parser.add_argument('--nr_phases', help="Churn Number of Phases")
parser.add_argument('--per_phase_its', help="Iterations Per Phase")
parser.add_argument('--time_bet_phases', help="Time between Phases")
parser.add_argument('--churn_percent', help="Churn Percentage")
parser.add_argument('--free_database', help="Free database on nodes startup", type = str2bool, const=True, default=free_database_space_on_new_node_launch, nargs='?')
parser.add_argument('--churn_log_file', help="Churn Log File")

args = vars(parser.parse_args())

if args.get('warmup_int'):
  warmup_interval = int(args.get('warmup_int'))
if args.get('mount_int'):
  warmup_interval = int(args.get('mount_int'))
if args.get('replication_factor_min'):
  replication_factor_min = int(args.get('replication_factor_min'))
if args.get('replication_factor_max'):
  replication_factor_max = int(args.get('replication_factor_max'))
if args.get('nr_nodes'):
  nr_of_nodes = int(args.get('nr_nodes'))
if args.get('churn_int'):
  churn_interval = int(args.get('churn_int'))
if args.get('nr_phases'):
  nr_of_phases = int(args.get('nr_phases'))
if args.get('per_phase_its'):
  churn_iterations_per_phase = int(args.get('per_phase_its'))
if args.get('time_bet_phases'):
  time_between_phases = int(args.get('time_bet_phases'))
if args.get('churn_percent'):
  churn_percentage = int(args.get('churn_percent'))
if args.get('churn_log_file'):
  churn_log_file = args.get('churn_log_file')

free_database_space_on_new_node_launch = args.get('free_database')

logging.basicConfig(
  filename=churn_log_file,
  filemode='w',
  level=logging.DEBUG,
  format='%(asctime)s → %(message)s', datefmt='%m/%d/%Y %I:%M:%S %p'
)

######### Auxiliary Functions ##########

def group(pos, nr_slices):
  temp = math.ceil(nr_slices * pos)
  if temp == 0:
    temp = 1

  return temp;

def calculate_nr_of_groups():
  global nr_of_nodes, replication_factor_max, replication_factor_min

  max_bound = nr_of_nodes / replication_factor_min
  min_bound = nr_of_nodes / replication_factor_max

  if(nr_of_nodes < replication_factor_min):
    return 1

  idx = 0
  enclosed = False
  while not enclosed:
    if(min_bound <= 2 ** idx <= max_bound):
      enclosed = True
    else:
      idx += 1

  nr_groups = (2 ** idx)

  log("Nr of Groups: " + str(nr_groups))

  return nr_groups

def create_inventory_dict():
  inv = {}
  data_loader = DataLoader()
  inventory = InventoryManager(loader = data_loader,
                             sources=['hosts'])

  ansible_inv = inventory.get_groups_dict()

  if 'master' in ansible_inv:
    inv['master'] = ansible_inv['master'][0]
  if 'client' in ansible_inv:
    inv['client'] = ansible_inv['client'][0]

  return inv

def log(msg):
  logging.debug(msg)

def variables_init():
  global bootstrapper_pod_ip, nr_of_groups, nr_of_nodes, groups, groups_usage_queue, current_peer_id

  # Read from file bootstrapper pod ip 
  # (wrote by running ansible network playbook)
  with open("lsfs_network/bootstrapper_ip", "r") as f:
    bootstrapper_pod_ip = f.read().strip()

  log("Bootstrapper Pod Ip: " + bootstrapper_pod_ip)

  # Populate groups global variable according to peers created
  nr_of_groups = calculate_nr_of_groups()
  step = 1 / (nr_of_nodes - 1)
  peers_position = numpy.arange(0, 1 + step, step=step)

  for x in range(0, nr_of_nodes):
    peer_id    = x + 1
    peer_pos   = round(peers_position[x],6)
    peer_group = group(peer_pos, nr_of_groups)
    machine_nr = peer_id
    groups[peer_group].put((peer_id, peer_pos, peer_id))

  current_peer_id = nr_of_nodes

  for group_nr in range(1, nr_of_groups + 1):
    groups_usage_queue.put(group_nr)

def run_peers():
  global warmup_interval, nr_of_nodes, free_database_space_on_new_node_launch

  subprocess.Popen("sed -i \"s/nr_of_peers:.*/nr_of_peers: {}/g\" {}"
                  .format(nr_of_nodes, 'group_vars/all.yml'), shell=True
                  ).wait()

  subprocess.Popen("cp -r group_vars lsfs_network", shell=True).wait()

  nodes_config_file = "lsfs_network/files/conf.yaml"
  subprocess.Popen("sed -i \"s/warmup_interval:.*/warmup_interval: {}/g\" {}"
                .format(warmup_interval, nodes_config_file.replace("/", "\\/")), shell=True
                ).wait()

  # Create Bootstrapper and Initial Peer Pods
  subprocess.Popen("ansible-playbook lsfs_network/playbook.yml -i hosts --extra-vars \"free_database={}\"".format(free_database_space_on_new_node_launch), shell=True).wait()

  variables_init()

  print("Peers Are Up!!")

  sleep(warmup_interval)

  print("Warmup Done!!")

def setup_network():
  run_peers()
  mount_filesystem()

def kill_nodes(nodes_id, nodes_group):
  global master_addr

  print("Killing " + str(len(nodes_id)) + " node(s)")

  kill_command = "ssh {} \"kubectl delete pods".format(master_addr)

  nodes_list_str = "["
  node_group_it = 0
  for node_id in nodes_id:
    nodes_list_str += " {}({})".format(node_id, nodes_group[node_group_it]) 
    kill_command += " peer{}".format(node_id)
    node_group_it += 1
  kill_command += " --namespace=lsfs &> /dev/null &\""
  nodes_list_str += " ]"
  
  kill_process = subprocess.Popen(kill_command, shell=True)
  output, error = kill_process.communicate()

  log("Killed nodes: " + nodes_list_str)
  print("Nodes Killed")

def mount_filesystem():
  global bootstrapper_pod_ip, client_addr, mount_folder, mount_interval

  subprocess.Popen("ssh {} \"sudo umount -l {}\"".format(client_addr, mount_folder), shell=True).wait()

  subprocess.Popen("cp -r group_vars lsfs_client", shell=True).wait()

  subprocess.Popen("ansible-playbook lsfs_client/playbook.yml -i hosts --extra-vars \"bootstrapper_ip={}\"".format(bootstrapper_pod_ip), shell=True).wait()

  sleep(mount_interval)
  print("Filesystem is Mounted")

def start_nodes(new_nodes_pos, machine_nrs, nodes_group):
  global master_addr, current_peer_id, bootstrapper_pod_ip, free_database_space_on_new_node_launch, groups

  print("Moving group_vars to lsfs_add_peers")

  subprocess.Popen("cp -r group_vars lsfs_add_peers", shell=True).wait()

  new_nodes_ids = []

  nodes_list_str = "["
  for i in range(len(new_nodes_pos)):
    current_peer_id += 1
    new_nodes_ids.append(current_peer_id)
    print((current_peer_id, new_nodes_pos[i], machine_nrs[i]))
    groups[nodes_group[i]].put((current_peer_id, new_nodes_pos[i], machine_nrs[i]))
    nodes_list_str += " {}({})".format(current_peer_id, nodes_group[i]) 
  nodes_list_str += " ]"

  ansible_vars = json.dumps(
    {
      'bootstrapper_ip': bootstrapper_pod_ip,
      'nodes_id': new_nodes_ids,
      'nodes_pos': new_nodes_pos,
      'machine_nrs': machine_nrs,
      'free_database': free_database_space_on_new_node_launch
    }
    , separators=(',',':')
  )

  print("Running lsfs_add_peers playbook")

  subprocess.Popen("ansible-playbook lsfs_add_peers/playbook.yml -i hosts --extra-vars \'{}\'".format(ansible_vars), shell=True).wait()

  log("Started nodes: " + nodes_list_str)

  print("New Peers Are UP!!")

def apply_churn():
  global nr_of_nodes, churn_percentage, groups_usage_queue, groups
  
  nr_of_nodes_to_substitute = round(nr_of_nodes * (churn_percentage / 100))

  nodes_to_remove = [] # [(node_id, node_pos, machine_nr),...]
  nodes_group = []
  for i in range(nr_of_nodes_to_substitute):
    next_group = groups_usage_queue.get()
    next_node = groups[next_group].get()
    print(next_node)
    nodes_to_remove.append(next_node)
    groups_usage_queue.put(next_group)
    nodes_group.append(next_group)

  nodes_id, nodes_pos, machine_nrs = zip(*nodes_to_remove)

  print("Going to Kill nodes")

  kill_nodes(nodes_id, nodes_group)

  print("Going to Start nodes")

  start_nodes(nodes_pos, machine_nrs, nodes_group)

  print("Nodes Started")

def initiate_churn():
  global nr_of_phases, time_between_phases, churn_interval, churn_iterations_per_phase

  curr_phases = 0

  # Apply Churn Forever
  while(curr_phases < nr_of_phases):

    if curr_phases != nr_of_phases:
      # Wait Time Between Phases
      sleep(time_between_phases)
      print("Slept time between phases")


    for it in range(0, churn_iterations_per_phase):

      time_start = datetime.datetime.now()
      time_to_wait_for = time_start + datetime.timedelta(seconds=churn_interval)

      # Apply Churn
      print("Applying Churn")
      apply_churn()
      print("Churn Applied")

      if (it + 1) != churn_iterations_per_phase:
        # Wait Churn Interval
        pause.until(time_to_wait_for)
        print("Slept Churn Interval")
    
    curr_phases += 1
  print("Finished Churn")



############ Starting Point ############

def main():
  global warmup_interval, master_addr, client_addr, args
  
  #Create Inventory dictionary
  inv = create_inventory_dict()
  master_addr = inv['master']
  client_addr = inv['client']

  # Setup network
  if(args.get('init')):
    setup_network()
  else:
    variables_init()
  
  # Start Churning Phase
  initiate_churn()

if __name__ == "__main__":
  main()

# sed -i "s/int random_int = uni(random_eng);/int random_int = uni(random_eng); std::cout << random_int << std::endl;/g" dynamic_load_balancer.cpp
