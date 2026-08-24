#!/bin/bash
# watch-run.sh — supervises an autopilot run.
#
# Two things stop a run that the autopilot itself cannot handle: the game
# pauses on "Hint!" dialogs until Got it is pressed, and a game over drops
# back to the main menu. This closes the dialogs and walks back into the
# level, then reports the engine's own LEVEL_FINISHED.
#
# Usage: watch-run.sh [device] [budget_seconds]
A=~/Library/Android/sdk/platform-tools/adb
DEV="${1:-emulator-5554}"; BUDGET="${2:-900}"
GOTIT_X=1077; GOTIT_Y=699

START=$(date +%s); BEST=0; RESTARTS=0
while :; do
    E=$(( $(date +%s) - START ))
    if [ "$($A -s "$DEV" logcat -d -t 4000 2>/dev/null | grep -c 'SMCTEST LEVEL_FINISHED')" -gt 0 ]; then
        echo "===== LEVEL_FINISHED apres ${E}s (${RESTARTS} relances) ====="; exit 0
    fi
    [ "$E" -ge "$BUDGET" ] && { echo "timeout ${E}s best=$BEST relances=$RESTARTS"; exit 3; }

    $A -s "$DEV" shell input tap $GOTIT_X $GOTIT_Y >/dev/null 2>&1   # dismiss hints
    sleep 3

    MODE=$($A -s "$DEV" logcat -d -t 600 2>/dev/null | grep -oE "mode=[0-9]" | tail -1 | cut -d= -f2)
    if [ "$MODE" = "3" ]; then       # back at the main menu: game over
        RESTARTS=$((RESTARTS+1))
        echo "  relance $RESTARTS (t=${E}s, best=$BEST)"
        bash /tmp/enter-level.sh "$DEV" >/dev/null 2>&1
        continue
    fi

    X=$($A -s "$DEV" logcat -d -t 300 2>/dev/null | grep -oE "PLAYERPOS x=-?[0-9.]+" | cut -d= -f2 | tail -1)
    [ -n "$X" ] && python3 -c "import sys;sys.exit(0 if float('$X')>float('$BEST') else 1)" 2>/dev/null && BEST=$X
    echo "  t=${E}s x=$X best=$BEST"
done
