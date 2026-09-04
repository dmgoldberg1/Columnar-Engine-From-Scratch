resource "yandex_vpc_network" "benchmark" {
  name        = "${var.project_name}-network"
  description = "Network for the columnar benchmark environment."
}

resource "yandex_vpc_subnet" "benchmark" {
  name           = "${var.project_name}-subnet"
  description    = "Subnet for the columnar benchmark environment."
  zone           = var.zone
  network_id     = yandex_vpc_network.benchmark.id
  v4_cidr_blocks = var.subnet_cidr_blocks
}
