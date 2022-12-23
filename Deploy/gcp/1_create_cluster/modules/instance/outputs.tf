output "instances_info" {
  description = "Map of instances (name, ip)"
  value       =  "${zipmap(google_compute_instance.instance.*.name, google_compute_instance.instance.*.network_interface.0.access_config.0.nat_ip)}"
}

#Creates map something like this: 
# {
#    peer-0 = 0.0.0.0
#    peer-1 = 0.0.0.0
# }