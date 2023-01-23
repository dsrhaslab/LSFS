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

NR_GROUPS=64

WORKLOAD_NAME=$1

FORK_NUMBER=$NR_PEERS

###########################################################################################################
#                            Setup cluster                             ####################################
###########################################################################################################

# Async run -> cluster creation can be run simultaneously with node setup (& detatches process)
ansible-playbook -f $FORK_NUMBER 1_create_cluster.yml -i hosts

ansible-playbook -f $FORK_NUMBER 2_update_config.yml -e "remote_com_directory=$REMOTE_COM_DIRECTORY" -i hosts

# ansible-playbook -f $FORK_NUMBER 3_nodes_setup.yml -e "remote_com_directory=$REMOTE_COM_DIRECTORY remote_mount_vol_directory=$REMOTE_MOUNT_VOLUME_DIRECTORY nr_peers=$NR_PEERS nr_groups=$NR_GROUPS project_id=$PROJECT_ID" -i hosts

# sleep 60

# ansible-playbook 4_create_pods.yml -e "remote_com_directory=$REMOTE_COM_DIRECTORY remote_mount_vol_directory=$REMOTE_MOUNT_VOLUME_DIRECTORY nr_peers=$NR_PEERS" -i hosts

# ansible-playbook -f $FORK_NUMBER zip_save_db.yml -e "remote_com_directory=$REMOTE_COM_DIRECTORY project_id=$PROJECT_ID" -i hosts

# ansible-playbook monitoring/start_monitoring.yml -e "remote_com_directory=$REMOTE_COM_DIRECTORY container_com_directory=$CONTAINER_COM_DIRECTORY wl_name=$WORKLOAD_NAME nr_peers=$NR_PEERS" -i hosts -v

# ansible-playbook -f $FORK_NUMBER monitoring/stop_monitoring.yml -e "remote_com_directory=$REMOTE_COM_DIRECTORY container_com_directory=$CONTAINER_COM_DIRECTORY wl_name=$WORKLOAD_NAME project_id=$PROJECT_ID nr_peers=$NR_PEERS" -i hosts -v