#!/bin/bash

#This script runs all the workloads in the WORKLOADS_PATH directory

COM_DIRECTORY=shared_dir
MOUNT_POINT=/test_filesystem/InnerFolder

IMAGE_NAME=brancc0cdocker/lsfs-local
#------------------------------------------

# Workloads MASTER variables

REMOTE_WORKLOADS_PATH=$COM_DIRECTORY/workloads-filebench
REMOTE_WORKLOADS_READ_PATH=$REMOTE_WORKLOADS_PATH/read-data-micro
REMOTE_WORKLOADS_WRITE_PATH=$REMOTE_WORKLOADS_PATH/write-data-micro
REMOTE_WORKLOADS_METADATA_PATH=$REMOTE_WORKLOADS_PATH/metadata-micro

# Workloads LOCAL variables

LOCAL_WORKLOADS_PATH=../workloads-filebench
LOCAL_WORKLOADS_READ_PATH=$LOCAL_WORKLOADS_PATH/read-data-micro
LOCAL_WORKLOADS_WRITE_PATH=$LOCAL_WORKLOADS_PATH/write-data-micro
LOCAL_WORKLOADS_METADATA_PATH=$LOCAL_WORKLOADS_PATH/metadata-micro

#------------------------------------------

# Metrics variables

METRICS_PATH=$COM_DIRECTORY/metrics

CONFIG_PATH=$COM_DIRECTORY/conf.yml

# Output variables

LOCAL_OUTPUT_PATH=outputs


#------------------------------------------


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



###########################################################################################################
#                     Run all filebench workloads                      ####################################
###########################################################################################################

# Setup Workloads

for WL_PATH in $(find $LOCAL_WORKLOADS_PATH -maxdepth 4 -type f -printf "%p\n"); do

    sed -i "/set \$WORKLOAD_PATH.*/c\set \$WORKLOAD_PATH=$MOUNT_POINT" $WL_PATH
    sed -i "/set \$WORKLOAD_RUNTIME.*/c\set \$WORKLOAD_RUNTIME=$RUNTIME_PER_WORKLOAD" $WL_PATH

done



ansible-playbook deploy/1_setup_deploy.yml -i hosts -v

WORKLOAD_TYPE=write
OUTPUT_PATH="outputs-run-$(date +"%Y_%m_%d_%I_%M_%p")"

LOAD_BALANCER=dynamic
USE_CACHE=False
CACHE_REFRESH=1000

mkdir -p $LOCAL_OUTPUT_PATH/$OUTPUT_PATH/$WORKLOAD_TYPE

for WL_PATH in $(find $LOCAL_WORKLOADS_WRITE_PATH -maxdepth 2 -type f -printf "%p\n"); do

    wl_file=$(basename $WL_PATH)
    wl_name=$(echo $wl_file | cut -f 1 -d '.') #removes .f
    wl_remote_path=$REMOTE_WORKLOADS_WRITE_PATH/$wl_file

    for WL_CONF_PARAL_LIMIT in ${WORKLOAD_VAR_PARALELIZATION_LIMIT[@]}; do

        for WL_CONF_IO in ${WORKLOAD_VAR_IO_SIZE[@]}; do

            ansible-playbook deploy/change_run_config.yml -e "config_file=$CONFIG_PATH load_balancer=$LOAD_BALANCER use_cache=$USE_CACHE cache_refresh=$CACHE_REFRESH paralelization=$WL_CONF_PARAL_LIMIT wl_conf_io=$WL_CONF_IO wl_path=$wl_remote_path" -i hosts -v
        
            WL_CONF_NAME="$wl_name-$WL_CONF_IO-$LOAD_BALANCER-$WL_CONF_PARAL_LIMIT"

            OUTPUT_FILE_PATH=$LOCAL_OUTPUT_PATH/$OUTPUT_PATH/$WORKLOAD_TYPE/run-$WL_CONF_NAME-lsfs-fb.output

            touch $OUTPUT_FILE_PATH

            for ((RUN_ITER=1; RUN_ITER<=NR_OF_ITERATIONS_PER_WORKLOAD; RUN_ITER++)); do

                ansible-playbook deploy/2_container_deploy.yml -e "image_name=$IMAGE_NAME" -i hosts -v

                echo -e "\nRun: #$RUN_ITER,wl_name:$WL_CONF_NAME,wl_path:$wl_remote_path,fs:lsfs\n\n" >> $OUTPUT_FILE_PATH

                ansible-playbook deploy/3_run_workload.yml -e "com_directory=$COM_DIRECTORY metrics_path=$METRICS_PATH wl_name=$WL_CONF_NAME wl_path=$wl_remote_path output_path=$OUTPUT_FILE_PATH" -i hosts -v
            
                ansible-playbook deploy/4_shutdown_container.yml -i hosts -v

                ansible-playbook deploy/clean_peer_db.yml -i hosts -v
                        
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

    for WL_CONF_PARAL_LIMIT in ${WORKLOAD_VAR_PARALELIZATION_LIMIT[@]}; do

        for WL_CONF_IO in ${WORKLOAD_VAR_IO_SIZE[@]}; do

            ansible-playbook deploy/change_run_config.yml -e "config_file=$CONFIG_PATH load_balancer=$LOAD_BALANCER use_cache=$USE_CACHE cache_refresh=$CACHE_REFRESH paralelization=$WL_CONF_PARAL_LIMIT wl_conf_io=$WL_CONF_IO wl_path=$wl_remote_path" -i hosts -v
        
            WL_CONF_NAME="$wl_name-$WL_CONF_IO-$LOAD_BALANCER-$WL_CONF_PARAL_LIMIT"

            OUTPUT_FILE_PATH=$LOCAL_OUTPUT_PATH/$OUTPUT_PATH/$WORKLOAD_TYPE/run-$WL_CONF_NAME-lsfs-fb.output

            touch $OUTPUT_FILE_PATH

            for ((RUN_ITER=1; RUN_ITER<=NR_OF_ITERATIONS_PER_WORKLOAD; RUN_ITER++)); do

                ansible-playbook deploy/2_container_deploy.yml -e "image_name=$IMAGE_NAME" -i hosts -v

                echo -e "\nRun: #$RUN_ITER,wl_name:$WL_CONF_NAME,wl_path:$wl_remote_path,fs:lsfs\n\n" >> $OUTPUT_FILE_PATH

                ansible-playbook deploy/3_run_workload.yml -e "com_directory=$COM_DIRECTORY metrics_path=$METRICS_PATH wl_name=$WL_CONF_NAME wl_path=$wl_remote_path output_path=$OUTPUT_FILE_PATH" -i hosts -v
            
                ansible-playbook deploy/4_shutdown_container.yml -i hosts -v

                ansible-playbook deploy/clean_peer_db.yml -i hosts -v
                        
            done
                    
        done

    done

