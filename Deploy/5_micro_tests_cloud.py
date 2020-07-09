import configparser
from ansible.parsing.dataloader import DataLoader
from ansible.inventory.manager import InventoryManager
from time import sleep
import subprocess
import os
import regex as re

# Default values
warmup_interval = 120
mount_interval = 60
nr_of_nodes = 4
time_between_runs = 300
nfs = False
fuse_nfs = False
peers_active = True
free_database_peer_startup = True
monitoring = True
home_folder = "/home/danielsf97"
mount_folder = "/home/danielsf97/lsfs-mount/mount"
results_folder = "/home/danielsf97/results"
results_local_folder = 'results'

master_addr = None
client_addr = None
bootstrapper_ip = None

workloads_escrita = [
  "seq-write-1th-4k.f",
  "seq-write-1th-32k.f",
  "seq-write-1th-128k.f",
  "seq-write-1th-1024k.f",
  "seq-write-16th-16f-4k.f",
  "seq-write-16th-16f-32k.f",
  "seq-write-16th-16f-128k.f",
  "seq-write-16th-16f-1024k.f"
]

workloads_leitura_1f = [
  "seq-read-1th-4kdf1.f",
  "seq-read-1th-32kdf1.f",
  "seq-read-1th-128kdf1.f",
  "seq-read-1th-1024kdf1.f",
  "rand-read-1th-4kdf1.f",
  "rand-read-1th-32kdf1.f",
  "rand-read-1th-128kdf1.f",
  "rand-read-1th-1024kdf1.f"
]

workloads_leitura_16f = [
  "seq-read-16th-16f-4kdf1.f",
  "seq-read-16th-16f-32kdf1.f",
  "seq-read-16th-16f-128kdf1.f",
  "seq-read-16th-16f-1024kdf1.f",
  "rand-read-16th-16f-4kdf1.f",
  "rand-read-16th-16f-32kdf1.f",
  "rand-read-16th-16f-128kdf1.f",
  "rand-read-16th-16f-1024kdf1.f"
]

def nfs_like_filesystem():
    global nfs, fuse_nfs
    return (nfs or fuse_nfs)

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

def cleanup():
  global client_addr

  subprocess.Popen("ssh {} \"sudo umount -l {}\"".format(client_addr, mount_folder), shell=True).wait()

def run_peers():
  global bootstrapper_ip, warmup_interval, nr_of_nodes

  subprocess.Popen("sed -i \"s/nr_of_peers:.*/nr_of_peers: {}/g\" {}"
                  .format(nr_of_nodes, 'group_vars/all.yml'), shell=True
                  ).wait()

  subprocess.Popen("cp -r group_vars lsfs_network", shell=True).wait()

  nodes_config_file = "lsfs_network/files/conf.yaml"
  subprocess.Popen("sed -i \"s/warmup_interval:.*/warmup_interval: {}/g\" {}"
                .format(warmup_interval, nodes_config_file.replace("/", "\\/")), shell=True
                ).wait()

  subprocess.Popen("ansible-playbook lsfs_network/playbook.yml -i hosts --extra-vars \"free_database={}\"".format(free_database_peer_startup), shell=True).wait()

  # ansible_command = ["ansible-playbook", "lsfs_network/playbook.yml", "-i", "hosts"]
  # ansible_process = subprocess.Popen(ansible_command)
  # output, error = ansible_process.communicate()

  with open("lsfs_network/bootstrapper_ip", "r") as f:
    bootstrapper_ip = f.read().strip()

  print("Peers Are Up!!")
  sleep(warmup_interval)
  print("Peers Converged to Right Number of Groups!!")

def mount_filesystem():
  global bootstrapper_ip

  subprocess.Popen("cp -r group_vars lsfs_client", shell=True).wait()

  subprocess.Popen("ansible-playbook lsfs_client/playbook.yml -i hosts --extra-vars \"bootstrapper_ip={}\"".format(bootstrapper_ip), shell=True).wait()

  sleep(mount_interval)
  print("Filesystem is Mounted")
  

