
resource "google_compute_instance" "instance" {
  count = var.instance_count
  
  project = var.project_id

  name         = "${var.instance.name}${sum([count.index,1])}"
  machine_type = var.instance.type
  zone         = var.zone

  tags = var.instance.tags

  boot_disk {
    initialize_params {
      image =  var.instance.boot_disk.image
      size = var.instance.boot_disk.size
      type = var.instance.boot_disk.type
    }
  }

  network_interface {
    network = var.network
    subnetwork = var.subnetwork
    access_config {
      // Ephemeral public IP
    }
  }

  metadata = {
    ssh-keys = var.ssh_key_metadata
    lsfs = var.label 
  }

  guest_accelerator = [
    {
      type = var.instance.gpu.type 
      count = var.instance.gpu.count
    }
  ]
  # For GPU
  scheduling { 
    on_host_maintenance = "${var.instance.gpu.count != 0 ? "TERMINATE" : "MIGRATE"}"
  }

  metadata_startup_script = var.startup_script
}