# Ansible configuration

This directory contains configuration management for the benchmark VM. The
first playbook only verifies SSH access, Python availability, and fact
collection. It does not modify the remote machine.

## Components

- `ansible.cfg` contains project-level Ansible settings.
- `inventory.example.ini` is a safe example of the managed-host inventory.
- `inventory.ini` is the local inventory with the current VM public IP. It is
  ignored by Git.
- `group_vars/benchmark.yml` constructs the versioned release image from the
  `BENCHMARK_RELEASE` Git commit SHA and defines the remote deployment directory.
- `playbooks/check.yml` verifies the connection to the VM.
- `playbooks/bootstrap.yml` installs Docker Engine, Buildx, and Docker Compose
  and makes sure that the Docker service starts automatically.
- `playbooks/deploy.yml` checks out the pinned application revision, prepares a
  smoke-test dataset, pulls the already-built image, and starts Compose without
  compiling on the VM.
- `playbooks/smoke.yml` loads the sample dataset, validates Query 1, and checks
  the benchmark metrics, Prometheus target, and Grafana health.
- `playbooks/site.yml` runs bootstrap, deployment, and smoke testing in order.
- `requirements.yml` pins additional Ansible collections used by playbooks.

## Install Ansible on Fedora

Ansible runs on the local control machine, not in the benchmark VM:

```bash
sudo dnf install ansible-core
ansible --version
```

## Prepare the inventory

Start the VM and obtain its current public IP. Dynamic public IP addresses can
change after a stop and start. Then create the local inventory:

```bash
cp inventory.example.ini inventory.ini
```

Replace the example `ansible_host` value in `inventory.ini` with the current
VM public IP address.

## Verify access

From this directory, run the ad-hoc ping module:

```bash
ansible benchmark -m ansible.builtin.ping
```

Unlike the network `ping` utility, the Ansible ping module connects over SSH
and executes a small Python operation on the managed host. A successful result
contains `"ping": "pong"`.

Then run the read-only check playbook:

```bash
ansible-playbook playbooks/check.yml
```

## Prepare the VM

Review the changes supported by Ansible check mode:

```bash
ansible-playbook --check --diff playbooks/bootstrap.yml
```

Apply the configuration:

```bash
ansible-playbook playbooks/bootstrap.yml
```

The playbook uses privilege escalation (`become: true`) because package and
service management requires root permissions. The SSH user is not added to the
`docker` group, so manual Docker commands must be run with `sudo`.

## Deploy a published release

Install the pinned Ansible collection on the control machine:

```bash
ansible-galaxy collection install --requirements-file requirements.yml
```

Every image published by CI has the full Git commit SHA as its tag. Select the
commit that has a successful GitHub Actions run:

```bash
export BENCHMARK_RELEASE="$(git -C ../.. rev-parse HEAD)"
printf '%s\n' "$BENCHMARK_RELEASE"
```

The value must contain 40 hexadecimal characters. The commit must already be
pushed to `main`; otherwise its image does not exist in GHCR.

Check the deployment playbook syntax:

```bash
ansible-playbook --syntax-check playbooks/deploy.yml
```

Deploy that exact release:

```bash
ansible-playbook playbooks/deploy.yml
```

The repository is cloned over HTTPS only to obtain the Compose and monitoring
configuration. Ansible writes `BENCHMARK_IMAGE` to the remote `.env` file and
runs Compose with `build: never` and `pull: always`. Therefore the VM downloads
the image `ghcr.io/dmgoldberg1/columnar-engine-from-scratch:<commit>` and never
recompiles the C++ application.

The GHCR package must be public for this credential-free deployment. The smoke
dataset is copied only when `datasets/hits.csv` is absent, preserving a dataset
placed on the VM later. Compose attaches that host directory to `/data` through
a bind mount, so the dataset is not stored inside the image.

## Run the smoke test

Run the smoke test after a successful deployment:

```bash
ansible-playbook playbooks/smoke.yml
```

The smoke test is separate from deployment because it executes application
operations rather than configuring the operating system. It expects Query 1 to
return `1000`, the number of rows in the committed sample dataset.

For a new VM, run all three playbooks with one command:

```bash
ansible-playbook playbooks/site.yml
```

`site.yml` first prepares the machine, then deploys `BENCHMARK_RELEASE`, and
only after a successful health check starts the smoke test.
