#!/bin/bash

#This script runs all the workloads in the WORKLOADS_PATH directory with multiple configurations


COM_DIRECTORY=shared_dir
MOUNT_POINT=/test_filesystem/InnerFolder

#------------------------------------------

# Workloads MASTER variables

REMOTE_WORKLOADS_PATH=$COM_DIRECTORY/workloads-filebench
REMOTE_WORKLOADS_READ_PATH=$REMOTE_WORKLOADS_PATH/read-data-micro
REMOTE_WORKLOADS_WRITE_PATH=$REMOTE_WORKLOADS_PATH/write-data-micro
REMOTE_WORKLOADS_METADATA_PATH=$REMOTE_WORKLOADS_PATH/metadata-micro

# Workloads LOCAL variables

LOCAL_WORKLOADS_PATH=../../workloads-filebench
LOCAL_WORKLOADS_READ_PATH=$LOCAL_WORKLOADS_PATH/read-data-micro
LOCAL_WORKLOADS_WRITE_PATH=$LOCAL_WORKLOADS_PATH/write-data-micro
LOCAL_WORKLOADS_METADATA_PATH=$LOCAL_WORKLOADS_PATH/metadata-micro

#------------------------------------------

# Metrics variables

METRICS_PATH=$COM_DIRECTORY/metrics


# Output variables

LOCAL_OUTPUT_PATH=../../outputs


#------------------------------------------

# Nodes data variables

REMOTE_CONFIG_FILE=$COM_DIRECTORY/conf.yaml
LOCAL_CONFIG_FILE=../../conf.yaml

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

PEERS_CONFIG=("2_2_3" "4_4_5")

RUNTIME_PER_WORKLOAD=100 #seconds
NR_OF_ITERATIONS_PER_WORKLOAD=2

WORKLOAD_VAR_IO_SIZE=("4k" "128k")
WORKLOAD_VAR_PARALELIZATION_LIMIT=("4k" "128k")
WORKLOAD_VAR_LB_TYPE=("smart" "dynamic")
WORKLOAD_VAR_CACHE=("cache_on" "cache_off");
WORKLOAD_VAR_CACHE_REFRESH_TIME=("1000" "30000" "60000")


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

    ansible-playbook 1_setup_deploy.yml -i ../hosts -v

    WORKLOAD_TYPE=write
    OUTPUT_PATH="outputs-run-$CONFIG_P-$(date +"%Y_%m_%d_%I_%M_%p")"

    # LOAD_BALANCER=dynamic
    # USE_CACHE=False
    # CACHE_REFRESH=1000

    # mkdir -p $LOCAL_OUTPUT_PATH/$OUTPUT_PATH/$WORKLOAD_TYPE

    # for WL_PATH in $(find $LOCAL_WORKLOADS_WRITE_PATH -maxdepth 2 -type f -printf "%p\n"); do

    #     wl_file=$(basename $WL_PATH)
    #     wl_name=$(echo $wl_file | cut -f 1 -d '.') #removes .f
    #     wl_remote_path=$REMOTE_WORKLOADS_WRITE_PATH/$wl_file

    #     for WL_CONF_PARAL_LIMIT in ${WORKLOAD_VAR_PARALELIZATION_LIMIT[@]}; do

    #         for WL_CONF_IO in ${WORKLOAD_VAR_IO_SIZE[@]}; do

    #             ansible-playbook change_run_config.yml -e "load_balancer=$LOAD_BALANCER config_file=$REMOTE_CONFIG_FILE use_cache=$USE_CACHE cache_refresh=$CACHE_REFRESH paralelization=$WL_CONF_PARAL_LIMIT wl_conf_io=$WL_CONF_IO wl_path=$wl_remote_path" -i ../hosts -v
                
    #             WL_CONF_NAME="$wl_name-$WL_CONF_IO-$LOAD_BALANCER-$WL_CONF_PARAL_LIMIT"

    #             OUTPUT_FILE_PATH=$LOCAL_OUTPUT_PATH/$OUTPUT_PATH/$WORKLOAD_TYPE/run-$WL_CONF_NAME-lsfs-fb.output

    #             touch $OUTPUT_FILE_PATH

    #             for ((RUN_ITER=1; RUN_ITER<=NR_OF_ITERATIONS_PER_WORKLOAD; RUN_ITER++)); do

    #                 ansible-playbook 2_pod_deploy.yml -e "nr_peers=$NR_PEERS" -i ../hosts -v

    #                 echo -e "\nRun: #$RUN_ITER,wl_name:$WL_CONF_NAME,wl_path:$wl_remote_path,fs:lsfs\n\n" >> $OUTPUT_FILE_PATH

    #                 ansible-playbook 4_run_workload.yml -e "nr_peers=$NR_PEERS com_directory=$COM_DIRECTORY metrics_path=$METRICS_PATH wl_name=$WL_CONF_NAME wl_path=$wl_remote_path output_path=$OUTPUT_FILE_PATH" -i ../hosts -v
                    
    #                 ansible-playbook 5_shutdown_pods.yml -i ../hosts -v
                    
    #                 ansible-playbook clean_playbooks/clean_peer_db.yml -i ../hosts -v
                    
    #             done
                            
    #         done

    #     done
        
    # done

