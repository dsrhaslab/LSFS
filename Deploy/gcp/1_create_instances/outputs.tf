output "client_instances_ip" {
  description = "Client instance name and ip"
  value       = module.client_instances.instances_info
}

output "peer_instances_ip" {
  description = "Peers instance name and ip"
  value       = module.peer_instances.instances_info
}

output "master_instances_ip" {
  description = "Master instance name and ip"
  value       = module.master.instances_info
}