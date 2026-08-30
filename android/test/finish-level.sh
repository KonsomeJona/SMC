#!/bin/bash
# finish-level.sh — keeps attempting a level until the engine reports it done.
#
# Each death costs a life; when the run leaves the level, walk the menus back
# in and try again. Stops on LEVEL_FINISHED or after MAX attempts.
#
# Usage: finish-level.sh [device] [max_attempts] [per_attempt_seconds]

A=~/Library/Android/sdk/platform-tools/adb
DEV="${1:-emulator-5554}"
MAX="${2:-8}"
PER="${3:-180}"

back_into_level() {
    # From wherever we are: force a clean path through the menus.
    $A -s "$DEV" shell am force-stop me.takohi.bandagoo
    $A -s "$DEV" shell am start -n me.takohi.bandagoo/org.smc.SMCActivity >/dev/null 2>&1
    sleep 26
    $A -s "$DEV" shell input tap 1073 270; sleep 5
    $A -s "$DEV" shell input tap 319 309;  sleep 2
    $A -s "$DEV" shell input tap 961 867;  sleep 8
    $A -s "$DEV" shell input tap 1896 913; sleep 11
}

for i in $(seq 1 "$MAX"); do
    echo "===== tentative $i/$MAX ====="
    back_into_level
    bash /tmp/play-level.sh "$DEV" "$PER" "lvl1-$i"
    RC=$?
    if [ "$RC" -eq 0 ]; then
        echo "NIVEAU 1 TERMINE a la tentative $i"
        exit 0
    fi
done
echo "echec apres $MAX tentatives"
exit 1
