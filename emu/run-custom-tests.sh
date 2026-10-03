#!/usr/bin/env bash
# run-custom-tests.sh <emu binary> <firmware dir>  (make test-custom calls it)
# Each script runs headless in real time, capped at 3 minutes; prints a
# summary and exits 1 on any FAIL, crash or hang. Logs: build/logs/custom-*.
set -u
cd "$(dirname "$0")"
EMU=$1; FW_DIR=$2
mkdir -p build/logs
card=(); [ -d "$FW_DIR/emu-card" ] && card=(--card "$FW_DIR/emu-card")
fail=0
for t in tests/generic/*.emu "$FW_DIR"/emu-tests/*.emu; do
  [ -f "$t" ] || continue
  log="build/logs/custom-$(basename "$t" .emu).txt"
  extra=(); argline=$(grep -m1 '^#!args ' "$t" | sed 's/^#!args //')
  [ -n "$argline" ] && read -r -a extra <<< "$argline"
  ( "$EMU" --headless --flash none ${card[@]+"${card[@]}"} ${extra[@]+"${extra[@]}"} --script "$t" > "$log" 2>&1 ) &
  pid=$! waited=0
  while kill -0 $pid 2>/dev/null; do
    sleep 1; waited=$((waited + 1))
    if [ $waited -ge 180 ]; then kill -9 $pid 2>/dev/null; echo "HANG (killed after 180 s)" >> "$log"; break; fi
  done
  wait $pid; st=$?
  [ $st -gt 128 ] && echo "CRASH (signal $((st - 128)))" >> "$log"
  echo "exit $st" >> "$log"
  p=$(grep -c '^PASS' "$log"); x=$(grep -c '^FAIL' "$log")
  if [ "$x" -eq 0 ] && [ "$st" -eq 0 ]; then m=ok; else m=FAIL; fail=1; fi
  printf '%-4s %-24s %3d pass %2d fail  exit %s   (%s)\n' "$m" "$(basename "$t" .emu)" "$p" "$x" "$st" "$log"
  grep -E '^FAIL|CRASH|HANG|^\?\?' "$log" | sed 's/^/       /'
done
exit $fail
