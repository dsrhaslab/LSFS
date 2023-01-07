#!/bin/bash

#This script creates the setup and k8s cluster in gcp using ansible scripts
CONTAINER_COM_DIRECTORY=shared_dir

PROJECT_ID="largescale22"

REMOTE_COM_DIRECTORY=lsfs/shared_dir
REMOTE_MOUNT_VOLUME_DIRECTORY=lsfs/test_filesystem

#------------------------------------------

# Local variables

LOCAL_CONFIG_FILE=conf.yaml

#------------------------------------------
#               Node Setup
#------------------------------------------
NR_PEERS=500

NR_GROUPS=16

WORKLOAD_NAME=$1

###########################################################################################################
#                            Setup cluster                             ####################################
###########################################################################################################

# Async run -> cluster creation can be run simultaneously with node setup (& detatches process)
ansible-playbook -f 100 1_create_cluster.yml -i hosts -v &

ansible-playbook -f 100 2_nodes_setup.yml -e "remote_com_directory=$REMOTE_COM_DIRECTORY remote_mount_vol_directory=$REMOTE_MOUNT_VOLUME_DIRECTORY nr_peers=$NR_PEERS nr_groups=$NR_GROUPS project_id=$PROJECT_ID" -i hosts -v

sleep 60

ansible-playbook 3_create_pods.yml -e "remote_com_directory=$REMOTE_COM_DIRECTORY remote_mount_vol_directory=$REMOTE_MOUNT_VOLUME_DIRECTORY nr_peers=$NR_PEERS" -i hosts -v

# ansible-playbook monitoring/start_monitoring.yml -e "remote_com_directory=$REMOTE_COM_DIRECTORY container_com_directory=$CONTAINER_COM_DIRECTORY wl_name=$WORKLOAD_NAME nr_peers=$NR_PEERS" -i hosts -v

# ansible-playbook monitoring/stop_monitoring.yml -e "remote_com_directory=$REMOTE_COM_DIRECTORY container_com_directory=$CONTAINER_COM_DIRECTORY wl_name=$WORKLOAD_NAME project_id=$PROJECT_ID nr_peers=$NR_PEERS" -i hosts -v