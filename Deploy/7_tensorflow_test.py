from ansible.parsing.dataloader import DataLoader
from ansible.inventory.manager import InventoryManager
import subprocess
import os
import regex as re
import argparse
from datetime import date

tensorflow_dir = "/home/danielsf97/tensorflow/"
#local_dataset_dir = "/home/gsd/datasets/imagenet/tf_records"
local_dataset_dir = "/home/danielsf97/lsfs-mount/mount/tf_records"
home_dir = "/home/danielsf97/"
dataset_dir = home_dir + "datasets/imagenet/tf_records"
mount_folder = home_dir + "lsfs-mount/mount"
populate_size = 500000000 # 1GB
warmup_interval = 0
mount_interval = 50
nr_of_nodes = 1
monitoring = True

########### Default Values ###########

model = "resnet"
batch_size = 64 # ou 32
epochs = 3
shuffle_buffer = 10000
num_gpus = 0

######################################

master_addr = None
client_addr = None
run_dir = None
run_name = None

parser = argparse.ArgumentParser()
parser.add_argument('-r', '--restore', help="Restore database from snapshot",
                    action='store_true')

args = vars(parser.parse_args())

def create_inventory_dict():
  inv = {}
  data_loader = DataLoader()
  inventory = InventoryManager(loader = data_loader,
                             sources=['cluster-deploy/ansible_hosts'])

  ansible_inv = inventory.get_groups_dict()

  if 'master' in ansible_inv:
    inv['master'] = ansible_inv['master'][0]
  if 'client' in ansible_inv:
    inv['client'] = ansible_inv['client'][0]

  return inv

def setup_network():
  global mount_folder, warmup_interval, mount_interval, nr_of_nodes

  subprocess.Popen("python3 1_setup_lsfs.py --warmup_int {} --mount_int {} --mount_folder {} --nr_nodes {}"
                .format(
                  warmup_interval,
                  mount_interval,
                  mount_folder,
                  nr_of_nodes
                ), shell=True
              ).wait()

def populate_database():
  global client_addr, dataset_dir, mount_folder, populate_size, home_dir

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
                      mount_folder,
                      populate_size
                    ), shell=True
                  ).wait()

def setup_network_from_snapshot():
  global mount_folder, warmup_interval, mount_interval, nr_of_nodes

  subprocess.Popen("python3 3_setup-lsfs-from-snaphot.py "
                  "--warmup_int {} "
                  "--mount_int {} "
                  "--nr_nodes {} "
                  "--mount_folder {}"
                .format(
                  warmup_interval,
                  mount_interval,
                  nr_of_nodes,
                  mount_folder
                ), shell=True
              ).wait()

def setup_test_configs():
  global tensorflow_dir, local_dataset_dir, client_addr

  # On Remote Server
  script_path = tensorflow_dir + "train-official-model.sh"
  transf_tensorflow_dir = tensorflow_dir.replace("/", "\\/")
  command1 = "ssh {} \"sed -i \'s/=\\\".*\/tensorflow\//\=\\\"{}/g\' {}\"".format(client_addr, transf_tensorflow_dir, script_path)
  transf_dataset_dir = local_dataset_dir.replace("/", "\\/")
  command2 = "ssh {} \"sed -i \'s/LOCAL_DATASET_DIR=\\\".*\\\"/LOCAL_DATASET_DIR=\\\"{}\\\"/g\' {}\"".format(client_addr, transf_dataset_dir, script_path)

  subprocess.Popen(command1, shell=True).wait()
  subprocess.Popen(command2, shell=True).wait()

  # Locally
  start_monitoring_ansible_config = "monitoring/start-monitoring/group_vars/all.yml"
  resources_dir = tensorflow_dir + "resources"

  subprocess.Popen("sed -i \"s/resources_dir:.*/resources_dir: \\\"{}\\\"/g\" {}"
                  .format(resources_dir.replace("/", "\\/"), start_monitoring_ansible_config.replace("/", "\\/")), shell=True
                  ).wait()


def create_results_directory():
  global client_addr, model, batch_size, epochs, tensorflow_dir, run_dir, run_name

  results_dir = tensorflow_dir + "results"
  day = date.today().strftime("%Y_%m_%d")

  # Create results directory
  run_name = "{}-bs{}-ep{}-{}".format(model, batch_size, epochs, day)
  run_dir = "{}/{}".format(results_dir, run_name)
  command = "ssh {} \"mkdir -p {}\"".format(client_addr, run_dir)
  subprocess.Popen(command, shell=True).wait()

def start_monitoring():
  global run_name, monitoring

  if(not monitoring):
     return

  subprocess.Popen("ansible-playbook monitoring/start-monitoring/playbook.yml -i hosts --extra-vars \"run_name={}\"".format(run_name), shell=True).wait()

def stop_monitoring():
  global run_name, monitoring

  if(not monitoring):
     return

  subprocess.Popen("ansible-playbook monitoring/stop-monitoring/playbook.yml -i hosts --extra-vars \"run_name={}\"".format(run_name), shell=True).wait()

def tensorflow_training():
  global model, batch_size, epochs, shuffle_buffer, num_gpus, tensorflow_dir, client_addr

  subprocess.Popen("ssh {} \"{}./train-official-model.sh run "
                   "-m {} "
                   "-b {} "
                   "-e {} "
                   "-s {} "
                   "-g {} "
                   "\""
                  .format(
                    client_addr,
                    tensorflow_dir,
                    model,
                    batch_size,
                    epochs,
                    shuffle_buffer,
                    num_gpus
                  ), shell=True
                ).wait()

def retrieve_results():
  global client_addr, run_dir, run_name

  subprocess.Popen("scp -r {}:{} results/"
                     .format(client_addr, run_dir), shell=True
                  ).wait()

def main():
  global master_addr, client_addr
  
  #Create Inventory dictionary
  inv = create_inventory_dict()
  master_addr = inv['master']
  client_addr = inv['client']

  # Setup network and populate
  # if(args.get('restore')):
  #   setup_network_from_snapshot()
  # else:
  #   setup_network()
  #   populate_database()

  # setup_test_configs()

  # create_results_directory()

  # start_monitoring()

  tensorflow_training()

  # stop_monitoring()

  # retrieve_results()

if __name__ == "__main__":
  main()

# Se estiver a ter dificuldades a correr o tensorflow, 
# tentar instalar o cuda 10.1 através do exemplo do seguinte site:
# https://medium.com/@exesse/cuda-10-1-installation-on-ubuntu-18-04-lts-d04f89287130
# Atenção só ao export que tem de ser 
#  export PATH=/usr/local/cuda/bin${PATH:+:${PATH}} e não
#  export PATH=/usr/local/cuda-10.1/bin${PATH:+:${PATH}}