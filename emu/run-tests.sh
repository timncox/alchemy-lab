#!/usr/bin/env bash
# Run every firmware's tests/<fw>/*.emu headless, all firmwares in parallel
# (each script runs in real time), then print a summary. Exit 1 on any FAIL,
# crash or hang.
#
#   ./run-tests.sh                 all firmwares
#   ./run-tests.sh mark plaits     just these
set -u
cd "$(dirname "$0")"
FWS=("$@")
[ ${#FWS[@]} -eq 0 ] && FWS=(belt mark smack clouds elements marbles meld plaits warps)
mkdir -p build/logs

run_fw() {
  local f=$1 card=() rc=0
  [ -d "build/$f/card" ] && card=(--card "build/$f/card")
  for t in tests/$f/*.emu; do
    [ -f "$t" ] || continue
    local log="build/logs/$f-$(basename "$t" .emu).txt"
    # a hung firmware would stall forever: cap each script at 3 minutes
    ( "build/$f/emu" --headless --flash none ${card[@]+"${card[@]}"} --script "$t" > "$log" 2>&1 ) &
    local pid=$! waited=0
    while kill -0 $pid 2>/dev/null; do
      sleep 1; waited=$((waited + 1))
      if [ $waited -ge 180 ]; then kill -9 $pid 2>/dev/null; echo "HANG (killed after 180 s)" >> "$log"; break; fi
    done
    wait $pid; local st=$?
    [ $st -gt 128 ] && echo "CRASH (signal $((st - 128)))" >> "$log"
    echo "exit $st" >> "$log"
  done
}

for f in "${FWS[@]}"; do
  [ -x "build/$f/emu" ] || { echo "build/$f/emu missing (make FW=$f)"; exit 2; }
  run_fw "$f" &
done
wait

fail=0
for f in "${FWS[@]}"; do
  for t in tests/$f/*.emu; do
    [ -f "$t" ] || continue
    log="build/logs/$f-$(basename "$t" .emu).txt"
    p=$(grep -c '^PASS' "$log"); x=$(grep -c '^FAIL' "$log")
    st=$(tail -1 "$log")
    if [ "$x" -eq 0 ] && [ "$st" = "exit 0" ]; then mark=ok; else mark=FAIL; fail=1; fi
    printf '%-4s %-9s %-16s %3d pass %2d fail  %s\n' "$mark" "$f" "$(basename "$t" .emu)" "$p" "$x" "$st"
    grep -E '^FAIL|CRASH|HANG|^\?\?' "$log" | sed 's/^/       /'
  done
done
exit $fail
