#!/bin/bash

#This script runs all the workloads in the WORKLOADS_PATH directory with multiple configurations
CONTAINER_COM_DIRECTORY=shared_dir

PROJECT_ID="largescale22"

REMOTE_COM_DIRECTORY=lsfs/shared_dir
REMOTE_MOUNT_VOLUME_DIRECTORY=lsfs/test_filesystem

#------------------------------------------

# Local variables

LOCAL_CONFIG_FILE=conf.yaml

LOCAL_OUTPUT_PATH=outputs

LOCAL_DSTAT_OUTPUT_PATH=dstat_outputs


#------------------------------------------
#               Node Setup
#------------------------------------------
NR_PEERS=16

NR_GROUPS=16


###########################################################################################################
#                            Setup cluster                             ####################################
###########################################################################################################


ansible-playbook 1_create_cluster.yml -i hosts -v

ansible-playbook 2_nodes_setup.yml -e "remote_com_directory=$REMOTE_COM_DIRECTORY remote_mount_vol_directory=$REMOTE_MOUNT_VOLUME_DIRECTORY nr_peers=$NR_PEERS nr_groups=$NR_GROUPS project_id=$PROJECT_ID" -i hosts -v

sleep 60

 ansible-playbook 3_create_pods.yml -e "remote_com_directory=$REMOTE_COM_DIRECTORY remote_mount_vol_directory=$REMOTE_MOUNT_VOLUME_DIRECTORY nr_peers=$NR_PEERS" -i hosts -v

                    