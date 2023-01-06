#!/bin/bash

sudo apt-add-repository -y ppa:ansible/ansible
sudo apt update
sudo apt -y install ansible

# FOR ANSIBLE
sudo apt install -y python3-pip
sudo pip install openshift pyyaml==5.4.1 kubernetes kubernetes-validate google-cloud-storage requests google-auth boto

git clone https://github.com/dsrhaslab/LSFS.git ~