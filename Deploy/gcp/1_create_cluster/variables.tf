
variable "project_id" {
  type        = string 
  description = "The project id in which the cluster is going to be build"
}

variable "region" {
  type        = string 
  description = "The region to host the cluster"
}

variable "zones" {
  type        = list(string)
  description = "The zone(s) to host the cluster"
}

variable "peer_node_count" {
    type = number
    description = "Number of peer worker nodes to be created" 
}

variable "client_node_count" {
    type = number
    description = "Number of client nodes to be created" 
}

variable "nodes_user" {
  type = string
  description = "User used is nodes"  
}

variable "ssh_path" {
  type = string
  description = "Path to ssh keys to be added to nodes"
}