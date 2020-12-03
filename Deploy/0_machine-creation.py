import configparser
from ansible.parsing.dataloader import DataLoader
from ansible.inventory.manager import InventoryManager
from time import sleep
import subprocess
import os
import regex as re
import argparse

nr_of_nodes = 10
client_addr = None
home_folder = "/home/danielsf97"
mount_folder = "/home/danielsf97/lsfs-mount/mount"

parser = argparse.ArgumentParser()
parser.add_argument('--nr_nodes', help="Number of Nodes")

args = vars(parser.parse_args())

if args.get('nr_nodes'):
  nr_of_nodes = int(args.get('nr_nodes'))

def gcloud_create_machines():
  global nr_of_nodes

  subprocess.Popen("sed -i \"s/nr_of_peers:.*/nr_of_peers: {}/g\" {}"
                    .format(nr_of_nodes, 'group_vars/all.yml'), shell=True
                  ).wait()

  subprocess.Popen("cp -r group_vars cluster-deploy-snapshot", shell=True).wait()

  ansible_command = ["ansible-playbook", "cluster-deploy-snapshot/setup-machines.yml"]
  ansible_process = subprocess.Popen(ansible_command)
  output, error = ansible_process.communicate()

  subprocess.Popen("cp cluster-deploy-snapshot/ansible_hosts hosts", shell=True).wait()

  print("Machines Created")

def send_workloads_to_server():
  global mount_folder, home_folder, client_addr

  temp = mount_folder.split("/")
  mount_folder_sed = '\\/'.join(temp)

  for workload_type in ["read-data-micro", "write-data-micro"]:
    workload_path = "workloads-filebench/" + workload_type
    for file_name in os.listdir(workload_path):
      full_file_path = os.path.join(workload_path, file_name)

      subprocess.Popen("sed -i \"s/path=\\\".*\\\"/path=\\\"{}\\\"/g\" {}"
                        .format(mount_folder_sed, full_file_path), shell=True
                       ).wait()

  subprocess.Popen("scp -r workloads-filebench/ {}:{}"
                     .format(client_addr,home_folder), shell=True
                  ).wait()

def create_inventory_dict():
  inv = {}
  data_loader = DataLoader()
  inventory = InventoryManager(loader = data_loader,
                             sources=['hosts'])

  ansible_inv = inventory.get_groups_dict()

  if 'client' in ansible_inv:
    inv['client'] = ansible_inv['client'][0]

  return inv

############ Starting Point ############

def main():
  global client_addr

  # Gcloud create machines
  gcloud_create_machines()

  # Create Inventory dictionary
  inv = create_inventory_dict()
  client_addr = inv['client']

  # Send Workloads to Client Node
  send_workloads_to_server()

if __name__ == "__main__":
  main()

