#!/bin/bash

#This script runs all the workloads in the WORKLOADS_PATH directory

MOUNT_POINT=/test_filesystem/InnerFolder
#------------------------------------------

# Workloads MASTER variables

REMOTE_WORKLOADS_PATH=workloads-filebench
REMOTE_WORKLOADS_READ_PATH=$REMOTE_WORKLOADS_PATH/read-data-micro
REMOTE_WORKLOADS_WRITE_PATH=$REMOTE_WORKLOADS_PATH/write-data-micro
REMOTE_WORKLOADS_METADATA_PATH=$REMOTE_WORKLOADS_PATH/metadata-micro

# Workloads LOCAL variables

LOCAL_WORKLOADS_PATH=workloads-filebench
LOCAL_WORKLOADS_READ_PATH=$LOCAL_WORKLOADS_PATH/read-data-micro
LOCAL_WORKLOADS_WRITE_PATH=$LOCAL_WORKLOADS_PATH/write-data-micro
LOCAL_WORKLOADS_METADATA_PATH=$LOCAL_WORKLOADS_PATH/metadata-micro

#------------------------------------------

# Output variables

LOCAL_OUTPUT_PATH=outputs

#------------------------------------------


RUNTIME_PER_WORKLOAD=900 #seconds
NR_OF_ITERATIONS_PER_WORKLOAD=3

WORKLOAD_VAR_IO_SIZE=("4k" "128k")



###########################################################################################################
#                     Run all filebench workloads                      ####################################
###########################################################################################################

# Setup Workloads

for WL_PATH in $(find $LOCAL_WORKLOADS_PATH -maxdepth 4 -type f -printf "%p\n"); do

    sed -i "/set \$WORKLOAD_PATH.*/c\set \$WORKLOAD_PATH=$MOUNT_POINT" $WL_PATH
    sed -i "/set \$WORKLOAD_RUNTIME.*/c\set \$WORKLOAD_RUNTIME=$RUNTIME_PER_WORKLOAD" $WL_PATH

done



ansible-playbook deploy/1_setup_deploy.yml -i deploy/hosts -v

WORKLOAD_TYPE=write
OUTPUT_PATH="outputs-run-$(date +"%Y_%m_%d_%I_%M_%p")"


mkdir -p $LOCAL_OUTPUT_PATH/$OUTPUT_PATH/$WORKLOAD_TYPE

for WL_PATH in $(find $LOCAL_WORKLOADS_WRITE_PATH -maxdepth 2 -type f -printf "%p\n"); do

    wl_file=$(basename $WL_PATH)
    wl_name=$(echo $wl_file | cut -f 1 -d '.') #removes .f
    wl_remote_path=$REMOTE_WORKLOADS_WRITE_PATH/$wl_file

    for WL_CONF_IO in ${WORKLOAD_VAR_IO_SIZE[@]}; do

        WL_CONF_NAME="$wl_name-$WL_CONF_IO"

        OUTPUT_FILE_PATH=$LOCAL_OUTPUT_PATH/$OUTPUT_PATH/$WORKLOAD_TYPE/run-$WL_CONF_NAME-pf.output
        
        touch $OUTPUT_FILE_PATH

        for ((RUN_ITER=1; RUN_ITER<=NR_OF_ITERATIONS_PER_WORKLOAD; RUN_ITER++)); do

            ansible-playbook deploy/2_container_deploy.yml -i deploy/hosts -v

            echo -e "\nRun: #$RUN_ITER,wl_name:$WL_CONF_NAME,wl_path:$wl_remote_path,fs:passthrough\n\n" >> $OUTPUT_FILE_PATH

            ansible-playbook deploy/3_run_workload.yml -e "wl_path=$wl_remote_path output_path=$OUTPUT_FILE_PATH" -i deploy/hosts -v
        
            ansible-playbook deploy/4_shutdown_container.yml -i deploy/hosts -v

            sleep 300
                    
        done
                    
    done

done

sleep 300

#------------------------------------------

# Run read workloads

WORKLOAD_TYPE=read

mkdir -p $LOCAL_OUTPUT_PATH/$OUTPUT_PATH/$WORKLOAD_TYPE

for WL_PATH in $(find $LOCAL_WORKLOADS_READ_PATH -maxdepth 2 -type f -printf "%p\n"); do

    wl_file=$(basename $WL_PATH)
    wl_name=$(echo $wl_file | cut -f 1 -d '.') #removes .f
    wl_remote_path=$REMOTE_WORKLOADS_READ_PATH/$wl_file

    for WL_CONF_IO in ${WORKLOAD_VAR_IO_SIZE[@]}; do

        WL_CONF_NAME="$wl_name-$WL_CONF_IO"

        OUTPUT_FILE_PATH=$LOCAL_OUTPUT_PATH/$OUTPUT_PATH/$WORKLOAD_TYPE/run-$WL_CONF_NAME-pf.output
        
        touch $OUTPUT_FILE_PATH

        for ((RUN_ITER=1; RUN_ITER<=NR_OF_ITERATIONS_PER_WORKLOAD; RUN_ITER++)); do

            ansible-playbook deploy/2_container_deploy.yml -i deploy/hosts -v

            echo -e "\nRun: #$RUN_ITER,wl_name:$WL_CONF_NAME,wl_path:$wl_remote_path,fs:passthrough\n\n" >> $OUTPUT_FILE_PATH

            ansible-playbook deploy/3_run_workload.yml -e "wl_path=$wl_remote_path output_path=$OUTPUT_FILE_PATH" -i deploy/hosts -v
        
            ansible-playbook deploy/4_shutdown_container.yml -i deploy/hosts -v
            
            sleep 300
        done
                
    done

done

sleep 300
#------------------------------------------

# Run metadata workloads

WORKLOAD_TYPE=metadata

mkdir -p $LOCAL_OUTPUT_PATH/$OUTPUT_PATH/$WORKLOAD_TYPE

for WL_PATH in $(find $LOCAL_WORKLOADS_METADATA_PATH -maxdepth 2 -type f -printf "%p\n"); do

    wl_file=$(basename $WL_PATH)
    wl_name=$(echo $wl_file | cut -f 1 -d '.') #removes .f
    wl_remote_path=$REMOTE_WORKLOADS_METADATA_PATH/$wl_file

   
    WL_CONF_NAME="$wl_name"

    OUTPUT_FILE_PATH=$LOCAL_OUTPUT_PATH/$OUTPUT_PATH/$WORKLOAD_TYPE/run-$WL_CONF_NAME-pf.output


    touch $OUTPUT_FILE_PATH

    for ((RUN_ITER=1; RUN_ITER<=NR_OF_ITERATIONS_PER_WORKLOAD; RUN_ITER++)); do

        ansible-playbook deploy/2_container_deploy.yml -i deploy/hosts -v

        echo -e "\nRun: #$RUN_ITER,wl_name:$WL_CONF_NAME,wl_path:$wl_remote_path,fs:passthrough\n\n" >> $OUTPUT_FILE_PATH

        ansible-playbook deploy/3_run_workload.yml -e "wl_path=$wl_remote_path output_path=$OUTPUT_FILE_PATH" -i deploy/hosts -v
    
        ansible-playbook deploy/4_shutdown_container.yml -i deploy/hosts -v

        sleep 300     
    done

done
