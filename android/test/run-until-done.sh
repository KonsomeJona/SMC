#!/bin/bash
# run-until-done.sh — retry a level until the engine reports it finished.
A=~/Library/Android/sdk/platform-tools/adb
DEV="$1"; JUMPS="$2"; EXITX="$3"; TRIES="${4:-10}"; PER="${5:-260}"
for i in $(seq 1 "$TRIES"); do
    echo "########## essai $i/$TRIES"
    bash /tmp/enter-level.sh "$DEV" >/dev/null 2>&1
    bash /tmp/play-planned.sh "$DEV" "$JUMPS" "$EXITX" "$PER" "run$i"
    [ $? -eq 0 ] && { echo "SUCCES a l essai $i"; exit 0; }
done
exit 1
