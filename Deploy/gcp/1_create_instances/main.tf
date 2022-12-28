
locals {
    
    # Iowa
    env_name            = "dev"
    env_project_name    = "lsfs"
    cluster_name        = "${local.env_name}-${local.env_project_name}-cluster"
}

provider "google" {
  project = var.project_id
  region  = var.region
}


module "vpc" {
  source       = "terraform-google-modules/network/google"
  project_id   = var.project_id
  network_name = "${local.env_name}-${local.env_project_name}-vpc"
  subnets = [
    {
      subnet_name   = "${local.env_name}-${local.env_project_name}-subnet-0"
      subnet_ip     = "10.10.0.0/16"
      subnet_region = var.region
    },
  ]
  secondary_ranges = {
    "${local.env_name}-${local.env_project_name}-subnet-0" = [
      {
        range_name    = "${local.env_name}-${local.env_project_name}-pods"
        ip_cidr_range = "10.20.0.0/16"
      },
      {
        range_name    = "${local.env_name}-${local.env_project_name}-services"
        ip_cidr_range = "10.30.0.0/16"
      },
    ]
  }
}

resource "google_compute_firewall" "allow-internal" {
  name    = "${local.env_name}-${local.env_project_name}-fw-allow-internal"
  network = module.vpc.network_name
  allow {
    protocol = "icmp"
  }
  allow {
    protocol = "ipip"
  }
  allow {
    protocol = "tcp"
    ports    = ["0-65535"]
  }
  allow {
    protocol = "udp"
    ports    = ["0-65535"]
  }
  source_ranges = [
    "10.10.0.0/16",
    "10.20.0.0/16",
    "10.30.0.0/16"
  ]
}
resource "google_compute_firewall" "allow-external" {
  name    = "${local.env_name}-${local.env_project_name}-fw-allow-external"
  network = module.vpc.network_name
  allow {
    protocol = "tcp"
    ports    = ["80", "8080", "1000-2000", "6443", "22"]
  }
  allow {
    protocol = "icmp"
  }
  source_ranges =["0.0.0.0/0"] 
}


module "peer_instances" {
  source      = "./modules/instance"
  project_id  = var.project_id
  region      = var.region 
  zone        = var.zones[0]
  
  instance_count = var.peer_node_count
  instance = {
    name      = "peer" 
    type      = "e2-small"
    tags      = ["ssh"]
    boot_disk   = {
      image     = "ubuntu-os-cloud/ubuntu-2004-lts"
      size      = 25
      type      = "pd-ssd"
    }
  }
  
  network     = module.vpc.network_name
  subnetwork  = module.vpc.subnets_names[0]

  ssh_key_metadata = "${var.nodes_user}:${chomp(file(var.ssh_path))}"

  label = "peer"

  startup_script = file("startup_script.sh")
}


module "client_instances" {
  source      = "./modules/instance"
  project_id  = var.project_id
  region      = var.region 
  zone        = var.zones[0]
  
  instance_count = var.client_node_count
  instance = {
    name      = "client" 
    type      = "n1-standard-2"
    tags      = ["ssh"]
    boot_disk   = {
      image     = "ubuntu-os-cloud/ubuntu-2004-lts"
      size      = 15
      type      = "pd-ssd"
    }
    # gpu = {
    #   count = 1
    #   type = "nvidia-tesla-t4"
    # }
  }
  
  
  network     = module.vpc.network_name
  subnetwork  = module.vpc.subnets_names[0]

  ssh_key_metadata = "${var.nodes_user}:${chomp(file(var.ssh_path))}"
  label = "client"

  startup_script = file("startup_script.sh")
}

module "master" {
  source      = "./modules/instance"
  project_id  = var.project_id
  region      = var.region 
  zone        = var.zones[0]
  
  instance_count = var.master_count
  instance = {
    name      = "master" 
    type      = "n1-standard-2"
    tags      = ["ssh"]
    boot_disk   = {
      image     = "ubuntu-os-cloud/ubuntu-2004-lts"
      size      = 15
      type      = "pd-ssd"
    }
  }
  
  network     = module.vpc.network_name
  subnetwork  = module.vpc.subnets_names[0]

  ssh_key_metadata = "${var.nodes_user}:${chomp(file(var.ssh_path))}"
  label = "master"

  startup_script = file("startup_script.sh")
}


resource "local_file" "instances_obj" {
  content  = templatefile("ips_template.tftpl", {
    master_obj = module.master.instances_info, 
    worker_obj_clients = module.client_instances.instances_info
    worker_obj_peers = module.peer_instances.instances_info
    })
  filename = "../2_cluster_deploy/hosts"
}
