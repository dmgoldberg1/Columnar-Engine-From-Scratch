variable "project_name" {
  description = "Name prefix for Yandex Cloud resources."
  type        = string
  default     = "columnar-benchmark"
}

variable "zone" {
  description = "Default Yandex Cloud availability zone for the benchmark infrastructure."
  type        = string
  default     = "ru-central1-d"
}

variable "subnet_cidr_blocks" {
  description = "Private IPv4 ranges allocated to the benchmark subnet."
  type        = list(string)
  default     = ["10.10.0.0/24"]
}

variable "ssh_user" {
  description = "Default Linux user of the VM image used for SSH access."
  type        = string
  default     = "ubuntu"
}

variable "ssh_public_key_path" {
  description = "Path to the public SSH key added to the VM."
  type        = string
  default     = "~/.ssh/id_ed25519.pub"
}

variable "ssh_allowed_cidr_blocks" {
  description = "IPv4 ranges allowed to connect to the VM over SSH."
  type        = list(string)
}

variable "vm_image_family" {
  description = "Yandex Cloud image family used for the VM boot disk."
  type        = string
  default     = "ubuntu-2404-lts"
}
