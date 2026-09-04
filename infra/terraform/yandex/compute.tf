data "yandex_compute_image" "ubuntu" {
  family    = var.vm_image_family
  folder_id = "standard-images"
}

resource "yandex_compute_instance" "benchmark" {
  name        = "${var.project_name}-vm"
  description = "VM for the columnar benchmark environment."
  zone        = var.zone
  platform_id = "standard-v3"

  resources {
    cores  = 2
    memory = 2
  }

  boot_disk {
    initialize_params {
      image_id = data.yandex_compute_image.ubuntu.id
      size     = 20
      type     = "network-hdd"
    }
  }

  network_interface {
    subnet_id          = yandex_vpc_subnet.benchmark.id
    nat                = true
    security_group_ids = [yandex_vpc_security_group.benchmark.id]
  }

  metadata = {
    ssh-keys = "${var.ssh_user}:${trimspace(file(pathexpand(var.ssh_public_key_path)))}"
  }
}