done


#------------------------------------------

# Run metadata workloads

WORKLOAD_TYPE=metadata

LOAD_BALANCER=dynamic

WL_CONF_PARAL_LIMIT="128k"
WL_CONF_IO="128k"

mkdir -p $LOCAL_OUTPUT_PATH/$OUTPUT_PATH/$WORKLOAD_TYPE

for WL_PATH in $(find $LOCAL_WORKLOADS_METADATA_PATH -maxdepth 2 -type f -printf "%p\n"); do

    wl_file=$(basename $WL_PATH)
    wl_name=$(echo $wl_file | cut -f 1 -d '.') #removes .f
    wl_remote_path=$REMOTE_WORKLOADS_METADATA_PATH/$wl_file

    for CACHE_CONF in ${WORKLOAD_VAR_CACHE[@]}; do

        if [ "$CACHE_CONF" = "cache_off" ]; then
            NEW_CACHE_REFRESH=()
            USE_CACHE=False
        else
            NEW_CACHE_REFRESH=( ${WORKLOAD_VAR_CACHE_REFRESH_TIME[@]} )
            USE_CACHE=true
        fi

        for REFRESH_CONF in ${NEW_CACHE_REFRESH[@]}; do

            ansible-playbook deploy/change_run_config.yml -e "config_file=$CONFIG_PATH load_balancer=$LOAD_BALANCER use_cache=$USE_CACHE cache_refresh=$REFRESH_CONF paralelization=$WL_CONF_PARAL_LIMIT wl_conf_io=$WL_CONF_IO wl_path=$wl_remote_path" -i hosts -v
        
            WL_CONF_NAME="$wl_name-$WL_CONF_IO-$LOAD_BALANCER-$WL_CONF_PARAL_LIMIT"

            OUTPUT_FILE_PATH=$LOCAL_OUTPUT_PATH/$OUTPUT_PATH/$WORKLOAD_TYPE/run-$WL_CONF_NAME-lsfs-fb.output

            touch $OUTPUT_FILE_PATH

            for ((RUN_ITER=1; RUN_ITER<=NR_OF_ITERATIONS_PER_WORKLOAD; RUN_ITER++)); do

                ansible-playbook deploy/2_container_deploy.yml -e "image_name=$IMAGE_NAME" -i hosts -v

                echo -e "\nRun: #$RUN_ITER,wl_name:$WL_CONF_NAME,wl_path:$wl_remote_path,fs:lsfs\n\n" >> $OUTPUT_FILE_PATH

                ansible-playbook deploy/3_run_workload.yml -e "com_directory=$COM_DIRECTORY metrics_path=$METRICS_PATH wl_name=$WL_CONF_NAME wl_path=$wl_remote_path output_path=$OUTPUT_FILE_PATH" -i hosts -v
            
                ansible-playbook deploy/4_shutdown_container.yml -i hosts -v

                ansible-playbook deploy/clean_peer_db.yml -i hosts -v
                                    
            done
        
        done

    done
    
done
