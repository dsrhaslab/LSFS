#!/bin/bash

# Change these variables
# ======================================================================================
REPO="monarch"
USER="/home/brancojse"
WORKSPACE=${USER}/$REPO

SCRIPT_DIR="${WORKSPACE}/tensorflow/models/official-models-2.1.0/official/vision/image_classification"
CHECKPOINTING_DIR="/tmp/checkpointing"
# ======================================================================================

# Default values
# DATASET_DIR="${USER}/lsfs/test_filesystem/InnerFolder/100g_tfrecords"
DATASET_DIR="${USER}/external/100g_tfrecords"
BATCH_SIZE=64
EPOCHS=3
NUM_GPUS=1

DATE="$(date +%Y_%m_%d-%H_%M)"
RUN_NAME="lenet-bs$BATCH_SIZE-ep$EPOCHS-$DATE"
RUN_DIR="${USER}/$RUN_NAME"

# Create results directory
mkdir -p $RUN_DIR

# Create log file
touch $RUN_DIR/log.txt

export PYTHONPATH=$PYTHONPATH:${WORKSPACE}/tensorflow/models/official-models-2.1.0

echo -e "Model: LeNet\nDataset: ImageNet\nBatch size: $BATCH_SIZE\nEpochs: $EPOCHS\nGPUs: $NUM_GPUS\nFramework: Tensorflow \nDataset:${DATASET_DIR}"> $RUN_DIR/info.txt
python3 $SCRIPT_DIR/lenet_imagenet_main.py $SKIP_EVAL --train_epochs=$EPOCHS --batch_size=$BATCH_SIZE --model_dir=$CHECKPOINTING_DIR --data_dir=$DATASET_DIR --num_gpus=$NUM_GPUS |& tee $RUN_DIR/log.txt