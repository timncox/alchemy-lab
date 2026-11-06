#!/usr/bin/env bash
# Phone mode end to end, no window: boot an emulator headless in Phone USB
# mode with --hostlink, ask it HELLO / PHONE_INFO / STATE with
# hostlink_client.py, exit 0 only if every answer is OK.
#
#   tools/hostlink-test.sh [emu binary] [port] [usb mode]
#   tools/hostlink-test.sh build/belt-phone/emu 5079 phone+audio
set -u
cd "$(dirname "$0")/.."
EMU=${1:-build/belt-phone/emu}
PORT=${2:-5079}
USB=${3:-phone}
[ -x "$EMU" ] || { echo "$EMU missing (make FW=belt-phone FW_ROOT=...)"; exit 2; }
mkdir -p build/logs
serve=build/logs/hostlink-serve.emu
echo "wait 20000" > "$serve"     # waits keep the socket served
log=build/logs/hostlink-$PORT.txt
"$EMU" --headless --flash none --usb "$USB" --hostlink "$PORT" --script "$serve" > "$log" 2>&1 &
pid=$!
python3 tools/hostlink_client.py --port "$PORT" --check
rc=$?
kill $pid 2>/dev/null; wait $pid 2>/dev/null
[ $rc -eq 0 ] && echo "ok   hostlink ($USB, port $PORT)" || { echo "FAIL hostlink ($USB, port $PORT); emulator log: $log"; cat "$log"; }
exit $rc
