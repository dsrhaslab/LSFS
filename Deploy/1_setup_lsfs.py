import configparser
from ansible.parsing.dataloader import DataLoader
from ansible.inventory.manager import InventoryManager
from time import sleep
import subprocess
import os
import regex as re
import argparse

warmup_interval = 120
mount_interval = 120
nr_of_nodes = 4
mount_folder = "/home/danielsf97/lsfs-mount/mount"
master_addr = None
client_addr = None
bootstrapper_ip = None
free_database_peer_startup = False

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
parser.add_argument('--warmup_int', help="Warmup Interval")
parser.add_argument('--mount_int', help="Mount Interval")
parser.add_argument('--mount_folder', help="Mount Folder", type = str)
parser.add_argument('--nr_nodes', help="Number of Nodes")
parser.add_argument('--free_database', help="Free database on nodes startup", type = str2bool, const=True, default=free_database_peer_startup, nargs='?')

args = vars(parser.parse_args())

if args.get('warmup_int'):
  warmup_interval = int(args.get('warmup_int'))
if args.get('mount_int'):
  mount_interval = int(args.get('mount_int'))
if args.get('mount_folder'):
  mount_folder = args.get('mount_folder')
if args.get('nr_nodes'):
  nr_of_nodes = int(args.get('nr_nodes'))

free_database_peer_startup = args.get('free_database')

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

def run_peers():
  global bootstrapper_ip, nr_of_nodes, warmup_interval, free_database_peer_startup

  subprocess.Popen("sed -i \"s/nr_of_peers:.*/nr_of_peers: {}/g\" {}"
                  .format(nr_of_nodes, 'group_vars/all.yml'), shell=True
                  ).wait()

  subprocess.Popen("cp -r group_vars lsfs_network", shell=True).wait()

  nodes_config_file = "lsfs_network/files/conf.yaml"
  subprocess.Popen("sed -i \"s/warmup_interval:.*/warmup_interval: {}/g\" {}"
                .format(warmup_interval, nodes_config_file.replace("/", "\\/")), shell=True
                ).wait()

  subprocess.Popen("ansible-playbook lsfs_network/playbook.yml -i hosts --extra-vars \"free_database={}\"".format(free_database_peer_startup), shell=True).wait()

  with open("lsfs_network/bootstrapper_ip", "r") as f:
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

############ Starting Point ############

def main():
  global master_addr, client_addr

  # Create Inventory dictionary
  inv = create_inventory_dict()
  master_addr = inv['master']
  client_addr = inv['client']

  # Run LSFS Peers
  run_peers()

  # Mount Filesystem
  mount_filesystem()

if __name__ == "__main__":
  main()