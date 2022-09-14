#!/bin/bash


#------------------------------------------

CONFIG_FILE=conf.yaml


BASE_ANSIBLE_PATH=Deploy/cloudinhas/ansible_k8s
PEER_DEPLOY_PLAYBOOKS_PATH=$BASE_ANSIBLE_PATH/3_deploy



# Peer configuration variables

# Format of NR_OF_PEERS
#  a. number_of_peers
#  b. replication_min
#  c. replication_max
#  Delimiter. ":"
#
#  Example. 
#       a:b:c
#       2:2:3
#       number_of_peers=2
#       replication_min=2
#       replication_max=3
#

PEERS_CONFIG=("2:2:3" "4:4:5")





###########################################################################################################
#                             Functions                                ####################################
###########################################################################################################

# Change group construction replication
change_gc_rep(){

    #$1 - replication min
    #$2 - replication max

    sed -i "/rep_min.*/c\    rep_min: $1" $CONFIG_FILE
    sed -i "/rep_max.*/c\    rep_max: $2" $CONFIG_FILE
   
}


 for CONFIG_P in ${PEERS_CONFIG[@]}; do

    NR_PEERS=$(echo $CONFIG_P | cut -d ":" -f 1)
    GC_REP_MIN=$(echo $CONFIG_P | cut -d ":" -f 2)
    GC_REP_MAX=$(echo $CONFIG_P | cut -d ":" -f 3)

    #change_gc_rep $GC_REP_MIN $GC_REP_MAX
    

    ansible-playbook reset_prev_deploy.yml -i $BASE_ANSIBLE_PATH/hosts -v

    ansible-playbook 1_setup_deploy.yml -i $BASE_ANSIBLE_PATH/hosts -v

    ansible-playbook 2_pod_deploy.yml -e "nr_peers=$NR_PEERS" -i $BASE_ANSIBLE_PATH/hosts -v

    sleep 60

    ansible-playbook $PEER_DEPLOY_PLAYBOOKS_PATH/3_run_workloads.yml -e "peer_config_str=${NR_PEERS}nds<$GC_REP_MIN-$GC_REP_MAX> nr_peers=$NR_PEERS" -i $BASE_ANSIBLE_PATH/hosts -v

    #ansible-playbook collect_results.yml -i $BASE_ANSIBLE_PATH/hosts -v

    ansible-playbook 3_shutdown_pods.yml -i $BASE_ANSIBLE_PATH/hosts -v


 done