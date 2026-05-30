#!/usr/bin/env bash
set -euo pipefail

export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y --no-install-recommends   build-essential   cmake   ninja-build   python3   curl   unzip   pkg-config
rm -rf /var/lib/apt/lists/*
