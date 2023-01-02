#!/bin/bash

sudo apt-get update -y

## DOCKER

sudo apt-get install docker.io -y
sudo systemctl enable docker
sudo systemctl start docker

echo '{"exec-opts": ["native.cgroupdriver=systemd"]}'| sudo tee /etc/docker/daemon.json

sudo systemctl restart docker

sleep 10

## KUBERNETES

curl -s https://packages.cloud.google.com/apt/doc/apt-key.gpg | sudo apt-key add -
cat <<EOF | sudo tee /etc/apt/sources.list.d/kubernetes.list
deb https://apt.kubernetes.io/ kubernetes-xenial main
EOF

sudo apt-get update
sudo apt-get install -y kubelet=1.25.3-00 kubeadm=1.25.3-00 kubectl=1.25.3-00
sudo apt-mark hold kubelet kubeadm kubectl

sudo swapoff -a
sudo sed -i '/ swap / s/^\(.*\)$/#\1/g' /etc/fstab

# FOR ANSIBLE
sudo apt install -y python3-pip
sudo pip install openshift pyyaml==5.4.1 kubernetes kubernetes-validate google-cloud-storage requests google-auth boto

sudo apt -y install unzip