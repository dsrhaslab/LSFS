
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


#module "gke_auth" {
#  source       = "terraform-google-modules/kubernetes-engine/google//modules/auth"
#  depends_on   = [module.gke]
#  project_id   = var.project_id
#  location     = module.gke.location
#  cluster_name = module.gke.name
#}

#resource "local_file" "kubeconfig" {
#  content  = module.gke_auth.kubeconfig_raw
#  filename = "${local.env_name}-${local.env_project_name}-kubeconfig"
#}

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

  firewall_rules = [{
    name                    = "allow-ssh-ingress"
    description             = null
    direction               = "INGRESS"
    priority                = 1000
    ranges                  = ["0.0.0.0/0"]
    allow = [{
      protocol = "tcp"
      ports    = ["22"]
    }]
  }]
}


module "gke" {
  source                 = "terraform-google-modules/kubernetes-engine/google"
  project_id             = var.project_id
  name                   = local.cluster_name
  create_service_account = false
  regional               = false
  logging_service        = "none"
  region                 = var.region
  zones                  = var.zones
  network                = module.vpc.network_name
  subnetwork             = module.vpc.subnets_names[0]
  ip_range_pods          = "${local.env_name}-${local.env_project_name}-pods"
  ip_range_services      = "${local.env_name}-${local.env_project_name}-services"
  node_pools = [
    {
      name                      = "peer-nodes"
      machine_type              = "g1-small"
      autoscaling               = false
      node_count                = var.peer_node_count
      disk_type                 = "pd-ssd"
      disk_size_gb              = 15
    },
    {
      name                      = "client-nodes"
      machine_type              = "e2-medium"
      autoscaling               = false
      node_count                = var.client_node_count
      disk_type                 = "pd-ssd"
      disk_size_gb              = 15
    },

  ]

  node_pools_labels = {

    peer-nodes = {
      lsfs = "peer"
    }

    client-nodes = {
      lsfs = "client"
    }
  }

  node_pools_tags = {
    all = [
      "ssh",
    ]
  }
  node_pools_metadata = {
        all = {
          ssh-keys = "${var.nodes_user}:${file(var.ssh_path)}"
        }
  }
}






