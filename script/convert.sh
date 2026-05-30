#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 2 ]; then
  echo "Usage: $0 INPUT_CSV COLUMNAR" >&2
  exit 1
fi

INPUT_CSV="$1"
COLUMNAR="$2"
REPO_ROOT="$(pwd)"
BUILD_DIR="$REPO_ROOT/build"
BENCHMARK_BIN="$BUILD_DIR/benchmark"

if [ ! -f "$INPUT_CSV" ]; then
  echo "Input CSV not found: $INPUT_CSV" >&2
  exit 1
fi

if [ ! -x "$BENCHMARK_BIN" ]; then
  echo "Benchmark binary not found: $BENCHMARK_BIN" >&2
  exit 1
fi

mkdir -p "$(dirname "$COLUMNAR")"
INPUT_CSV_REAL="$(readlink -f "$INPUT_CSV")"
REPO_INPUT_REAL="$(readlink -f "$REPO_ROOT/hits_with_header.csv" 2>/dev/null || true)"
if [ "$INPUT_CSV_REAL" != "$REPO_INPUT_REAL" ]; then
  ln -sfn "$INPUT_CSV_REAL" "$REPO_ROOT/hits_with_header.csv"
fi
(
  cd "$BUILD_DIR"
  ./benchmark --gtest_filter=ClickBenchQueriesTest.BuildHitsColumnarFile
)
cp "$BUILD_DIR/db_file_benchmark_test.egg" "$COLUMNAR"
ln -sfn "$(readlink -f "$COLUMNAR")" "$BUILD_DIR/db_file_benchmark.egg"
ln -sfn "$(readlink -f "$COLUMNAR")" "$BUILD_DIR/db_file_benchmark_test.egg"
