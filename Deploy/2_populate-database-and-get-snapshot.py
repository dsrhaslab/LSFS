import configparser
from ansible.parsing.dataloader import DataLoader
from ansible.inventory.manager import InventoryManager
from time import sleep
import subprocess
import os
import regex as re

master_addr = None
client_addr = None
home_dir = "/home/danielsf97/"
dataset_dir = home_dir + "datasets_reduced_v2/imagenet/tf_records"
lsfs_dir = home_dir + "lsfs-mount/mount"
populate_size = 100000000000 # 1GB

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

# def populate_database():
#   global client_addr

#   full_file_path = "workloads-filebench/write-data-micro/seq-write-1th-4k.f"

#   print("Running filebench")

#   subprocess.Popen("ssh {} \"sudo sysctl kernel.randomize_va_space=0 && \
#                     filebench -f {}\""
#                     .format(
#                       client_addr,
#                       full_file_path
#                     ), shell=True
#                   ).wait()

def populate_database():
  global client_addr, home_dir, populate_size, dataset_dir, lsfs_dir

  subprocess.Popen("scp file_comparing.py {}:{}"
                .format(
                  client_addr,
                  home_dir
                ), shell=True
              ).wait()

  subprocess.Popen("ssh {} \"python3 file_comparing.py {} {} -b -n 4 -s {}\""
                    .format(
                      client_addr,
                      dataset_dir,
                      lsfs_dir,
                      populate_size
                    ), shell=True
                  ).wait()

def make_snapshot():
  subprocess.Popen("cp -r group_vars lsfs_snapshot", shell=True).wait()

  ansible_command = ["ansible-playbook", "lsfs_snapshot/playbook.yml", "-i", "hosts"]
  ansible_process = subprocess.Popen(ansible_command)
  output, error = ansible_process.communicate()

############ Starting Point ############

def main():
  global master_addr, client_addr
  
  # Create Inventory dictionary
  inv = create_inventory_dict()
  master_addr = inv['master']
  client_addr = inv['client']

  populate_database()

  # # Start Churning Phase
  #make_snapshot()

if __name__ == "__main__":
  main()


