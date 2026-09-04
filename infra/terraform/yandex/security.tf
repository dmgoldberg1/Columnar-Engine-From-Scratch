resource "yandex_vpc_security_group" "benchmark" {
  name        = "${var.project_name}-sg"
  description = "Network access rules for the benchmark VM."
  network_id  = yandex_vpc_network.benchmark.id

  ingress {
    description    = "SSH from explicitly allowed client addresses."
    protocol       = "TCP"
    v4_cidr_blocks = var.ssh_allowed_cidr_blocks
    port           = 22
  }

  egress {
    description    = "Allow the VM to reach package repositories and the internet."
    protocol       = "ANY"
    v4_cidr_blocks = ["0.0.0.0/0"]
  }
}