def kill_processes():
  global master_addr

  kill_command = "ssh {} \"kubectl delete --all pods --namespace=lsfs\"".format(master_addr)
  kill_process = subprocess.Popen(kill_command, shell=True)
  output, error = kill_process.communicate()
  print("Processes Killed")

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

def run_filebench(file_name):
  global client_addr, results_folder

  match = re.search(r'write', file_name)
  if match:
      full_file_path = "workloads-filebench/write-data-micro/" + file_name

  match = re.search(r'read', file_name)
  if match:
      full_file_path = "workloads-filebench/read-data-micro/" + file_name

  subprocess.Popen("ssh {} \"mkdir -p {}\""
                    .format(client_addr,results_folder), shell=True
                  ).wait()

  print("Running filebench - " + file_name)

  results_file_path = results_folder + "/" + file_name[:-2] + ".txt"
  subprocess.Popen("ssh {} \"sudo sysctl kernel.randomize_va_space=0 && \
                    filebench -f {} > {}\""
                    .format(
                      client_addr,
                      full_file_path,
                      results_file_path 
                    ), shell=True
                  ).wait()

  subprocess.Popen("scp {}:{} {}"
                    .format(client_addr, results_file_path, results_local_folder), shell=True
                ).wait()
  
def write_1_file():
    cleanup()
    run_peers()
    mount_filesystem()
    run_filebench("seq-write-1f-4k-write1.f")

def write_16_files():
    cleanup()
    run_peers()
    mount_filesystem()
    run_filebench("seq-write-16th-16f-4k-write16.f")

def start_monitoring(run_name):
  global monitoring

  if(not monitoring):
     return

  subprocess.Popen("ansible-playbook monitoring/start-monitoring/playbook.yml -i hosts --extra-vars \"run_name={}\"".format(run_name), shell=True).wait()

def stop_monitoring(run_name):
  global monitoring

  if(not monitoring):
     return

  subprocess.Popen("ansible-playbook monitoring/stop-monitoring/playbook.yml -i hosts --extra-vars \"run_name={}\"".format(run_name), shell=True).wait()

def clear_caches():
  subprocess.Popen("ssh {} \"sudo sync && \
                    echo 3 > sudo /proc/sys/vm/drop_caches\""
                    , shell=True
                  ).wait()

def ensure_directory(directory):
  if not os.path.exists(directory):
    os.makedirs(directory)

############ Starting Point ############

def main():
  global master_addr, client_addr, workloads_escrita, workloads_leitura_16f, workloads_leitura_1f, time_between_runs, results_local_folder

  ensure_directory(results_local_folder)

  if nfs_like_filesystem():
    pass
    # for file in ${workloads_escrita[@]}; do
    #     mount_nfs_like_filesystem
    #     run_filebench $file
    #     wait $FILEBENCH_PROCESS
    #     cleanup_nfs_like_filesystem
    #     sleep $TIME_BETWEEN_RUNS
    # done
  else:

    # Create Inventory dictionary
    inv = create_inventory_dict()
    master_addr = inv['master']
    client_addr = inv['client']

    # Send Workloads to Client Node
    send_workloads_to_server()

    for file in workloads_escrita:
      cleanup()
      run_peers()
      mount_filesystem()
      start_monitoring(file[:-2])
      run_filebench(file)
      stop_monitoring(file[:-2])
      kill_processes()
      sleep(time_between_runs)

    write_1_file()
    for file in workloads_leitura_1f:
      sleep(time_between_runs)
      clear_caches()
      start_monitoring(file[:-2])
      run_filebench(file)
      stop_monitoring(file[:-2])

    write_16_files()
    for file in workloads_leitura_16f:
      sleep(time_between_runs)
      clear_caches()
      start_monitoring(file[:-2])
      run_filebench(file)
      stop_monitoring(file[:-2])

if __name__ == "__main__":
  main()
