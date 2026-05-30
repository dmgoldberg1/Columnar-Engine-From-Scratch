#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 4 ]; then
  echo "Usage: $0 QUERY_NUM COLUMNAR OUTPUT LOGS" >&2
  exit 1
fi

QUERY_NUM="$1"
COLUMNAR="$2"
OUTPUT="$3"
LOGS="$4"
REPO_ROOT="$(pwd)"
BUILD_DIR="$REPO_ROOT/build"
BENCHMARK_BIN="$BUILD_DIR/benchmark"
TMP_LOG="$(mktemp)"
trap 'rm -f "$TMP_LOG"' EXIT

if [ ! -x "$BENCHMARK_BIN" ]; then
  echo "Benchmark binary not found: $BENCHMARK_BIN" >&2
  exit 1
fi

if [ ! -f "$COLUMNAR" ]; then
  echo "Columnar file not found: $COLUMNAR" >&2
  exit 1
fi

mkdir -p "$(dirname "$OUTPUT")" "$(dirname "$LOGS")"
COLUMNAR_REAL="$(readlink -f "$COLUMNAR")"
BUILD_TEST_REAL="$(readlink -f "$BUILD_DIR/db_file_benchmark_test.egg" 2>/dev/null || true)"
BUILD_MAIN_REAL="$(readlink -f "$BUILD_DIR/db_file_benchmark.egg" 2>/dev/null || true)"
if [ "$COLUMNAR_REAL" != "$BUILD_TEST_REAL" ]; then
  ln -sfn "$COLUMNAR_REAL" "$BUILD_DIR/db_file_benchmark_test.egg"
fi
if [ "$COLUMNAR_REAL" != "$BUILD_MAIN_REAL" ]; then
  ln -sfn "$COLUMNAR_REAL" "$BUILD_DIR/db_file_benchmark.egg"
fi

TEST_NAME="$({
python3 - "$REPO_ROOT/benchmark_test.cpp" "$QUERY_NUM" <<'PY2'
import re
import sys

benchmark_path = sys.argv[1]
query_index = int(sys.argv[2]) + 1
with open(benchmark_path, encoding='utf-8') as f:
    tests = []
    for line in f:
        m = re.match(r'^TEST\(ClickBenchQueriesTest,\s*(Query[0-9][^)]*)\)', line)
        if m:
            tests.append(m.group(1))

for name in tests:
    if re.fullmatch(rf'Query0*{query_index}', name):
        print(name)
        raise SystemExit(0)

for name in tests:
    if re.match(rf'^Query0*{query_index}(?![0-9])', name):
        print(name)
        raise SystemExit(0)

if 0 <= query_index - 1 < len(tests):
    print(tests[query_index - 1])
PY2
} )"

if [ -z "$TEST_NAME" ]; then
  echo "Unknown query number: $QUERY_NUM" >&2
  exit 1
fi

set +e
(
  cd "$BUILD_DIR"
  ./benchmark --gtest_filter="ClickBenchQueriesTest.${TEST_NAME}"
) >"$TMP_LOG" 2>&1
STATUS="$?"
set -e

cp "$TMP_LOG" "$LOGS"
if [ "$STATUS" -ne 0 ]; then
  exit "$STATUS"
fi

awk '
  /^Running main\(\) from / { next }
  /^Note: Google Test filter = / { next }
  /^\[==========\]/ { next }
  /^\[----------\]/ { next }
  /^\[ RUN      \]/ { next }
  /^\[       OK \]/ { next }
  /^\[  PASSED  \]/ { next }
  /^$/ { next }
  { print }
' "$TMP_LOG" > "$OUTPUT"
