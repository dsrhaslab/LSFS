
variable "project_id" {
  type        = string 
  description = "The project id used"
}

variable "region" {
  type        = string 
  description = "The region to host the cluster"
}

variable "zone" {
  type        = string
  description = "The zone to host the cluster"
}

variable "network" {
    type        = string
    description = "Network name" 
}

variable "subnetwork" {
    type        = string
    description = "Subnetwork name" 
}

variable "ssh_key_metadata" {
  type        = string
  description = "The ssh key metadata (user:ssh pub key)"
}

variable "startup_script" {
  type        = string
  description = "The startup script"
  default = ""
}

variable "instance_count" {
  type        = number
  description = "Instance count"
  default = 1
}

variable "label" {
  type        = string
  description = "Instance label"
}

variable "instance" {
  type = object({
    name = string
    type = string 
    tags = list(string)
    boot_disk = object({
      image   = string
      size    = number
      type    = string
    })
    gpu = optional(object({
      type = optional(string)
      count = optional(number)
    }), {type = "", count = 0})
  })
  description = "Instance object configuration" 
}