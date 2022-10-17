#!/bin/bash

#This script runs all the workloads in the WORKLOADS_PATH directory


CONTAINER_COM_DIRECTORY=shared_dir

REMOTE_COM_DIRECTORY=lsfs/single_node/shared_dir

IMAGE_NAME=brancc0cdocker/lsfs-local
#------------------------------------------

MOUNT_POINT=/test_filesystem/InnerFolder


CONTAINER_WORKLOADS_PATH=$CONTAINER_COM_DIRECTORY/workloads-filebench
CONTAINER_WORKLOADS_READ_PATH=$CONTAINER_WORKLOADS_PATH/read-data-micro
CONTAINER_WORKLOADS_WRITE_PATH=$CONTAINER_WORKLOADS_PATH/write-data-micro
CONTAINER_WORKLOADS_METADATA_PATH=$CONTAINER_WORKLOADS_PATH/metadata-micro

# Workloads MASTER variables

REMOTE_WORKLOADS_PATH=$REMOTE_COM_DIRECTORY/workloads-filebench
REMOTE_WORKLOADS_READ_PATH=$REMOTE_WORKLOADS_PATH/read-data-micro
REMOTE_WORKLOADS_WRITE_PATH=$REMOTE_WORKLOADS_PATH/write-data-micro
REMOTE_WORKLOADS_METADATA_PATH=$REMOTE_WORKLOADS_PATH/metadata-micro

# Workloads LOCAL variables

LOCAL_WORKLOADS_PATH=../workloads-filebench
LOCAL_WORKLOADS_READ_PATH=$LOCAL_WORKLOADS_PATH/read-data-micro
LOCAL_WORKLOADS_WRITE_PATH=$LOCAL_WORKLOADS_PATH/write-data-micro
LOCAL_WORKLOADS_METADATA_PATH=$LOCAL_WORKLOADS_PATH/metadata-micro

#------------------------------------------

# Output variables

LOCAL_OUTPUT_PATH=outputs

LOCAL_DSTAT_OUTPUT_PATH=dstat_outputs

#------------------------------------------


RUNTIME_PER_WORKLOAD=900 #seconds
NR_OF_ITERATIONS_PER_WORKLOAD=3

WORKLOAD_VAR_IO_SIZE=("4k" "128k")
WORKLOAD_VAR_PARALELIZATION_LIMIT=("4k")
WORKLOAD_VAR_LB_TYPE=("smart" "dynamic")
WORKLOAD_VAR_CACHE=("cache_on" "cache_off");
WORKLOAD_VAR_CACHE_REFRESH_TIME=("1000" "30000" "60000")



###########################################################################################################
#                     Run all filebench workloads                      ####################################
###########################################################################################################

# Setup Workloads

for WL_PATH in $(find $LOCAL_WORKLOADS_PATH -maxdepth 4 -type f -printf "%p\n"); do

    sed -i "/set \$WORKLOAD_PATH.*/c\set \$WORKLOAD_PATH=$MOUNT_POINT" $WL_PATH
    sed -i "/set \$WORKLOAD_RUNTIME.*/c\set \$WORKLOAD_RUNTIME=$RUNTIME_PER_WORKLOAD" $WL_PATH

done



ansible-playbook deploy/1_setup_deploy.yml -e "remote_com_directory=$REMOTE_COM_DIRECTORY" -i deploy/hosts -v

WORKLOAD_TYPE=write
OUTPUT_PATH="outputs-run-$(date +"%Y_%m_%d_%I_%M_%p")"

LOAD_BALANCER=dynamic
USE_CACHE=False
CACHE_REFRESH=1000

mkdir -p $LOCAL_OUTPUT_PATH/$OUTPUT_PATH/$WORKLOAD_TYPE

mkdir -p $LOCAL_OUTPUT_PATH/$OUTPUT_PATH/$LOCAL_DSTAT_OUTPUT_PATH

