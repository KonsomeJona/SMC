#!/bin/bash
# watch-run.sh — lets the in-engine autopilot run, dismissing the "Hint!"
# dialogs that pause the game (they are what stopped every earlier session),
# and reports when the engine says the level is finished.
A=~/Library/Android/sdk/platform-tools/adb
DEV="${1:-emulator-5554}"; BUDGET="${2:-600}"
GOTIT_X=1077; GOTIT_Y=699

START=$(date +%s); BEST=0
while :; do
    E=$(( $(date +%s) - START ))
    N=$($A -s "$DEV" logcat -d -t 4000 2>/dev/null | grep -c "SMCTEST LEVEL_FINISHED")
    [ "$N" -gt 0 ] && { echo "===== LEVEL_FINISHED (x$N) apres ${E}s ====="; exit 0; }
    [ "$E" -ge "$BUDGET" ] && { echo "timeout ${E}s best=$BEST"; exit 3; }

    # Close any hint box; harmless when none is up (centre of screen, no pad zone).
    $A -s "$DEV" shell input tap $GOTIT_X $GOTIT_Y >/dev/null 2>&1
    sleep 4
    X=$($A -s "$DEV" logcat -d -t 300 2>/dev/null | grep -oE "PLAYERPOS x=-?[0-9.]+" | cut -d= -f2 | tail -1)
    [ -n "$X" ] && python3 -c "import sys;sys.exit(0 if float('$X')>float('$BEST') else 1)" 2>/dev/null && BEST=$X
    echo "  t=${E}s x=$X best=$BEST"
done
