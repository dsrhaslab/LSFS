#!/bin/bash

#This script runs all the workloads in the WORKLOADS_PATH directory with multiple configurations

#$1 Peer configuration string (nr_peers-nr_grupos)
#$2 Number of peers in the cluster

COM_DIRECTORY=shared_dir
MOUNT_POINT=/test_filesystem/InnerFolder
#------------------------------------------

# Workloads variables

WORKLOADS_PATH=$COM_DIRECTORY/workloads-filebench
WORKLOADS_READ_PATH=$WORKLOADS_PATH/read-data-micro
WORKLOADS_WRITE_PATH=$WORKLOADS_PATH/write-data-micro
WORKLOADS_METADATA_PATH=$WORKLOADS_PATH/metadata-micro

RUNTIME_PER_WORKLOAD=900 #seconds
NR_OF_ITERATIONS_PER_WORKLOAD=2


#WORKLOAD_VAR_IO_SIZE=("4k" "32k")
WORKLOAD_VAR_IO_SIZE=("4k" "32k")
#WORKLOAD_VAR_PARALELIZATION_LIMIT=("4k" "8k" "16k" "32k" "64k" "96k" "128k")
WORKLOAD_VAR_PARALELIZATION_LIMIT=("4k")
WORKLOAD_VAR_LB_TYPE=("smart" "dynamic")
WORKLOAD_VAR_CACHE=("cache_on" "cache_off");
WORKLOAD_VAR_CACHE_REFRESH_TIME=("500" "1000" "2000" "4000")

#------------------------------------------

# Metrics variables

METRICS_PATH=$COM_DIRECTORY/metrics


#------------------------------------------

# Nodes data variables

CONFIG_FILE=$COM_DIRECTORY/conf.yaml
PEERS_IPS_FILE=$COM_DIRECTORY/peer_ips
NR_PEERS_IN_CLUSTER=$2
BOOTSTRAPPER_IP=""
CLIENT_NAME=""

#------------------------------------------

# Output variables

OUTPUT_PATH="outputs-run-$1-$(date +"%Y_%m_%d_%I_%M_%p")"

#------------------------------------------


TOTAL_NR_WORKLOADS=$(find $WORKLOADS_PATH -maxdepth 4 -type f -printf "%p\n" | wc -l)
PEERS_IPS=()


###########################################################################################################
#                             Functions                                ####################################
###########################################################################################################



read_peer_hosts_file() {
    
    # $1 = peer_hosts_file
    
    while IFS= read -r line; do
       PEERS_IPS+=($line)
    done < "$1"
}

get_bootstrapper_ip() {
    
    BOOTSTRAPPER_IP=$(kubectl get pods -n lsfs -o wide --no-headers | grep bootstrapper | awk '{ print $6}')

}

get_client_pod_name() {
    
    CLIENT_NAME=$(kubectl get pods -n lsfs -o wide --no-headers | grep client1 | awk '{ print $1}')

}

start_dstat() {

    #$1 - workload name
    #$2 - Peer number

    kubectl exec -n lsfs peer$2 -- bash /$COM_DIRECTORY/scripts/init_dstat.sh $METRICS_PATH $2 run-$1-lsfs-fb.dstat.csv
}


stop_dstat() {

    #$1 - Peer number

    kubectl exec -n lsfs peer$1 -- bash /$COM_DIRECTORY/scripts/stop_dstat.sh &> /dev/null
}


run_fb_workload() {

    #$1 - Workload Path
    #$2 - Output file

    kubectl exec -it -n lsfs $CLIENT_NAME -- /bin/bash -c "filebench -f /$1" &>> $2
}

reset_client() {

    #$1 - client_id

    echo "$1" > $COM_DIRECTORY/client_id

    get_client_pod_name

    kubectl delete pod -n lsfs $CLIENT_NAME &> /dev/null

    sleep 20
}


reset_db() {

    #$1 - Command version
    echo "Clean peers database and reset client."

    kubectl exec -it -n lsfs $CLIENT_NAME -- /bin/bash -c "./build/client_exe $BOOTSTRAPPER_IP $1 /$CONFIG_FILE /$PEERS_IPS_FILE" &> /dev/null

    sleep 10

    kubectl delete pod -n lsfs $CLIENT_NAME &> /dev/null

    sleep 30

}

