import configparser
from ansible.parsing.dataloader import DataLoader
from ansible.inventory.manager import InventoryManager
from time import sleep
import subprocess
import os
import regex as re
import argparse
import json

warmup_interval = 120
mount_interval = 120
mount_interval = 60
nr_of_nodes = 100
replication_factor_min = 5
replication_factor_max = 10
mount_folder = "/home/danielsf97/lsfs-mount/mount"
bucket_folder_name = "100nodes_filebench"
master_addr = None
client_addr = None
bootstrapper_ip = None
group_database = True

####### Global Dynamic Variables #######

nr_of_groups = None
peer_groups = []

parser = argparse.ArgumentParser()
parser.add_argument('--warmup_int', help="Warmup Interval")
parser.add_argument('--mount_int', help="Mount Interval")
parser.add_argument('--mount_folder', help="Mount Folder")
parser.add_argument('--nr_nodes', help="Number of Nodes")
parser.add_argument('--replication_factor_min', help="Replication factor min")
parser.add_argument('--replication_factor_max', help="Replication factor max")

args = vars(parser.parse_args())

if args.get('warmup_int'):
  warmup_interval = args.get('warmup_int')
if args.get('mount_int'):
  mount_interval = args.get('mount_int')
if args.get('nr_nodes'):
  nr_of_nodes = args.get('nr_nodes')
if args.get('mount_folder'):
  mount_folder = args.get('mount_folder')
if args.get('replication_factor_min'):
  replication_factor_min = int(args.get('replication_factor_min'))
if args.get('replication_factor_max'):
  replication_factor_max = int(args.get('replication_factor_max'))

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

def setup_network_from_snapshot():
  global bootstrapper_ip, warmup_interval, nr_of_nodes, group_database, peer_groups

  if group_database:
    ansible_scripts_path = "lsfs_network-from-group-snapshot"
  else:
    ansible_scripts_path = "lsfs_network-from-snapshot"

  subprocess.Popen("sed -i \"s/nr_of_peers:.*/nr_of_peers: {}/g\" {}"
                  .format(nr_of_nodes, 'group_vars/all.yml'), shell=True
                  ).wait()

  subprocess.Popen("cp -r group_vars {}".format(ansible_scripts_path), shell=True).wait()

  nodes_config_file = "{}/files/conf.yaml".format(ansible_scripts_path)
  subprocess.Popen("sed -i \"s/warmup_interval:.*/warmup_interval: {}/g\" {}"
                .format(warmup_interval, nodes_config_file.replace("/", "\\/")), shell=True
                ).wait()

  if group_database:
      ansible_vars = json.dumps(
        {
          'folder_name': bucket_folder_name,
          'peer_groups': peer_groups
        }
        , separators=(',',':')
      )

    subprocess.Popen("ansible-playbook lsfs_network-from-group-snapshot/playbook.yml -i hosts --extra-vars \'{}\'".format(ansible_vars), shell=True).wait()
  else:
    subprocess.Popen("ansible-playbook lsfs_network-from-snapshot/playbook.yml -i hosts --extra-vars \"folder_name={}\"".format(bucket_folder_name), shell=True).wait()

  with open("{}/bootstrapper_ip".format(ansible_scripts_path), "r") as f:
    bootstrapper_ip = f.read().strip()

  print("Peers Are Up!!")
  sleep(warmup_interval)
  print("Peers Converged to Right Number of Groups!!")

def mount_filesystem():
  global bootstrapper_ip, client_addr, mount_interval

  subprocess.Popen("ssh {} \"sudo umount -l {}\"".format(client_addr, mount_folder), shell=True).wait()

  subprocess.Popen("cp -r group_vars lsfs_client", shell=True).wait()

  subprocess.Popen("ansible-playbook lsfs_client/playbook.yml -i hosts --extra-vars \"bootstrapper_ip={}\"".format(bootstrapper_ip), shell=True).wait()

  print("Filesystem is Mounted")
  sleep(mount_interval)

def variables_init():
  global nr_of_groups, nr_of_nodes, peer_groups

  # Populate groups global variable according to peers created
  nr_of_groups = calculate_nr_of_groups()
  step = 1 / (nr_of_nodes - 1)
  peers_position = numpy.arange(0, 1 + step, step=step)

  for x in range(0, nr_of_nodes):
    peer_id    = x + 1
    peer_pos   = round(peers_position[x],6)
    peer_group = group(peer_pos, nr_of_groups)
    machine_nr = peer_id
    peer_groups.append(peer_group)

############ Starting Point ############

def main():
  global master_addr, client_addr

  # Create Inventory dictionary
  inv = create_inventory_dict()
  master_addr = inv['master']
  client_addr = inv['client']

  # Variables initialization
  variables_init()

  # Start Churning Phase
  setup_network_from_snapshot()

  # Mount Filesystem
  mount_filesystem()

if __name__ == "__main__":
  main()
