output "network_id" {
  description = "ID of the benchmark VPC network."
  value       = yandex_vpc_network.benchmark.id
}

output "subnet_id" {
  description = "ID of the benchmark VPC subnet."
  value       = yandex_vpc_subnet.benchmark.id
}

output "vm_internal_ip" {
  description = "Private IP address of the benchmark VM."
  value       = yandex_compute_instance.benchmark.network_interface[0].ip_address
}

output "vm_public_ip" {
  description = "Public IP address of the benchmark VM."
  value       = yandex_compute_instance.benchmark.network_interface[0].nat_ip_address
}

output "ssh_command" {
  description = "Command for connecting to the benchmark VM."
  value       = "ssh ${var.ssh_user}@${yandex_compute_instance.benchmark.network_interface[0].nat_ip_address}"
}