change_lb(){

    #$1 - LB type (dynamic or smart)

    if [ "$1" = "dynamic" ]; then
        sed -i "/type.*/c\      type: dynamic" $CONFIG_FILE
    else
        sed -i "/type.*/c\      type: smart" $CONFIG_FILE
    fi
}

change_cache(){

    #$1 - Cache or no cache
    #$2 - Cache refresh time

    if [ "$1" = "cache_on" ]; then
        sed -i "/use_cache.*/c\      use_cache: true" $CONFIG_FILE
        sed -i "/refresh_cache_time.*/c\      refresh_cache_time: $2" $CONFIG_FILE
    else
        sed -i "/use_cache.*/c\      use_cache: False" $CONFIG_FILE
    fi
}


change_parallelization(){

    #$1 - parallelization limit

    sed -i "/limit_write_paralelization_to.*/c\    limit_write_paralelization_to: $1" $CONFIG_FILE
    sed -i "/limit_read_paralelization_to.*/c\    limit_read_paralelization_to: $1" $CONFIG_FILE
}





###########################################################################################################
#                     Run all filebench workloads                      ####################################
###########################################################################################################

# Get Bootstrapper ip

get_bootstrapper_ip


# Setup Workloads

for WL_PATH in $(find $WORKLOADS_PATH -maxdepth 4 -type f -printf "%p\n"); do

    sed -i "/set \$WORKLOAD_PATH.*/c\set \$WORKLOAD_PATH=$MOUNT_POINT" $WL_PATH
    sed -i "/set \$WORKLOAD_RUNTIME.*/c\set \$WORKLOAD_RUNTIME=$RUNTIME_PER_WORKLOAD" $WL_PATH

done


#------------------------------------------

# Run write workloads

i=0

WORKLOAD_TYPE=write

mkdir -p $OUTPUT_PATH/$WORKLOAD_TYPE

for WL_PATH in $(find $WORKLOADS_WRITE_PATH -maxdepth 2 -type f -printf "%p\n"); do

    wl_file=$(basename $WL_PATH)
    wl_name=$(echo $wl_file | cut -f 1 -d '.') #removes .f

    for LB_TYPE in ${WORKLOAD_VAR_LB_TYPE[@]}; do

        change_lb $LB_TYPE

        for WL_CONF_PARAL_LIMIT in ${WORKLOAD_VAR_PARALELIZATION_LIMIT[@]}; do

            change_parallelization $WL_CONF_PARAL_LIMIT
            
            reset_client $((i+1))

            for WL_CONF_IO in ${WORKLOAD_VAR_IO_SIZE[@]}; do
            
                sed -i "/set \$IO_SIZE.*/c\set \$IO_SIZE=$WL_CONF_IO" $WL_PATH

                output_results_file="$OUTPUT_PATH/$WORKLOAD_TYPE/run-$wl_name-$WL_CONF_IO-$LB_TYPE-$WL_CONF_PARAL_LIMIT-lsfs-fb.output"
                touch $output_results_file

                for ((RUN_ITER=1; RUN_ITER<=NR_OF_ITERATIONS_PER_WORKLOAD; RUN_ITER++)); do

                    get_client_pod_name

                    echo -e "\nRun: #$RUN_ITER,wl_name:$wl_name-$WL_CONF_IO-$LB_TYPE-$WL_CONF_PARAL_LIMIT,wl_path:$WL_PATH,fs:lsfs\n\n" >> $output_results_file

                    echo "Starting dstat in all $NR_PEERS_IN_CLUSTER peers - $i x."

                    for ((PEER_NR=1; PEER_NR<=NR_PEERS_IN_CLUSTER; PEER_NR++)); do

                        start_dstat "$wl_name-$WL_CONF_IO-$LB_TYPE-$WL_CONF_PARAL_LIMIT" $PEER_NR
                    
                    done

                    run_fb_workload $WL_PATH $output_results_file

                    echo "Stopping dstat - $i x."

                    for ((PEER_NR=1; PEER_NR<=NR_PEERS_IN_CLUSTER; PEER_NR++)); do

                        stop_dstat $PEER_NR
                    
                    done

                    reset_db $((i+RUN_ITER))
            
                done
                
                i=$((i+NR_OF_ITERATIONS_PER_WORKLOAD+1))
            
            done

        done
    
    done

