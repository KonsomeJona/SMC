#!/bin/bash
# play-planned.sh — runs a level using a jump plan derived from the level file.
#
# A reactive pilot dies: small Maryo is killed by any contact, and by the time
# a stall is measured the enemy has already landed on him. The level XML gives
# every enemy position and every real gap, so the jumps are known in advance.
#
# Usage: play-planned.sh DEVICE JUMPS_CSV EXIT_X TIMEOUT TAG

A=~/Library/Android/sdk/platform-tools/adb
DEV="$1"; JUMPS="$2"; EXITX="$3"; BUDGET="${4:-400}"; TAG="${5:-lvl}"

RIGHT_X=462;  RIGHT_Y=773
JUMP_X=1896;  JUMP_Y=913

LIVE=/tmp/live-$TAG.log
$A -s "$DEV" logcat -c 2>/dev/null
( $A -s "$DEV" logcat > "$LIVE" 2>/dev/null ) & LOGPID=$!
trap 'kill $LOGPID 2>/dev/null' EXIT
sleep 2

getx()     { tail -n 300 "$LIVE" 2>/dev/null | grep -oE "PLAYERPOS x=-?[0-9.]+" | tail -1 | cut -d= -f2; }
finished() { grep -c "SMCTEST LEVEL_FINISHED" "$LIVE" 2>/dev/null; }

START=$(date +%s); BEST=0; STALL=0; PREV=0
echo "[$TAG] plan: $(echo $JUMPS | tr ',' ' ' | wc -w) sauts, sortie x=$EXITX"

while :; do
    ELAPSED=$(( $(date +%s) - START ))
    [ "$(finished)" -gt 0 ] && { echo "[$TAG] TERMINE en ${ELAPSED}s"; exit 0; }
    [ "$ELAPSED" -ge "$BUDGET" ] && { echo "[$TAG] TIMEOUT ${ELAPSED}s (x max $BEST)"; exit 3; }

    X=$(getx); X=${X:-0}
    python3 -c "import sys;sys.exit(0 if float('$X')>float('$BEST') else 1)" 2>/dev/null && BEST=$X

    # Jump if a planned hazard is just ahead of the player.
    NEED=$(python3 -c "
x=float('$X')
plan=[float(v) for v in '$JUMPS'.split(',') if v]
print(1 if any(x <= p <= x+190 for p in plan) else 0)" 2>/dev/null)

    # The plan covers enemies and gaps. Walls and pipes are neither, so also
    # jump whenever forward progress has stopped.
    if python3 -c "import sys;sys.exit(0 if abs(float('$X')-float('${PREV:-0}'))<15 else 1)" 2>/dev/null; then
        STALL=$((STALL+1))
    else
        STALL=0
    fi
    PREV=$X
    [ "$STALL" -ge 1 ] && NEED=1

    $A -s "$DEV" shell input swipe $RIGHT_X $RIGHT_Y $RIGHT_X $RIGHT_Y 900 >/dev/null 2>&1 &
    R=$!
    if [ "$NEED" = "1" ]; then
        $A -s "$DEV" shell input swipe $JUMP_X $JUMP_Y $JUMP_X $JUMP_Y 650 >/dev/null 2>&1 &
        if [ "$STALL" -ge 3 ]; then
            sleep 0.35
            $A -s "$DEV" shell input swipe $JUMP_X $JUMP_Y $JUMP_X $JUMP_Y 650 >/dev/null 2>&1 &
        fi
    fi
    wait $R 2>/dev/null

    [ -n "$X" ] && echo "  [$TAG] t=${ELAPSED}s x=$X jump=$NEED best=$BEST"
done
