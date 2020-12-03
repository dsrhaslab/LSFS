import configparser
from ansible.parsing.dataloader import DataLoader
from ansible.inventory.manager import InventoryManager
from time import sleep
import subprocess
import os
import regex as re
import argparse

nr_of_nodes = 500

def machine_creation():
  global nr_of_nodes

  subprocess.Popen("sed -i \"s/nr_of_peers:.*/nr_of_peers: {}/g\" {}"
                    .format(nr_of_nodes, 'group_vars/all.yml'), shell=True
                  ).wait()

  subprocess.Popen("cp -r group_vars cluster-deploy-snapshot", shell=True).wait()

  ansible_command = ["ansible-playbook", "cluster-deploy-snapshot/playbook.yml"]
  ansible_process = subprocess.Popen(ansible_command)
  output, error = ansible_process.communicate()

  subprocess.Popen("cp cluster-deploy-snapshot/ansible_hosts hosts", shell=True).wait()

  print("Test Runner Instance Created - Machine Creation Running on Background")

############ Starting Point ############

def main():

  # Send Workloads to Client Node
  machine_creation()

if __name__ == "__main__":
  main()
