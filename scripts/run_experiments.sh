#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

REPEATS="${REPEATS:-3}"
POLICIES="${POLICIES:-0 1 2 3}"
PARALLELISM="${PARALLELISM:-1 2 4 8}"
STATS_FILE="${STATS_FILE:-tmp/stats.csv}"
CLEAR_STATS="${CLEAR_STATS:-1}"
LOCK_DIR="tmp/.experiments.lock"

if ! mkdir "$LOCK_DIR" 2>/dev/null; then
  echo "[tests] another experiment run is active. stop it before starting a new one."
  exit 1
fi

ctrl_pid=""
cleanup() {
  if [[ -n "$ctrl_pid" ]]; then
    kill "$ctrl_pid" >/dev/null 2>&1 || true
  fi
  rm -rf "$LOCK_DIR"
}
trap cleanup EXIT INT TERM

wait_for_controller_ready() {
  local attempts=0
  while (( attempts < 40 )); do
    if [[ -p tmp/controller_pipe ]] && kill -0 "$ctrl_pid" >/dev/null 2>&1; then
      return 0
    fi
    sleep 0.05
    attempts=$((attempts + 1))
  done
  return 1
}

submit_workload() {
  local runner_pids=()
  local failures=0

  for _ in $(seq 1 8); do
    ./bin/runner -e 1 sleep 1 >/dev/null 2>&1 &
    runner_pids+=("$!")
  done

  for _ in $(seq 1 6); do
    ./bin/runner -e 2 sleep 2 >/dev/null 2>&1 &
    runner_pids+=("$!")
  done

  for _ in $(seq 1 4); do
    ./bin/runner -e 3 sleep 4 >/dev/null 2>&1 &
    runner_pids+=("$!")
  done

  for pid in "${runner_pids[@]}"; do
    if ! kill -0 "$ctrl_pid" >/dev/null 2>&1; then
      echo "[tests] controller exited unexpectedly; terminating pending runners"
      for rp in "${runner_pids[@]}"; do
        kill "$rp" >/dev/null 2>&1 || true
      done
      for rp in "${runner_pids[@]}"; do
        wait "$rp" >/dev/null 2>&1 || true
      done
      return 0
    fi

    wait "$pid" || failures=$((failures + 1))
  done

  if (( failures > 0 )); then
    echo "[tests] warning: $failures runner(s) returned non-zero status"
  fi
}

echo "[tests] Building project"
make

if [[ "$CLEAR_STATS" == "1" ]]; then
  rm -f "$STATS_FILE"
fi

echo "[tests] Writing stats to $STATS_FILE"

for policy in $POLICIES; do
  for parallelism in $PARALLELISM; do
    for run in $(seq 1 "$REPEATS"); do
      echo "[tests] policy=$policy parallelism=$parallelism run=$run/$REPEATS"

      STATS_CSV="$STATS_FILE" PARALLELISM="$parallelism" ./bin/controller "$parallelism" "$policy" >/dev/null 2>&1 &
      ctrl_pid="$!"

      if ! wait_for_controller_ready; then
        echo "[tests] controller failed to initialize for policy=$policy parallelism=$parallelism"
        kill "$ctrl_pid" >/dev/null 2>&1 || true
        ctrl_pid=""
        continue
      fi

      submit_workload

      if kill -0 "$ctrl_pid" >/dev/null 2>&1; then
        ./bin/runner -s >/dev/null 2>&1 || true
        wait "$ctrl_pid" || true
      else
        echo "[tests] controller already stopped before shutdown request"
      fi
      ctrl_pid=""
    done
  done
done

echo "[tests] Completed. Results available at $STATS_FILE"
