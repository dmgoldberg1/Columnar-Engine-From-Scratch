#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

IMAGE="${BENCHMARK_IMAGE:-columnar-benchmark-service:dev}"
CONTAINER_NAME="${BENCHMARK_CONTAINER_NAME:-columnar-benchmark}"
PUBLISH_ADDRESS="${BENCHMARK_PUBLISH_ADDRESS:-127.0.0.1}"
HOST_PORT="${BENCHMARK_HOST_PORT:-8080}"
DATASET_DIR="${BENCHMARK_DATASET_DIR:-$REPO_ROOT/datasets}"
CPU_LIMIT="${BENCHMARK_CPU_LIMIT:-}"
MEMORY_LIMIT="${BENCHMARK_MEMORY_LIMIT:-}"

if [ ! -d "$DATASET_DIR" ]; then
  echo "Dataset directory not found: $DATASET_DIR" >&2
  echo "Create it and put the source dataset there before starting the container." >&2
  exit 1
fi

DATASET_DIR="$(readlink -f "$DATASET_DIR")"

DOCKER_RUN_ARGS=(
  --rm
  --name "$CONTAINER_NAME"
  --user "$(id -u):$(id -g)"
  --publish "$PUBLISH_ADDRESS:$HOST_PORT:8080"
  --mount "type=bind,source=$DATASET_DIR,target=/data"
)

if [ -n "$CPU_LIMIT" ]; then
  DOCKER_RUN_ARGS+=(--cpus "$CPU_LIMIT")
fi

if [ -n "$MEMORY_LIMIT" ]; then
  DOCKER_RUN_ARGS+=(--memory "$MEMORY_LIMIT")
fi

exec docker run "${DOCKER_RUN_ARGS[@]}" "$IMAGE"