done

#------------------------------------------

# Run read workloads

# i=0

# WORKLOAD_TYPE=read

# mkdir -p $OUTPUT_PATH/$WORKLOAD_TYPE

# for WL_PATH in $(find $WORKLOADS_READ_PATH -maxdepth 2 -type f -printf "%p\n"); do

#     wl_file=$(basename $WL_PATH)
#     wl_name=$(echo $wl_file | cut -f 1 -d '.') #removes .f
#     wl_type=$(echo $wl_file | cut -f 1 -d '-') 

#     for LB_TYPE in ${WORKLOAD_VAR_LB_TYPE[@]}; do

#         change_lb $LB_TYPE

#         if [ "$wl_type" = "rand" ]; then
#             NEW_VAR_PARALELIZATION_LIMIT=("4k" "16k" "32k")
#         else
#             NEW_VAR_PARALELIZATION_LIMIT=( ${WORKLOAD_VAR_PARALELIZATION_LIMIT[@]} )
#         fi

#         for WL_CONF_PARAL_LIMIT in ${NEW_VAR_PARALELIZATION_LIMIT[@]}; do

#             change_parallelization $WL_CONF_PARAL_LIMIT

#             reset_client $((i+1))

#             if [[ "$wl_type" = "rand" && "$WL_CONF_PARAL_LIMIT" = "4k" ]]; then
#                 NEW_VAR_IO_SIZE=("4k")
#             elif [[ "$wl_type" = "rand" && "$WL_CONF_PARAL_LIMIT" = "16k" ]]; then
#                 NEW_VAR_IO_SIZE=("32k")
#             elif [[ "$wl_type" = "rand" && "$WL_CONF_PARAL_LIMIT" = "32k" ]]; then
#                 NEW_VAR_IO_SIZE=("32k")
#             else 
#                 NEW_VAR_IO_SIZE=( ${WORKLOAD_VAR_IO_SIZE[@]} )
#             fi

#             for WL_CONF_IO in ${NEW_VAR_IO_SIZE[@]}; do
            
#                 sed -i "/set \$IO_SIZE.*/c\set \$IO_SIZE=$WL_CONF_IO" $WL_PATH

#                 output_results_file="$OUTPUT_PATH/$WORKLOAD_TYPE/run-$wl_name-$WL_CONF_IO-$LB_TYPE-$WL_CONF_PARAL_LIMIT-lsfs-fb.output"
#                 touch $output_results_file

#                 for ((RUN_ITER=1; RUN_ITER<=NR_OF_ITERATIONS_PER_WORKLOAD; RUN_ITER++)); do

#                     get_client_pod_name

#                     echo -e "\nRun: #$RUN_ITER,wl_name:$wl_name-$WL_CONF_IO-$LB_TYPE-$WL_CONF_PARAL_LIMIT,wl_path:$WL_PATH,fs:lsfs\n\n" >> $output_results_file

#                     echo "Starting dstat in all $NR_PEERS_IN_CLUSTER peers - $i x."

#                     for ((PEER_NR=1; PEER_NR<=NR_PEERS_IN_CLUSTER; PEER_NR++)); do

#                         start_dstat "$wl_name-$WL_CONF_IO-$LB_TYPE-$WL_CONF_PARAL_LIMIT" $PEER_NR
                    
#                     done

#                     run_fb_workload $WL_PATH $output_results_file

#                     echo "Stopping dstat - $i x."

#                     for ((PEER_NR=1; PEER_NR<=NR_PEERS_IN_CLUSTER; PEER_NR++)); do

#                         stop_dstat $PEER_NR
                    
#                     done
                
#                 done
            
#                 i=$((i+NR_OF_ITERATIONS_PER_WORKLOAD+1))

#             done

#         done
    
#     done

# done

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