for WL_PATH in $(find $LOCAL_WORKLOADS_WRITE_PATH -maxdepth 2 -type f -printf "%p\n"); do

    wl_file=$(basename $WL_PATH)
    wl_name=$(echo $wl_file | cut -f 1 -d '.') #removes .f
    wl_remote_path=$REMOTE_WORKLOADS_WRITE_PATH/$wl_file
    wl_container_path=$CONTAINER_WORKLOADS_WRITE_PATH/$wl_file

    for WL_CONF_IO in ${WORKLOAD_VAR_IO_SIZE[@]}; do

        if [ "$WL_CONF_IO" = "4k" ]; then
            NEW_WORKLOAD_VAR_PARALELIZATION_LIMIT=("4k")
        else
            NEW_WORKLOAD_VAR_PARALELIZATION_LIMIT=( ${WORKLOAD_VAR_PARALELIZATION_LIMIT[@]} )
        fi

        for WL_CONF_PARAL_LIMIT in ${NEW_WORKLOAD_VAR_PARALELIZATION_LIMIT[@]}; do

            ansible-playbook deploy/change_run_config.yml -e "remote_com_directory=$REMOTE_COM_DIRECTORY load_balancer=$LOAD_BALANCER use_cache=$USE_CACHE cache_refresh=$CACHE_REFRESH paralelization=$WL_CONF_PARAL_LIMIT wl_conf_io=$WL_CONF_IO wl_path=$wl_remote_path" -i deploy/hosts -v
        
            WL_CONF_NAME="$wl_name-$WL_CONF_IO-$LOAD_BALANCER-$WL_CONF_PARAL_LIMIT"

            OUTPUT_FILE_PATH=$LOCAL_OUTPUT_PATH/$OUTPUT_PATH/$WORKLOAD_TYPE/run-$WL_CONF_NAME-lsfs-fb.output
            DSTAT_FILE_PATH=$LOCAL_OUTPUT_PATH/$OUTPUT_PATH/$LOCAL_DSTAT_OUTPUT_PATH/run-$WL_CONF_NAME-lsfs

            mkdir -p $DSTAT_FILE_PATH
    
            touch $OUTPUT_FILE_PATH

            for ((RUN_ITER=1; RUN_ITER<=NR_OF_ITERATIONS_PER_WORKLOAD; RUN_ITER++)); do

                ansible-playbook deploy/2_container_deploy.yml -e "remote_com_directory=$REMOTE_COM_DIRECTORY container_com_directory=$CONTAINER_COM_DIRECTORY image_name=$IMAGE_NAME" -i deploy/hosts -v

                echo -e "\nRun: #$RUN_ITER,wl_name:$WL_CONF_NAME,wl_path:$wl_container_path,fs:lsfs\n\n" >> $OUTPUT_FILE_PATH

                ansible-playbook deploy/3_run_workload.yml -e "container_com_directory=$CONTAINER_COM_DIRECTORY remote_com_directory=$REMOTE_COM_DIRECTORY wl_name=$WL_CONF_NAME wl_path=$wl_container_path output_path=$OUTPUT_FILE_PATH dstat_path=$DSTAT_FILE_PATH" -i deploy/hosts -v
            
                ansible-playbook deploy/4_shutdown_container.yml -i deploy/hosts -v

                ansible-playbook deploy/clean_peer_db.yml -e "remote_com_directory=$REMOTE_COM_DIRECTORY" -i deploy/hosts -v
                        
            done
                    
        done

    done

done

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
    wl_container_path=$CONTAINER_WORKLOADS_READ_PATH/$wl_file

    for WL_CONF_IO in ${WORKLOAD_VAR_IO_SIZE[@]}; do

        if [ "$WL_CONF_IO" = "4k" ]; then
            NEW_WORKLOAD_VAR_PARALELIZATION_LIMIT=("4k")
        else
            NEW_WORKLOAD_VAR_PARALELIZATION_LIMIT=( ${WORKLOAD_VAR_PARALELIZATION_LIMIT[@]} )
        fi

        for WL_CONF_PARAL_LIMIT in ${NEW_WORKLOAD_VAR_PARALELIZATION_LIMIT[@]}; do

            ansible-playbook deploy/change_run_config.yml -e "remote_com_directory=$REMOTE_COM_DIRECTORY load_balancer=$LOAD_BALANCER use_cache=$USE_CACHE cache_refresh=$CACHE_REFRESH paralelization=$WL_CONF_PARAL_LIMIT wl_conf_io=$WL_CONF_IO wl_path=$wl_remote_path" -i deploy/hosts -v
        
            WL_CONF_NAME="$wl_name-$WL_CONF_IO-$LOAD_BALANCER-$WL_CONF_PARAL_LIMIT"

            OUTPUT_FILE_PATH=$LOCAL_OUTPUT_PATH/$OUTPUT_PATH/$WORKLOAD_TYPE/run-$WL_CONF_NAME-lsfs-fb.output
            DSTAT_FILE_PATH=$LOCAL_OUTPUT_PATH/$OUTPUT_PATH/$LOCAL_DSTAT_OUTPUT_PATH/run-$WL_CONF_NAME-lsfs

            mkdir -p $DSTAT_FILE_PATH

            touch $OUTPUT_FILE_PATH

            for ((RUN_ITER=1; RUN_ITER<=NR_OF_ITERATIONS_PER_WORKLOAD; RUN_ITER++)); do

                ansible-playbook deploy/2_container_deploy.yml -e "remote_com_directory=$REMOTE_COM_DIRECTORY container_com_directory=$CONTAINER_COM_DIRECTORY image_name=$IMAGE_NAME" -i deploy/hosts -v

                echo -e "\nRun: #$RUN_ITER,wl_name:$WL_CONF_NAME,wl_path:$wl_container_path,fs:lsfs\n\n" >> $OUTPUT_FILE_PATH

                ansible-playbook deploy/3_run_workload.yml -e "container_com_directory=$CONTAINER_COM_DIRECTORY remote_com_directory=$REMOTE_COM_DIRECTORY wl_name=$WL_CONF_NAME wl_path=$wl_container_path output_path=$OUTPUT_FILE_PATH dstat_path=$DSTAT_FILE_PATH" -i deploy/hosts -v
            
                ansible-playbook deploy/4_shutdown_container.yml -i deploy/hosts -v
                        
            done
                    
        done

    done

