#!/usr/bin/env bash
set -euo pipefail

# Config
BIN_MT="./build/modern/ccretro"
BIN_ST="./build/modern/ccretro-1t"
OUTDIR="${OUTDIR:-bench-out}"
SKIP_EXTREME="${SKIP_EXTREME:-0}"   # set to 1 to skip the 2^5e6 test

# Step counts per spec: 1k for all standard tests, 1 step for 5e6
STEPS_64_256=1000
STEPS_8192=1000
STEPS_5E6=1

mkdir -p "$OUTDIR"

# Ensure binaries exist
if [[ ! -x "$BIN_MT" || ! -x "$BIN_ST" ]]; then
  echo "Error: binaries not found in ./build/modern/. Build first with: make arch=modern"
  exit 1
fi

# Format steps as "1k steps", "10k steps", or "<n> steps"
fmt_steps() {
  local n="$1"
  case "$n" in
    1000)  echo "1k steps" ;;
    10000) echo "10k steps" ;;
    *)     echo "${n} steps" ;;
  esac
}

# Run one test, parse "Total runtime: X seconds" from program output
run_and_print() {
  local label="$1"
  local bin="$2"
  local args="$3"
  local outfile="$4"

  printf "Running test: %s: " "$label"
  # Run and save full output
  if ! "$bin" $args > "$outfile" 2>&1; then
    echo "FAILED"
    return
  fi

  # Extract the numeric runtime
  local t
  t="$(grep -E 'Total runtime:' "$outfile" | tail -n 1 | sed -E 's/.*Total runtime:[[:space:]]*([0-9.]+)[[:space:]]+seconds.*/\1/')"
  if [[ -z "$t" ]]; then
    echo "n/a"
  else
    echo "${t}s"
  fi
}

echo "Running Collatz Conjecture benchmark:"

run_and_print "2^64 to 2^256, $(fmt_steps "$STEPS_64_256") (MT)" \
  "$BIN_MT" "-start 64 -end 256 -stepsize $STEPS_64_256" \
  "$OUTDIR/out-64-256-mt.txt"

run_and_print "2^64 to 2^256, $(fmt_steps "$STEPS_64_256") (ST)" \
  "$BIN_ST" "-start 64 -end 256 -stepsize $STEPS_64_256" \
  "$OUTDIR/out-64-256-st.txt"

run_and_print "2^8192 to 2^8193, $(fmt_steps "$STEPS_8192") (MT)" \
  "$BIN_MT" "-start 8192 -end 8192 -stepsize $STEPS_8192" \
  "$OUTDIR/out-8192-mt.txt"

run_and_print "2^8192 to 2^8193, $(fmt_steps "$STEPS_8192") (ST)" \
  "$BIN_ST" "-start 8192 -end 8192 -stepsize $STEPS_8192" \
  "$OUTDIR/out-8192-st.txt"

if [[ "$SKIP_EXTREME" != "1" ]]; then
  run_and_print "2^5e6 to 2^5e6+1, $(fmt_steps "$STEPS_5E6") (ST)" \
    "$BIN_ST" "-start 5000000 -end 5000000 -stepsize $STEPS_5E6" \
    "$OUTDIR/out-5e6-st.txt"
fi
