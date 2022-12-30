#! /bin/bash

# Change tmp dir (sometimes tensorflow cant be installed because insufficient memory in tmp dir)
# mkdir aux
# cd aux
# export TMPDIR=$(pwd)
# cd
 
# Install tensorflow
pip install tensorflow

# Setup scripts to run training 
git clone https://github.com/dsrhaslab/monarch.git

## Install requirements
cd monarch/tensorflow/models/official-models-2.1.0/official/ 
pip3 install --user -r requirements.txt

## Comment some lines not to use "pastor" tool
sed -i '36{s/^/#/}' vision/image_classification/imagenet_preprocessing.py
sed -i '146,155{s/^/#/}' vision/image_classification/imagenet_preprocessing.py
sed -i '287{s/^/#/}' vision/image_classification/imagenet_preprocessing.py
sed -i '286{s/^..#/  /}' vision/image_classification/imagenet_preprocessing.py

## Changing train image size to dataset image size (103GB) = 896772 images
sed -i "s/'train':.*$/'train': 896772,/" vision/image_classification/imagenet_preprocessing.py