done


ansible-playbook deploy/clean_peer_db.yml -e "remote_com_directory=$REMOTE_COM_DIRECTORY" -i deploy/hosts -v

#------------------------------------------

# Run metadata workloads

WORKLOAD_TYPE=metadata

LOAD_BALANCER=dynamic

WL_CONF_PARAL_LIMIT="4k"
WL_CONF_IO="4k"

mkdir -p $LOCAL_OUTPUT_PATH/$OUTPUT_PATH/$WORKLOAD_TYPE

for WL_PATH in $(find $LOCAL_WORKLOADS_METADATA_PATH -maxdepth 2 -type f -printf "%p\n"); do

    wl_file=$(basename $WL_PATH)
    wl_name=$(echo $wl_file | cut -f 1 -d '.') #removes .f
    wl_remote_path=$REMOTE_WORKLOADS_METADATA_PATH/$wl_file
    wl_container_path=$CONTAINER_WORKLOADS_METADATA_PATH/$wl_file

    for CACHE_CONF in ${WORKLOAD_VAR_CACHE[@]}; do

        if [ "$CACHE_CONF" = "cache_off" ]; then
            NEW_CACHE_REFRESH=("1000")
            USE_CACHE=False
        else
            NEW_CACHE_REFRESH=( ${WORKLOAD_VAR_CACHE_REFRESH_TIME[@]} )
            USE_CACHE=true
        fi

        for REFRESH_CONF in ${NEW_CACHE_REFRESH[@]}; do

            ansible-playbook deploy/change_run_config.yml -e "remote_com_directory=$REMOTE_COM_DIRECTORY load_balancer=$LOAD_BALANCER use_cache=$USE_CACHE cache_refresh=$REFRESH_CONF paralelization=$WL_CONF_PARAL_LIMIT wl_conf_io=$WL_CONF_IO wl_path=$wl_remote_path" -i deploy/hosts -v
        
            if [ "$USE_CACHE" = "False" ]; then
                WL_CONF_NAME="$wl_name-$CACHE_CONF"
            else 
                WL_CONF_NAME="$wl_name-$CACHE_CONF-$REFRESH_CONF"
            fi

            OUTPUT_FILE_PATH=$LOCAL_OUTPUT_PATH/$OUTPUT_PATH/$WORKLOAD_TYPE/run-$WL_CONF_NAME-lsfs-fb.output
            DSTAT_FILE_PATH=$LOCAL_OUTPUT_PATH/$OUTPUT_PATH/$LOCAL_DSTAT_OUTPUT_PATH/run-$WL_CONF_NAME-lsfs

            mkdir -p $DSTAT_FILE_PATH

            touch $OUTPUT_FILE_PATH

            for ((RUN_ITER=1; RUN_ITER<=NR_OF_ITERATIONS_PER_WORKLOAD; RUN_ITER++)); do

                ansible-playbook deploy/2_container_deploy.yml -e "remote_com_directory=$REMOTE_COM_DIRECTORY container_com_directory=$CONTAINER_COM_DIRECTORY image_name=$IMAGE_NAME" -i deploy/hosts -v

                sleep 10

                echo -e "\nRun: #$RUN_ITER,wl_name:$WL_CONF_NAME,wl_path:$wl_container_path,fs:lsfs\n\n" >> $OUTPUT_FILE_PATH

                ansible-playbook deploy/3_run_workload.yml -e "container_com_directory=$CONTAINER_COM_DIRECTORY remote_com_directory=$REMOTE_COM_DIRECTORY wl_name=$WL_CONF_NAME wl_path=$wl_container_path output_path=$OUTPUT_FILE_PATH dstat_path=$DSTAT_FILE_PATH" -i deploy/hosts -v
            
                ansible-playbook deploy/4_shutdown_container.yml -i deploy/hosts -v

                ansible-playbook deploy/clean_peer_db.yml -e "remote_com_directory=$REMOTE_COM_DIRECTORY" -i deploy/hosts -v
                                    
            done
        
        done

    done
    
done
