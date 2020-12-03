import configparser
from ansible.parsing.dataloader import DataLoader
from ansible.inventory.manager import InventoryManager
from time import sleep
import subprocess
import os
import regex as re
import json

# Default values
warmup_interval = 120
mount_interval = 60
nr_of_nodes = 100
time_between_runs = 0
nfs = False
fuse_nfs = False
peers_active = True
free_database_peer_startup = False
monitoring = True
home_folder = "/home/danielsf97"
mount_folder = "/home/danielsf97/lsfs-mount/mount"
backend_folder = "/home/danielsf97/lsfs-mount/backend"
results_folder = "/home/danielsf97/results"
results_local_folder = 'results'
local_filesystem = True
fuse_filesystem = False
should_create_machines = False
should_destroy_machines = False
log_folder_name = "local_g1-small_ssd15_v2"
send_results_to_bucket = True

master_addr = None
client_addr = None
bootstrapper_ip = None

workloads_escrita = [
  #("seq-write-1th-4k.f",[1,2,3]),
  ("seq-write-1th-32k.f",[2,3])
  # "seq-write-1th-128k.f",
  # "seq-write-1th-1024k.f",
  #("seq-write-16th-16f-4k.f",[1,2,3]),
  # "seq-write-16th-16f-32k.f",
  # "seq-write-16th-16f-128k.f",
  # "seq-write-16th-16f-1024k.f"
]

workloads_leitura_1f = [
  #("seq-read-1th-32kdf1.f",[1,2,3]),
  #("seq-read-1th-4kdf1.f",[1,2,3]),
  #("seq-read-1th-32kdf1.f",[1,3]),
  # "seq-read-1th-128kdf1.f",
  # "seq-read-1th-1024kdf1.f",
  #("rand-read-1th-4kdf1.f",[1,2,3]),
  #("rand-read-1th-32kdf1.f",[1,2,3]),
  # "rand-read-1th-128kdf1.f",
  # "rand-read-1th-1024kdf1.f"
]

workloads_leitura_16f = [
  # "seq-read-16th-16f-4kdf1.f",
  # "seq-read-16th-16f-32kdf1.f",
  # "seq-read-16th-16f-128kdf1.f",
  # "seq-read-16th-16f-1024kdf1.f",
  # "rand-read-16th-16f-4kdf1.f",
  # "rand-read-16th-16f-32kdf1.f",
  # "rand-read-16th-16f-128kdf1.f",
  # "rand-read-16th-16f-1024kdf1.f"
]

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

def mount_local_fuse_filesystem():
  global mount_folder, backend_folder

  subprocess.Popen("ssh {} \"sudo ./fuse_high_pt {} {} > /dev/null 2> /dev/null &\""
                     .format(client_addr, mount_folder, backend_folder), shell=True
                  ).wait()

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

def run_filebench(file_name, try_nr = 0):
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

  local_results_path = results_local_folder + "/" + log_folder_name + "/" + file_name[:-2] + "/" + str(try_nr) + "/"
  ensure_directory(local_results_path)
  subprocess.Popen("scp {}:{} {}"
                    .format(client_addr, results_file_path, local_results_path), shell=True
                ).wait()

  if send_results_to_bucket:
    local_results_file_path = local_results_path + file_name[:-2] + ".txt"
    bucket_folder_path = "gs://lsfs_bucket/logs/" + log_folder_name + "/" + file_name[:-2] + "/" + str(try_nr) + "/"
    subprocess.Popen("gsutil cp {} {}"
                      .format(local_results_file_path, bucket_folder_path), shell=True
                     ).wait()  
  
def write_1_file():
    cleanup()
    run_peers()
    mount_filesystem()
    run_filebench("seq-write-1th-4k-write1.f")

def write_16_files():
    cleanup()
    run_peers()
    mount_filesystem()
    run_filebench("seq-write-16th-16f-4k-write16.f")

def start_monitoring(run_name):
  global monitoring

  if(not monitoring):
     return

  extra_vars = json.dumps(
    {
      'run_name': run_name,
      'gpu': False 
    }
    , separators=(',',':')
  )

  subprocess.Popen("ansible-playbook monitoring/start-monitoring/playbook.yml -i hosts --extra-vars \'{}\'".format(extra_vars), shell=True).wait()

