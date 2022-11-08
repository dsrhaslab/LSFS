#!/bin/bash

#This script runs all the workloads in the WORKLOADS_PATH directory with multiple configurations


CONTAINER_COM_DIRECTORY=shared_dir

REMOTE_COM_DIRECTORY=lsfs/shared_dir

#------------------------------------------

MOUNT_POINT=/test_filesystem/InnerFolder

# Workloads REMOTE_HOST variables

CONTAINER_WORKLOADS_PATH=$CONTAINER_COM_DIRECTORY/workloads-filebench
CONTAINER_WORKLOADS_READ_PATH=$CONTAINER_WORKLOADS_PATH/read-data-micro
CONTAINER_WORKLOADS_WRITE_PATH=$CONTAINER_WORKLOADS_PATH/write-data-micro
CONTAINER_WORKLOADS_METADATA_PATH=$CONTAINER_WORKLOADS_PATH/metadata-micro
CONTAINER_WORKLOADS_MACRO_PATH=$CONTAINER_WORKLOADS_PATH/macro

REMOTE_WORKLOADS_PATH=$REMOTE_COM_DIRECTORY/workloads-filebench
REMOTE_WORKLOADS_READ_PATH=$REMOTE_WORKLOADS_PATH/read-data-micro
REMOTE_WORKLOADS_WRITE_PATH=$REMOTE_WORKLOADS_PATH/write-data-micro
REMOTE_WORKLOADS_METADATA_PATH=$REMOTE_WORKLOADS_PATH/metadata-micro
REMOTE_WORKLOADS_MACRO_PATH=$REMOTE_WORKLOADS_PATH/macro

# Workloads LOCAL variables

LOCAL_WORKLOADS_PATH=../workloads-filebench
LOCAL_WORKLOADS_READ_PATH=$LOCAL_WORKLOADS_PATH/read-data-micro
LOCAL_WORKLOADS_WRITE_PATH=$LOCAL_WORKLOADS_PATH/write-data-micro
LOCAL_WORKLOADS_METADATA_PATH=$LOCAL_WORKLOADS_PATH/metadata-micro
LOCAL_WORKLOADS_MACRO_PATH=$LOCAL_WORKLOADS_PATH/macro

#------------------------------------------

# Local variables

LOCAL_CONFIG_FILE=conf.yaml

LOCAL_OUTPUT_PATH=outputs

LOCAL_DSTAT_OUTPUT_PATH=dstat_outputs


#------------------------------------------

#------------------------------------------
# Peer configuration variables

# Format of NR_OF_PEERS
#  a. number_of_peers
#  b. replication_min
#  c. replication_max
#  Delimiter. "_"
#
#  Example. 
#       a_b_c
#       2_2_3
#       number_of_peers=2
#       replication_min=2
#       replication_max=3
#


PEERS_CONFIG=("50_3_5")

RUNTIME_PER_WORKLOAD=900 #seconds
NR_OF_ITERATIONS_PER_WORKLOAD=3

WORKLOAD_VAR_IO_SIZE=("4k")
WORKLOAD_VAR_PARALELIZATION_LIMIT=("4k")
WORKLOAD_VAR_LB_TYPE=("smart" "dynamic")
#WORKLOAD_VAR_LB_TYPE=("dynamic")
WORKLOAD_VAR_CACHE=("cache_on" "cache_off");
WORKLOAD_VAR_CACHE_REFRESH_TIME=("1000")
WORKLOAD_VAR_ANTI_ENTROPY_INTERVAL=("30" "60" "300")


###########################################################################################################
#                             Functions                                ####################################
###########################################################################################################

# Change group construction replication
change_gc_rep(){

    #$1 - replication min
    #$2 - replication max

    sed -i "/rep_min.*/c\    rep_min: $1" $LOCAL_CONFIG_FILE
    sed -i "/rep_max.*/c\    rep_max: $2" $LOCAL_CONFIG_FILE
   
}

# Change number of required requests
change_wait_timeout(){

    #$1 - wait timeout in seconds

    sed -i "/client_wait_timeout.*/c\    client_wait_timeout: $1" $LOCAL_CONFIG_FILE
}

###########################################################################################################
#                     Run all filebench workloads                      ####################################
###########################################################################################################

# Setup Workloads

