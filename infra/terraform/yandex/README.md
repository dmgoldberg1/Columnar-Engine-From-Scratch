# Yandex Cloud infrastructure

This directory contains the Terraform configuration for the benchmark
environment in Yandex Cloud.

The configuration creates a minimal benchmark environment:

- a dedicated VPC network and subnet;
- a security group that allows inbound SSH from selected addresses;
- one Ubuntu VM with a boot disk and a dynamic public IP address.

Terraform creates the infrastructure, but it does not install Docker or deploy
the application. Software configuration remains a separate step.

## Authentication

The provider reads credentials and scope from environment variables:

```bash
export YC_TOKEN="$(yc iam create-token)"
export YC_CLOUD_ID="$(yc config get cloud-id)"
export YC_FOLDER_ID="$(yc config get folder-id)"
```

Do not put these values into `.tf` files or commit them to Git. For automated
deployments, use a dedicated service account with the minimum required roles
instead of a personal user token.

## Local values

Copy the example variables file when a local override is needed:

```bash
cp terraform.tfvars.example terraform.tfvars
```

`terraform.tfvars` is ignored by Git. The example file is safe to commit
because it contains no credentials.

Before planning, replace the example value of `ssh_allowed_cidr_blocks` with
the public IPv4 address of the computer from which you connect. You can view it
on that computer with:

```bash
curl -4 https://api.ipify.org
```

For example, if the command prints `198.51.100.25`, use:

```hcl
ssh_allowed_cidr_blocks = ["198.51.100.25/32"]
```

The `/32` suffix permits exactly one IPv4 address. Do not put a private address
such as `192.168.x.x` here: Yandex Cloud sees the public address used by your
internet connection.

The official Ubuntu image uses `ubuntu` as its default SSH user. With the
`ssh-keys` metadata mechanism, the public key is assigned to that image user;
the `ssh_user` variable must therefore remain consistent with the selected
image family.

## Validation workflow

```bash
terraform init
terraform fmt -check
terraform validate
terraform plan
```

`terraform plan` reads the cloud state and shows the changes without creating
anything. Always review its output before applying it:

```bash
terraform apply
```

After creation, Terraform prints the VM public IP and a ready-to-use SSH
command. You can print them again with:

```bash
terraform output
```

The public IP is dynamic: it can change after the VM is stopped and started.

To delete the resources managed by this configuration:

```bash
terraform destroy
```

Review the destroy plan before confirming it. The VM and its boot disk are
removed, so application data stored only on that disk is lost.
