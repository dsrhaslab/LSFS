#!/bin/bash

#This script creates the setup and k8s cluster in gcp using ansible scripts
CONTAINER_COM_DIRECTORY=shared_dir

PROJECT_ID="largescale22"

REMOTE_COM_DIRECTORY=lsfs/shared_dir
REMOTE_MOUNT_VOLUME_DIRECTORY=lsfs/test_filesystem

#------------------------------------------

# Local variables

LOCAL_CONFIG_FILE=../conf.yaml

#------------------------------------------
#               Node Setup
#------------------------------------------
NR_PEERS=10

NR_GROUPS=16

NR_PEERS_TO_KILL=3

WORKLOAD_NAME=$1

FORK_NUMBER=$NR_PEERS

###########################################################################################################
#                            Setup cluster                             ####################################
###########################################################################################################

# Async run -> cluster creation can be run simultaneously with node setup (& detatches process)
ansible-playbook setup_clean_peers.yml -e "nr_peers=$NR_PEERS nr_groups=$NR_GROUPS nr_peers_to_kill=$NR_PEERS_TO_KILL remote_com_directory=$REMOTE_COM_DIRECTORY" -i ../hosts