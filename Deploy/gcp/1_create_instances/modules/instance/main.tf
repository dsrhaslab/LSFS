
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

  dynamic "attached_disk" {
    for_each = var.attached_disk_name != null ? [""] : []
    content {
      source = var.attached_disk_name
    }
  }

  network_interface {
    network = var.network
    subnetwork = var.subnetwork
    dynamic "access_config" {
      for_each = var.public_ip ? [""] : []
      content {
      }
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

resource "null_resource" "instanceprov" {
  count = var.provisioner? 1 : 0
  
  connection {
      type        = "ssh"
      user        = var.instance_user
      private_key = "${file("~/.ssh/id_rsa")}"
      host        = "${google_compute_instance.instance.0.network_interface.0.access_config.0.nat_ip}"
  }

  provisioner "file" {
    source      = var.provisioner_file.origin
    destination = var.provisioner_file.destination
  }

  provisioner "file" {
    source      = var.provisioner_file2.origin
    destination = var.provisioner_file2.destination
  }

  provisioner "file" {
    content      = var.provisioner_file3.content
    destination = var.provisioner_file3.destination
  }

}