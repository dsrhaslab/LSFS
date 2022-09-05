#!/bin/bash

COM_DIRECTORY=shared_dir

#------------------------------------------

# Workloads variables

WORKLOADS_PATH=$COM_DIRECTORY/workloads-filebench
RUNTIME_PER_WORKLOAD=600 #seconds
NR_OF_ITERATIONS_PER_WORKLOAD=2

#------------------------------------------

# Metrics variables

METRICS_PATH=$COM_DIRECTORY/metrics


#------------------------------------------

# Nodes data variables

CONFIG_FILE=$COM_DIRECTORY/conf.yaml
PEERS_IPS_FILE=$COM_DIRECTORY/peer_ips
NR_PEERS_IN_CLUSTER=100
BOOTSTRAPPER_IP


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



########################################################################
#                     Run all filebench workloads                      #
########################################################################

mkdir -p $OUTPUT_PATH

#read_peer_hosts_file $PEERS_IPS_FILE

get_bootstrapper_ip

i=0

for WL_PATH in $(find $WORKLOADS_PATH -maxdepth 4 -type f -printf "%p\n"); do

    wl_file=$(basename $WL_PATH)
    wl_name=$(echo $wl_file | cut -f 1 -d '.') #removes .f

    output_results_file="$OUTPUT_PATH/run-$wl_name-lsfs-fb.output"
    touch $output_results_file

    for RUN_ITER in {1..$NR_OF_ITERATIONS_PER_WORKLOAD}; do

        echo -e "\nrun: #$RUN_ITER,wl_name:$wl_name,wl_path:$WL_PATH,fs:lsfs\n\n" >> $output_results_file

        for PEER_NR in {1..$NR_PEERS_IN_CLUSTER}; do

            dstat_log_name=run-$wl_name-lsfs-fb.dstat.csv

            kubectl exec -n lsfs peer$PEER_NR -- bash /$COM_DIRECTORY/scripts/init_dstat.sh $METRICS_PATH $PEER_NR $dstat_log_name
        
        done
    
        kubectl exec -it -n lsfs client1 -- /bin/bash -c "filebench -f $WL_PATH" &>> $output_results_file

        for PEER_NR in {1..$NR_PEERS_IN_CLUSTER}; do

            dstat_log_name=run-$wl_name-lsfs-fb.dstat.csv

            kubectl exec -n lsfs peer$PEER_NR -- bash /$COM_DIRECTORY/scripts/stop_dstat.sh
        
        done

        kubectl exec -it -n lsfs client1 -- /bin/bash -c "./build/client_exe $BOOTSTRAPPER_IP ${i+RUN_ITER} $CONFIG_FILE $PEERS_IPS_FILE"

        sleep 60
    
    done
    
    i+=$RUN_ITER

done