for WL_PATH in $(find $LOCAL_WORKLOADS_PATH -maxdepth 4 -type f -printf "%p\n"); do

    sed -i "/set \$WORKLOAD_PATH.*/c\set \$WORKLOAD_PATH=$MOUNT_POINT" $WL_PATH
    sed -i "/set \$WORKLOAD_RUNTIME.*/c\set \$WORKLOAD_RUNTIME=$RUNTIME_PER_WORKLOAD" $WL_PATH

done


for CONFIG_P in ${PEERS_CONFIG[@]}; do

    NR_PEERS=$(echo $CONFIG_P | cut -d "_" -f 1)
    GC_REP_MIN=$(echo $CONFIG_P | cut -d "_" -f 2)
    GC_REP_MAX=$(echo $CONFIG_P | cut -d "_" -f 3)

    change_gc_rep $GC_REP_MIN $GC_REP_MAX

    ansible-playbook deploy/1_setup_deploy.yml -e "remote_com_directory=$REMOTE_COM_DIRECTORY" -i deploy/hosts -v

    
    OUTPUT_PATH="outputs-run-$CONFIG_P-$(date +"%Y_%m_%d_%I_%M_%p")"

    mkdir -p $LOCAL_OUTPUT_PATH/$OUTPUT_PATH/$LOCAL_DSTAT_OUTPUT_PATH

    for ((RUN_ITER=1; RUN_ITER<=NR_OF_ITERATIONS_PER_WORKLOAD; RUN_ITER++)); do

        WORKLOAD_TYPE=macro

        USE_CACHE=true
        CACHE_REFRESH=1000

        NR_PUTS_REQUIRED=3
        NR_GETS_REQUIRED=1
        NR_GETS_VERSION_REQUIRED=1

        WL_CONF_PARAL_LIMIT=4k
        WL_CONF_IO=1m #macro workloads

        mkdir -p $LOCAL_OUTPUT_PATH/$OUTPUT_PATH/$WORKLOAD_TYPE

        WL_PATH=$LOCAL_WORKLOADS_MACRO_PATH/webserver.f

        wl_file=$(basename $WL_PATH)
        wl_name=$(echo $wl_file | cut -f 1 -d '.') #removes .f
        wl_remote_path=$REMOTE_WORKLOADS_MACRO_PATH/$wl_file
        wl_container_path=$CONTAINER_WORKLOADS_MACRO_PATH/$wl_file

        # for LB_TYPE in ${WORKLOAD_VAR_LB_TYPE[@]}; do

        #         ansible-playbook deploy/change_run_config.yml -e "remote_com_directory=$REMOTE_COM_DIRECTORY load_balancer=$LB_TYPE nr_puts_req=$NR_PUTS_REQUIRED nr_gets_req=$NR_GETS_REQUIRED nr_gets_vrs_req=$NR_GETS_VERSION_REQUIRED use_cache=$USE_CACHE cache_refresh=$CACHE_REFRESH paralelization=$WL_CONF_PARAL_LIMIT wl_conf_io=$WL_CONF_IO wl_path=$wl_remote_path" -i deploy/hosts -v
                
        #         WL_CONF_NAME="$wl_name-$LB_TYPE"

        #         OUTPUT_FILE_PATH=$LOCAL_OUTPUT_PATH/$OUTPUT_PATH/$WORKLOAD_TYPE/run-$WL_CONF_NAME-lsfs-fb.output
        #         DSTAT_FILE_PATH=$LOCAL_OUTPUT_PATH/$OUTPUT_PATH/$LOCAL_DSTAT_OUTPUT_PATH/run-$WL_CONF_NAME-lsfs

        #         mkdir -p $DSTAT_FILE_PATH
    
        #         touch $OUTPUT_FILE_PATH

        #         sleep 300

        #         ansible-playbook deploy/2_pod_deploy.yml -e "nr_peers=$NR_PEERS remote_com_directory=$REMOTE_COM_DIRECTORY" -i deploy/hosts -v

        #         #Wait for client stabilization
        #         sleep 100

        #         echo -e "\nRun: #$RUN_ITER,wl_name:$WL_CONF_NAME,wl_path:$wl_container_path,fs:lsfs\n\n" >> $OUTPUT_FILE_PATH

        #         ansible-playbook deploy/4_run_workload.yml -e "nr_peers=$NR_PEERS container_com_directory=$CONTAINER_COM_DIRECTORY remote_com_directory=$REMOTE_COM_DIRECTORY wl_name=$WL_CONF_NAME wl_path=$wl_container_path output_path=$OUTPUT_FILE_PATH dstat_path=$DSTAT_FILE_PATH" -i deploy/hosts -v
                
        #         ansible-playbook deploy/5_shutdown_pods.yml -i deploy/hosts -v
                
        #         ansible-playbook deploy/clean_playbooks/clean_peer_db.yml -e "remote_com_directory=$REMOTE_COM_DIRECTORY" -i deploy/hosts -v
        
        # done


        WORKLOAD_TYPE=macro

        LB_TYPE=smart
        USE_CACHE=true
        CACHE_REFRESH=1000

        NR_PUTS_REQUIRED=3
        NR_GETS_REQUIRED=1
        NR_GETS_VERSION_REQUIRED=1

        WL_CONF_PARAL_LIMIT=4k
        WL_CONF_IO=1m #macro workloads

        mkdir -p $LOCAL_OUTPUT_PATH/$OUTPUT_PATH/$WORKLOAD_TYPE

        WL_PATH=$LOCAL_WORKLOADS_MACRO_PATH/fileserver.f

        wl_file=$(basename $WL_PATH)
        wl_name=$(echo $wl_file | cut -f 1 -d '.') #removes .f
        wl_remote_path=$REMOTE_WORKLOADS_MACRO_PATH/$wl_file
        wl_container_path=$CONTAINER_WORKLOADS_MACRO_PATH/$wl_file

        for ENTROPY_INTERVAL in ${WORKLOAD_VAR_ANTI_ENTROPY_INTERVAL[@]}; do

                ansible-playbook deploy/change_run_config.yml -e "remote_com_directory=$REMOTE_COM_DIRECTORY load_balancer=$LB_TYPE nr_puts_req=$NR_PUTS_REQUIRED nr_gets_req=$NR_GETS_REQUIRED nr_gets_vrs_req=$NR_GETS_VERSION_REQUIRED use_cache=$USE_CACHE cache_refresh=$CACHE_REFRESH entropy_interval=$ENTROPY_INTERVAL paralelization=$WL_CONF_PARAL_LIMIT wl_conf_io=$WL_CONF_IO wl_path=$wl_remote_path" -i deploy/hosts -v
                
                WL_CONF_NAME="$wl_name-anti_entropy-$ENTROPY_INTERVAL"

                OUTPUT_FILE_PATH=$LOCAL_OUTPUT_PATH/$OUTPUT_PATH/$WORKLOAD_TYPE/run-$WL_CONF_NAME-lsfs-fb.output
                DSTAT_FILE_PATH=$LOCAL_OUTPUT_PATH/$OUTPUT_PATH/$LOCAL_DSTAT_OUTPUT_PATH/run-$WL_CONF_NAME-lsfs

                mkdir -p $DSTAT_FILE_PATH
    
                touch $OUTPUT_FILE_PATH

                sleep 300

                ansible-playbook deploy/2_pod_deploy.yml -e "nr_peers=$NR_PEERS remote_com_directory=$REMOTE_COM_DIRECTORY" -i deploy/hosts -v

                #Wait for client stabilization
                sleep 100

                echo -e "\nRun: #$RUN_ITER,wl_name:$WL_CONF_NAME,wl_path:$wl_container_path,fs:lsfs\n\n" >> $OUTPUT_FILE_PATH

                ansible-playbook deploy/4_run_workload.yml -e "nr_peers=$NR_PEERS container_com_directory=$CONTAINER_COM_DIRECTORY remote_com_directory=$REMOTE_COM_DIRECTORY wl_name=$WL_CONF_NAME wl_path=$wl_container_path output_path=$OUTPUT_FILE_PATH dstat_path=$DSTAT_FILE_PATH" -i deploy/hosts -v
                
                ansible-playbook deploy/5_shutdown_pods.yml -i deploy/hosts -v
                
                ansible-playbook deploy/clean_playbooks/clean_peer_db.yml -e "remote_com_directory=$REMOTE_COM_DIRECTORY" -i deploy/hosts -v
        
        done
        
    done

done