def stop_monitoring(run_name, try_nr):
  global monitoring

  if(not monitoring):
     return

  subprocess.Popen("ansible-playbook monitoring/stop-monitoring/playbook.yml -i hosts --extra-vars \"run_name={} try_nr={} folder_name={}\"".format(run_name, try_nr, log_folder_name), shell=True).wait()

def clear_caches():
  global client_addr

  if local_filesystem:
    subprocess.Popen("ssh {} \"sudo sync && \
                      sudo su -c \'echo 3 > /proc/sys/vm/drop_caches\'\"".format(client_addr)
                      , shell=True
                    ).wait()
  else:
    subprocess.Popen("ansible-playbook drop-caches-lsfs-nodes/playbook.yml -i hosts", shell=True).wait()


def clear_fileset():
  global client_addr, mount_folder

  subprocess.Popen("ssh {} \"sudo rm -r {}/*\"".format(client_addr, mount_folder)
                    , shell=True
                  ).wait()
            
  subprocess.Popen("ssh {} \"sudo rm -r {}/*\"".format(client_addr, backend_folder)
                    , shell=True
                  ).wait()

def ensure_directory(directory):
  if not os.path.exists(directory):
    os.makedirs(directory)

def create_machines():
  global nr_of_nodes

  subprocess.Popen("python3 0_machine-creation.py --nr_nodes {}".format(nr_of_nodes) , shell=True).wait()

def destroy_machines():
  subprocess.Popen("ansible-playbook cluster-deploy/gcloud_teardown.yml -i hosts", shell=True).wait()

############ Starting Point ############

def main():
  global master_addr, client_addr, workloads_escrita, workloads_leitura_16f, workloads_leitura_1f, time_between_runs, results_local_folder

  ensure_directory(results_local_folder)

  if should_create_machines:
    create_machines()

  # Create Inventory dictionary
  inv = create_inventory_dict()
  client_addr = inv['client']
  if not local_filesystem:
    master_addr = inv['master']

  # Send Workloads to Client Node
  #send_workloads_to_server()

  if local_filesystem:

    for file, tries in workloads_escrita:
      for try_nr in tries:
    #    stop_monitoring(file[:-2], try_nr)
    #    clear_fileset()
        clear_caches()
        if fuse_filesystem:
          mount_local_fuse_filesystem()
        start_monitoring(file[:-2])
        run_filebench(file, try_nr)
        stop_monitoring(file[:-2], try_nr)
        if fuse_filesystem:
          cleanup()
        clear_fileset()
        sleep(time_between_runs)

    #run_filebench("seq-write-1th-4k-write1.f")
    for file, tries in workloads_leitura_1f:
      for try_nr in tries:
       # stop_monitoring(file[:-2], try_nr)
        sleep(time_between_runs)
        clear_caches()
        start_monitoring(file[:-2])
        run_filebench(file, try_nr)
        stop_monitoring(file[:-2], try_nr)
    clear_fileset()
    

    # run_filebench("seq-write-16th-16f-4k-write16.f")
    # for file in workloads_leitura_16f:
    #   sleep(time_between_runs)
    #   clear_caches()
    #   start_monitoring(file[:-2])
    #   run_filebench(file)
    #   stop_monitoring(file[:-2])
    # clear_fileset()

  else:

    for file, tries in workloads_escrita:
      for try_nr in tries:
        cleanup()
        clear_caches()
        run_peers()
        mount_filesystem()
        start_monitoring(file[:-2])
        run_filebench(file, try_nr)
        stop_monitoring(file[:-2], try_nr)
        kill_processes()
        sleep(time_between_runs)

    #write_1_file()
    for file, tries in workloads_leitura_1f:
      for try_nr in tries:
        sleep(time_between_runs)
        clear_caches()
        start_monitoring(file[:-2])
        run_filebench(file, try_nr)
        stop_monitoring(file[:-2], try_nr)

#    write_16_files()
#    for file, tries in workloads_leitura_16f:
#      for try_nr in tries:
#        sleep(time_between_runs)
#        clear_caches()
#        start_monitoring(file[:-2])
#        run_filebench(file, try_nr)
#        stop_monitoring(file[:-2], try_nr)

  if should_destroy_machines:
    destroy_machines()

if __name__ == "__main__":
  main()
