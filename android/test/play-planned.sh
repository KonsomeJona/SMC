#!/bin/bash
# play-planned.sh — runs a level from a jump plan built out of the level file.
#
# Short cycles (~0.55 s) so the pilot reacts before an enemy arrives, and a
# jump window scaled by the player's speed instead of a fixed 190 px: at
# vx=10 the player covers ~90 px per cycle, so a fixed window fires either
# far too early or a cycle too late.
#
# Usage: play-planned.sh DEVICE JUMPS_CSV EXIT_X TIMEOUT TAG

A=~/Library/Android/sdk/platform-tools/adb
DEV="$1"; JUMPS="$2"; EXITX="$3"; BUDGET="${4:-600}"; TAG="${5:-lvl}"

RIGHT_X=462;  RIGHT_Y=773
LEFT_X=158;   LEFT_Y=773
JUMP_X=1896;  JUMP_Y=913

LIVE=/tmp/live-$TAG.log
$A -s "$DEV" logcat -c 2>/dev/null
( $A -s "$DEV" logcat > "$LIVE" 2>/dev/null ) & LOGPID=$!
trap 'kill $LOGPID 2>/dev/null' EXIT
sleep 2

state()    { tail -n 400 "$LIVE" 2>/dev/null | grep -oE "PLAYERPOS x=-?[0-9.]+ y=-?[0-9.]+ vx=-?[0-9.]+ vy=-?[0-9.]+ ground=[01]" | tail -1; }
finished() { grep -c "SMCTEST LEVEL_FINISHED" "$LIVE" 2>/dev/null; }

START=$(date +%s); BEST=0; PREV=0; STALL=0
echo "[$TAG] plan $(echo $JUMPS | tr ',' ' ' | wc -w) sauts, sortie x=$EXITX"

while :; do
    ELAPSED=$(( $(date +%s) - START ))
    [ "$(finished)" -gt 0 ] && { echo "[$TAG] ===== NIVEAU TERMINE en ${ELAPSED}s ====="; exit 0; }
    [ "$ELAPSED" -ge "$BUDGET" ] && { echo "[$TAG] TIMEOUT ${ELAPSED}s (x max $BEST)"; exit 3; }

    S=$(state)
    X=$(echo "$S" | grep -oE "x=-?[0-9.]+" | cut -d= -f2); X=${X:-0}
    VX=$(echo "$S" | grep -oE "vx=-?[0-9.]+" | cut -d= -f2); VX=${VX:-0}
    GND=$(echo "$S" | grep -oE "ground=[01]" | cut -d= -f2); GND=${GND:-1}

    python3 -c "import sys;sys.exit(0 if float('$X')>float('$BEST') else 1)" 2>/dev/null && BEST=$X

    # Jump when a planned hazard falls inside the distance the player will
    # cover during this cycle plus a safety margin.
    NEED=$(python3 -c "
x=float('$X'); vx=abs(float('$VX'))
reach = max(90.0, vx*95.0)          # ~0.95 s of travel
plan=[float(v) for v in '$JUMPS'.split(',') if v]
print(1 if any(x-20 <= p <= x+reach for p in plan) else 0)" 2>/dev/null)

    if python3 -c "import sys;sys.exit(0 if abs(float('$X')-float('$PREV'))<12 else 1)" 2>/dev/null; then
        STALL=$((STALL+1)); NEED=1
    else
        STALL=0
    fi
    PREV=$X

    if [ "$STALL" -ge 4 ]; then
        $A -s "$DEV" shell input swipe $LEFT_X $LEFT_Y $LEFT_X $LEFT_Y 600 >/dev/null 2>&1
        STALL=0
    fi

    $A -s "$DEV" shell input swipe $RIGHT_X $RIGHT_Y $RIGHT_X $RIGHT_Y 950 >/dev/null 2>&1 &
    R=$!
    # Jump every cycle: airborne clears both crates and enemies, and a small
    # Maryo dies to any contact he stays on the ground for.
    $A -s "$DEV" shell input swipe $JUMP_X $JUMP_Y $JUMP_X $JUMP_Y 700 >/dev/null 2>&1 &
    wait $R 2>/dev/null
done
