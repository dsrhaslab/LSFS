#!/bin/bash

COM_DIRECTORY=shared_dir

#------------------------------------------

# Workloads variables

WORKLOADS_PATH=$COM_DIRECTORY/workloads-filebench
WORKLOADS_READ_PATH=$WORKLOADS_PATH/read-data-micro
WORKLOADS_WRITE_PATH=$WORKLOADS_PATH/write-data-micro
WORKLOADS_METADATA_PATH=$WORKLOADS_PATH/metadata-micro

RUNTIME_PER_WORKLOAD=300 #seconds
NR_OF_ITERATIONS_PER_WORKLOAD=1

WORKLOAD_VAR_IO_SIZE=("4k","32k")
WORKLOAD_VAR_PARALELIZATION_LIMIT=("4k","8k","16k","32k","64k","96k","128k")
WORKLOAD_VAR_LB_TYPE=("smart","dynamic")

#------------------------------------------

# Metrics variables

METRICS_PATH=$COM_DIRECTORY/metrics


#------------------------------------------

# Nodes data variables

CONFIG_FILE=$COM_DIRECTORY/conf.yaml
PEERS_IPS_FILE=$COM_DIRECTORY/peer_ips
NR_PEERS_IN_CLUSTER=50
BOOTSTRAPPER_IP=""
CLIENT_NAME=""

#------------------------------------------

# Output variables

OUTPUT_PATH="outputs-run-$(date +"%Y_%m_%d_%I_%M_%p")"

#------------------------------------------


TOTAL_NR_WORKLOADS=$(find $WORKLOADS_PATH -maxdepth 4 -type f -printf "%p\n" | wc -l)
PEERS_IPS=()


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

    kubectl delete pod -n lsfs $CLIENT_NAME &> /dev/null

    sleep 20
}


reset_db() {

    #$1 - Command version

    kubectl exec -it -n lsfs $CLIENT_NAME -- /bin/bash -c "./build/client_exe $BOOTSTRAPPER_IP $1 /$CONFIG_FILE /$PEERS_IPS_FILE" &> /dev/null

    sleep 10

    kubectl delete pod -n lsfs $CLIENT_NAME &> /dev/null

    sleep 30

}


########################################################################
#                     Run all filebench workloads                      #
########################################################################

get_bootstrapper_ip


#------------------------------------------

# Run write workloads

# i=0

# WORKLOAD_TYPE=write

# mkdir -p $OUTPUT_PATH/$WORKLOAD_TYPE

# for WL_PATH in $(find $WORKLOADS_WRITE_PATH -maxdepth 2 -type f -printf "%p\n"); do

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

#         if [ $RUN_ITER -nq $NR_OF_ITERATIONS_PER_WORKLOAD ]; then

#             echo "Clean peers database and reset client."

#             reset_db $((i+RUN_ITER))
#         fi
    
#     done
    
#     i+=$RUN_ITER

# done

#------------------------------------------

# Run read workloads

i=0

WORKLOAD_TYPE=read

mkdir -p $OUTPUT_PATH/$WORKLOAD_TYPE

for WL_PATH in $(find $WORKLOADS_READ_PATH -maxdepth 2 -type f -printf "%p\n"); do

    wl_file=$(basename $WL_PATH)
    wl_name=$(echo $wl_file | cut -f 1 -d '.') #removes .f
    wl_type=$(echo $wl_file | cut -f 1 -d '-') #removes .f

    if [ "$RUN_ITER" = "rand" ]; then
        NEW_WORKLOAD_VAR_PARALELIZATION_LIMIT=("4k","16k","32k")
    else
        NEW_WORKLOAD_VAR_PARALELIZATION_LIMIT=$WORKLOAD_VAR_PARALELIZATION_LIMIT
    fi

    for WL_CONF_PARAL_LIMIT in $NEW_WORKLOAD_VAR_PARALELIZATION_LIMIT

        sed -i "/limit_write_paralelization_to.*/c\    limit_write_paralelization_to: $WL_CONF_PARAL_LIMIT" $CONFIG_FILE
        sed -i "/limit_read_paralelization_to.*/c\    limit_read_paralelization_to: $WL_CONF_PARAL_LIMIT" $CONFIG_FILE

        reset_client

        if [ "$RUN_ITER" = "rand" && "$WL_CONF_PARAL_LIMIT" = "4k" ]; then
            NEW_WORKLOAD_VAR_IO_SIZE=("4k")
        elif [ "$RUN_ITER" = "rand" && "$WL_CONF_PARAL_LIMIT" = "16k" ]; then
            NEW_WORKLOAD_VAR_IO_SIZE=("32k")
        elif [ "$RUN_ITER" = "rand" && "$WL_CONF_PARAL_LIMIT" = "32k" ]; then
            NEW_WORKLOAD_VAR_IO_SIZE=("32k")
        else 
            NEW_WORKLOAD_VAR_IO_SIZE=$WORKLOAD_VAR_IO_SIZE
        fi

        for WL_CONF_IO in $NEW_WORKLOAD_VAR_IO_SIZE
        
            sed -i "/set \$IO_SIZE.*/c\set \$IO_SIZE=$WL_CONF_IO" $WL_PATH

            output_results_file="$OUTPUT_PATH/$WORKLOAD_TYPE/run-$wl_name-$WL_CONF_IO-$WL_CONF_PARAL_LIMIT-lsfs-fb.output"
            touch $output_results_file

            for ((RUN_ITER=1; RUN_ITER<=NR_OF_ITERATIONS_PER_WORKLOAD; RUN_ITER++)); do

                get_client_pod_name

                echo -e "\nRun: #$RUN_ITER,wl_name:$wl_name-$WL_CONF_IO-$WL_CONF_PARAL_LIMIT,wl_path:$WL_PATH,fs:lsfs\n\n" >> $output_results_file

                echo "Starting dstat in all $NR_PEERS_IN_CLUSTER peers - $i x."

                for ((PEER_NR=1; PEER_NR<=NR_PEERS_IN_CLUSTER; PEER_NR++)); do

                    start_dstat "$wl_name-$WL_CONF_IO-$WL_CONF_PARAL_LIMIT" $PEER_NR
                
                done

                run_fb_workload $WL_PATH $output_results_file

                echo "Stopping dstat - $i x."

                for ((PEER_NR=1; PEER_NR<=NR_PEERS_IN_CLUSTER; PEER_NR++)); do

                    stop_dstat $PEER_NR
                
                done
            
            done
        
            i+=$RUN_ITER

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
    
#     i+=$RUN_ITER

# done