#------------------------------------------

# Run read workloads

    WORKLOAD_TYPE=read

    LOAD_BALANCER=dynamic
    USE_CACHE=False
    CACHE_REFRESH=1000

    mkdir -p $LOCAL_OUTPUT_PATH/$OUTPUT_PATH/$WORKLOAD_TYPE

    for WL_PATH in $(find $LOCAL_WORKLOADS_READ_PATH -maxdepth 2 -type f -printf "%p\n"); do

        wl_file=$(basename $WL_PATH)
        wl_name=$(echo $wl_file | cut -f 1 -d '.') #removes .f
        wl_remote_path=$REMOTE_WORKLOADS_READ_PATH/$wl_file

        for WL_CONF_PARAL_LIMIT in ${WORKLOAD_VAR_PARALELIZATION_LIMIT[@]}; do

            for WL_CONF_IO in ${WORKLOAD_VAR_IO_SIZE[@]}; do

                ansible-playbook change_run_config.yml -e "load_balancer=$LOAD_BALANCER config_file=$REMOTE_CONFIG_FILE use_cache=$USE_CACHE cache_refresh=$CACHE_REFRESH paralelization=$WL_CONF_PARAL_LIMIT wl_conf_io=$WL_CONF_IO wl_path=$wl_remote_path" -i ../hosts -v
                
                WL_CONF_NAME="$wl_name-$WL_CONF_IO-$LOAD_BALANCER-$WL_CONF_PARAL_LIMIT"

                OUTPUT_FILE_PATH=$LOCAL_OUTPUT_PATH/$OUTPUT_PATH/$WORKLOAD_TYPE/run-$WL_CONF_NAME-lsfs-fb.output

                touch $OUTPUT_FILE_PATH

                for ((RUN_ITER=1; RUN_ITER<=NR_OF_ITERATIONS_PER_WORKLOAD; RUN_ITER++)); do

                    ansible-playbook 2_pod_deploy.yml -e "nr_peers=$NR_PEERS" -i ../hosts -v

                    echo -e "\nRun: #$RUN_ITER,wl_name:$WL_CONF_NAME,wl_path:$wl_remote_path,fs:lsfs\n\n" >> $OUTPUT_FILE_PATH

                    ansible-playbook 4_run_workload.yml -e "nr_peers=$NR_PEERS com_directory=$COM_DIRECTORY metrics_path=$METRICS_PATH wl_name=$WL_CONF_NAME wl_path=$wl_remote_path output_path=$OUTPUT_FILE_PATH" -i ../hosts -v
                    
                    ansible-playbook 5_shutdown_pods.yml -i ../hosts -v
                                        
                done
                            
            done

        done
        
    done

done

#------------------------------------------

# Run metadata workloads

# sed -i "/nr_puts_required.*/c\    nr_puts_required: 3" $CONFIG_FILE
# sed -i "/nr_gets_required.*/c\    nr_gets_required: 3" $CONFIG_FILE
# sed -i "/nr_gets_version_required.*/c\    nr_gets_version_required: 3" $CONFIG_FILE

# i=0

# WORKLOAD_TYPE=metadata

# mkdir -p $OUTPUT_PATH/$WORKLOAD_TYPE

# reset_db 99

# for WL_PATH in $(find $WORKLOADS_METADATA_PATH -maxdepth 2 -type f -printf "%p\n"); do

#     wl_file=$(basename $WL_PATH)
#     wl_name=$(echo $wl_file | cut -f 1 -d '.') #removes .f

#     output_results_file="$OUTPUT_PATH/$WORKLOAD_TYPE/run-$wl_name-lsfs-fb.output"
#     touch $output_results_file

#     for ((RUN_ITER=1; RUN_ITER<=NR_OF_ITERATIONS_PER_WORKLOAD; RUN_ITER++)); do

#         get_client_pod_name

#         echo -e "\nRun: #$RUN_ITER,wl_name:$wl_name,wl_path:$WL_PATH,fs:lsfs\n\n" >> $output_results_file

#         echo "Starting dstat in all $NR_PEERS_IN_CLUSTER peers - $i x."

#         for ((PEER_NR=1; PEER_NR<=NR_PEERS_IN_CLUSTER; PEER_NR++)); do

#             start_dstat $wl_name $PEER_NR
        
#         done

#         run_fb_workload $WL_PATH $output_results_file

#         echo "Stopping dstat - $i x."

#         for ((PEER_NR=1; PEER_NR<=NR_PEERS_IN_CLUSTER; PEER_NR++)); do

#             stop_dstat $PEER_NR
        
#         done

#         echo "Clean peers database and reset client."

#         reset_db $((100+i+RUN_ITER))
        
#     done
    
#     i=$((i+NR_OF_ITERATIONS_PER_WORKLOAD))

# done
