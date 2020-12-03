from ansible.parsing.dataloader import DataLoader
from ansible.inventory.manager import InventoryManager
import subprocess
import os
import regex as re
import argparse
import json

client_addr = None
nr_of_nodes = 500
replication_factor_min = 2
replication_factor_max = 3
churn_interval   = 60
churn_iterations_per_phase = 2
time_between_phases = 120
nr_of_phases = 2
churn_percentage = 25 # por cento
warmup_interval = 120
mount_interval = 120
home_dir = "/home/danielsf97/"
dataset_dir = home_dir + "datasets/imagenet/tf_records"
mount_folder = home_dir + "lsfs-mount/mount"
results_folder = "/home/danielsf97/results"
results_local_folder = 'results'
populate_size = 500000000 # 1GB
monitoring = True
free_database_peer_startup = True
try_nr = 1
log_folder_name = "10_nodes_write32_read32"
send_results_to_bucket = True

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

  if 'master' in ansible_inv:
    inv['master'] = ansible_inv['master'][0]
  if 'client' in ansible_inv:
    inv['client'] = ansible_inv['client'][0]

  return inv

def setup_network():
  global mount_folder, warmup_interval, mount_interval, client_addr, free_database_peer_startup

  subprocess.Popen("ssh {} \"sudo umount -l {}\"".format(client_addr, mount_folder), shell=True).wait()

  subprocess.Popen("python3 1_setup_lsfs.py --warmup_int {} --mount_int {} --mount_folder {} --nr_nodes {} --free_database {}"
                .format(
                  warmup_interval,
                  mount_interval,
                  mount_folder,
                  nr_of_nodes,
                  free_database_peer_startup
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

  subprocess.Popen("ssh {} \"mkdir -p {}; /snap/bin/gsutil -m cp gs://lsfs_bucket/tf_records/* {}\""
                  .format(
                    client_addr,
                    dataset_dir,
                    dataset_dir
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
  global mount_folder, warmup_interval, mount_interval, nr_of_nodes, client_addr

  subprocess.Popen("ssh {} \"sudo umount -l {}\"".format(client_addr, mount_folder), shell=True).wait()


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

def introduce_churn():
  global warmup_interval, nr_of_nodes, churn_interval, \
    nr_of_phases, churn_iterations_per_phase, time_between_phases, \
    churn_percentage, mount_interval, free_database_peer_startup, \
    replication_factor_min, replication_factor_max

  subprocess.Popen("python3 4_setup_lsfs_with_churn.py "
                   "--warmup_int {} "
                   "--nr_nodes {} "
                   "--replication_factor_min {} "
                   "--replication_factor_max {} "
                   "--churn_int {} "
                   "--nr_phases {} "
                   "--per_phase_its {} "
                   "--time_bet_phases {} "
                   "--churn_percent {} "
                   "--free_database {} "
                  .format(
                    mount_interval,
                    nr_of_nodes,
                    replication_factor_min,
                    replication_factor_max,
                    churn_interval,
                    nr_of_phases,
                    churn_iterations_per_phase,
                    time_between_phases,
                    churn_percentage,
                    free_database_peer_startup
                  ), shell=True
                ).wait()

def evaluate_resilience():
  global client_addr, dataset_dir, mount_folder, populate_size

  subprocess.Popen("ssh {} \"mkdir -p {}\""
                    .format(client_addr,results_folder), shell=True
                  ).wait()

  results_file_path = results_folder + "/resilience_test.txt"
  subprocess.Popen("ssh {} \"python3 file_comparing.py {} {} -c -n 4 -s {} > {}\""
                    .format(
                      client_addr,
                      dataset_dir,
                      mount_folder,
                      populate_size,
                      results_file_path
                    ), shell=True
                  ).wait()

  local_results_path = results_local_folder + "/" + log_folder_name + "/resilience_test/" + str(try_nr) + "/"
  ensure_directory(local_results_path)
  subprocess.Popen("scp {}:{} {}"
                    .format(client_addr, results_file_path, local_results_path), shell=True
                ).wait()

  if send_results_to_bucket:
    local_results_file_path = local_results_path + "resilience_test.txt"
    bucket_folder_path = "gs://lsfs_bucket/logs/" + log_folder_name + "/resilience_test/" + str(try_nr) + "/"
    subprocess.Popen("gsutil cp {} {}"
                      .format(local_results_file_path, bucket_folder_path), shell=True
                     ).wait()

def start_monitoring(run_name):
  global monitoring

  if(not monitoring):
     return

  extra_vars = json.dumps(
    {
      'run_name': "resilience_test",
      'gpu': False 
    }
    , separators=(',',':')
  )

  subprocess.Popen("ansible-playbook monitoring/start-monitoring/playbook.yml -i hosts --extra-vars \'{}\'".format(extra_vars), shell=True).wait()
  #subprocess.Popen("ansible-playbook monitoring/start-monitoring/playbook.yml -i hosts --extra-vars \"run_name=resilience_test\"", shell=True).wait()

def stop_monitoring():
    global run_name, monitoring

  if(not monitoring):
     return

  subprocess.Popen("ansible-playbook monitoring/stop-monitoring/playbook.yml -i hosts --extra-vars \"run_name=resilience_test try_nr={} folder_name={}\""
                    .format(try_nr, log_folder_name)
                   , shell=True).wait()

############ Starting Point ############

def main():
  global master_addr, client_addr
  
  #Create Inventory dictionary
  inv = create_inventory_dict()
  master_addr = inv['master']
  client_addr = inv['client']

  # Setup network and populate
  if(args.get('restore')):
    setup_network_from_snapshot()
    start_monitoring()
  else:
    setup_network()
    start_monitoring()
    populate_database()

  # # Introduce Churn
  introduce_churn()

  # Check if all populated files match original files
  evaluate_resilience()

  stop_monitoring()

if __name__ == "__main__":
  main()
