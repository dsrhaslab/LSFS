from ansible.parsing.dataloader import DataLoader
from ansible.inventory.manager import InventoryManager
import subprocess
import os
import signal
import regex as re
import argparse
from datetime import date
from datetime import datetime
import json
import sys
import psutil

tensorflow_dir = "/home/danielsf97/tensorflow/"
#local_dataset_dir = "/home/danielsf97/datasets_reduced/imagenet/tf-records"
local_dataset_dir = "/home/danielsf97/lsfs-mount/mount"
home_dir = "/home/danielsf97/"
dataset_dir = home_dir + "datasets/imagenet/tf_records"
mount_folder = home_dir + "lsfs-mount/mount"
local_filesystem = True
results_local_folder = 'results'
populate_size = 500000000 # 1GB
warmup_interval = 0
mount_interval = 50
nr_of_nodes = 15
monitoring = True
try_nr = 1
log_folder_name = "testing-15nodes"
send_results_to_bucket = True

########### Churn Values #############

nr_of_phases                = 1
churn_iterations_per_phase  = sys.maxsize
time_between_phases         = 5*60
free_database_peer_startup  = True
churn_interval              = 5 * 60 # 5min/5min
churn_percentage            = 20 # percentage
replication_factor_min      = 3
replication_factor_max      = 5
churn_pid                   = None

########### Default Values ###########

model          = "lenet"
batch_size     = 32
epochs         = 1
shuffle_buffer = 10000
num_gpus       = 1

######################################

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
                             sources=['hosts'])

  ansible_inv = inventory.get_groups_dict()

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
  day = datetime.now().strftime("%Y_%m_%d-%H_%M")

  # Create results directory
  run_name = "{}-bs{}-ep{}-{}".format(model, batch_size, epochs, day)
  run_dir = "{}/{}".format(results_dir, run_name)
  command = "ssh {} \"mkdir -p {}\"".format(client_addr, run_dir)
  subprocess.Popen(command, shell=True).wait()

  script_path = tensorflow_dir + "train-official-model.sh"
  transf_run_dir = run_dir.replace("/", "\\/")
  command1 = "ssh {} \"sed -i \'s/RUN_NAME=\\\".*\\\"/RUN_NAME=\\\"{}\\\"/g\' {}\"".format(client_addr, run_name, script_path)
  command2 = "ssh {} \"sed -i \'s/RUN_DIR=\\\".*\\\"/RUN_DIR=\\\"{}\\\"/g\' {}\"".format(client_addr, transf_run_dir, script_path)

  subprocess.Popen(command1, shell=True).wait()
  subprocess.Popen(command2, shell=True).wait()

def start_monitoring():
  global monitoring, run_name

  if(not monitoring):
     return

  extra_vars = json.dumps(
    {
      'run_name': run_name,
      'gpu': True 
    }
    , separators=(',',':')
  )

  subprocess.Popen("ansible-playbook monitoring/start-monitoring/playbook.yml -i hosts --extra-vars \'{}\'".format(extra_vars), shell=True).wait()

def stop_monitoring():
  global run_name, monitoring

  if(not monitoring):
     return

  subprocess.Popen("ansible-playbook monitoring/stop-monitoring/playbook.yml -i hosts --extra-vars \"run_name={} try_nr={} folder_name={}\"".format(run_name, try_nr, log_folder_name), shell=True).wait()

def introduce_churn_in_background():
    global churn_pid, run_dir

    churn_log_file = "churn.log" 

    churn_pid = subprocess.Popen("python3 4_setup_lsfs_with_churn.py "
                                    "--nr_nodes {} "
                                    "--replication_factor_min {} "
                                    "--replication_factor_max {} "
                                    "--churn_int {} "
                                    "--nr_phases {} "
                                    "--per_phase_its {} "
                                    "--time_bet_phases {} "
                                    "--churn_percent {} "
                                    "--free_database {} "
                                    "--churn_log_file {} "
                                    .format(
                                        nr_of_nodes,
                                        replication_factor_min,
                                        replication_factor_max,
                                        churn_interval,
                                        nr_of_phases,
                                        churn_iterations_per_phase,
                                        time_between_phases,
                                        churn_percentage,
                                        free_database_peer_startup,
                                        churn_log_file
                                    ), shell=True
                                ).pid

def kill_churn_process():
    global churn_pid

    parent = psutil.Process(churn_pid)
    for child in parent.children(recursive=True):  # or parent.children() for recursive=False
        child.kill()
    parent.kill()

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
  global client_addr, run_dir, run_name, results_local_folder

  subprocess.Popen("scp -r {}:{} {}/"
                     .format(client_addr, run_dir, results_local_folder), shell=True
                  ).wait()

  if send_results_to_bucket:
    local_results_file_path = results_local_folder + "/" + run_name
    bucket_folder_path = "gs://lsfs_bucket/logs/" + log_folder_name + "/" +run_name + "/" + str(try_nr) + "/"
    subprocess.Popen("gsutil cp -r {}/* {}"
                      .format(local_results_file_path, bucket_folder_path), shell=True
                     ).wait()

def main():
  global client_addr
  
  #Create Inventory dictionary
  inv = create_inventory_dict()
  client_addr = inv['client']

  # Setup network and populate
  #if not local_filesystem:
  #  if(args.get('restore')):
  #    setup_network_from_snapshot()
  #  else:
  #    setup_network()
  #    populate_database()

  setup_test_configs()

  create_results_directory()

  start_monitoring()

  introduce_churn_in_background()

  tensorflow_training()

  kill_churn_process()

  stop_monitoring()

  retrieve_results()

if __name__ == "__main__":
  main()

# Se estiver a ter dificuldades a correr o tensorflow, 
# tentar instalar o cuda 10.1 através do exemplo do seguinte site:
# https://medium.com/@exesse/cuda-10-1-installation-on-ubuntu-18-04-lts-d04f89287130
# Atenção só ao export que tem de ser 
#  export PATH=/usr/local/cuda/bin${PATH:+:${PATH}} e não
#  export PATH=/usr/local/cuda-10.1/bin${PATH:+:${PATH}}
