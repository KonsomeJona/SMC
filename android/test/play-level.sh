#!/bin/bash
# play-level.sh — finishes a level through the on-screen pad.
#
# Default is to run right without jumping: jumping on every cycle makes the
# player overshoot platforms and fall into pits. A jump is fired only when
# forward progress stalls, and it is launched in the same burst as the hold so
# both fingers land together — a jump started mid-hold is dropped.
#
# Usage: play-level.sh [device] [timeout] [tag]

A=~/Library/Android/sdk/platform-tools/adb
DEV="${1:-emulator-5554}"
BUDGET="${2:-420}"
TAG="${3:-lvl}"

RIGHT_X=462;  RIGHT_Y=773
JUMP_X=1896;  JUMP_Y=913

# One long-lived logcat writing to a file. Polling with `logcat -d` while the
# same adb server is busy with `input swipe` returned empty output often enough
# to look like the player had left the level.
LIVE=/tmp/live-$TAG.log
$A -s "$DEV" logcat -c 2>/dev/null
( $A -s "$DEV" logcat > "$LIVE" 2>/dev/null ) &
LOGPID=$!
trap 'kill $LOGPID 2>/dev/null' EXIT
sleep 2

lc()       { tail -n "${1:-200}" "$LIVE" 2>/dev/null; }
getx()     { lc 200 | grep -oE "PLAYERPOS x=-?[0-9.]+" | tail -1 | cut -d= -f2; }
ground()   { lc 40  | grep -oE "ground=[01]" | tail -1 | cut -d= -f2; }
finished() { lc 800 | grep -c "SMCTEST LEVEL_FINISHED"; }
inlevel()  { lc 400 | grep -c "PLAYERPOS"; }

# Hold right for ~1.2 s; optionally press jump at the same instant.
step() {
  $A -s "$DEV" shell input swipe $RIGHT_X $RIGHT_Y $RIGHT_X $RIGHT_Y 1200 >/dev/null 2>&1 &
  local R=$!
  if [ "$1" = "jump" ]; then
      # Two jumps inside the hold: clearing a stack of crates takes more than
      # one hop, and a single jump per cycle stalled against the same wall.
      $A -s "$DEV" shell input swipe $JUMP_X $JUMP_Y $JUMP_X $JUMP_Y 650 >/dev/null 2>&1 &
      local J=$!
      wait $J 2>/dev/null
      $A -s "$DEV" shell input swipe $JUMP_X $JUMP_Y $JUMP_X $JUMP_Y 650 >/dev/null 2>&1 &
  fi
  wait $R 2>/dev/null
}

GONE=0; START=$(date +%s); BEST=$(getx); BEST=${BEST:-0}; PREV=$BEST; STALL=0; ITER=0
echo "[$TAG] depart x=$BEST budget=${BUDGET}s"

while :; do
    ELAPSED=$(( $(date +%s) - START ))
    if [ "$(finished)" -gt 0 ]; then echo "[$TAG] TERMINE en ${ELAPSED}s (x max $BEST)"; exit 0; fi
    if [ "$ELAPSED" -ge "$BUDGET" ]; then echo "[$TAG] TIMEOUT ${ELAPSED}s (x max $BEST)"; exit 3; fi

    ITER=$((ITER+1))
    if [ $((ITER % 4)) -eq 0 ] && [ "$STALL" -eq 0 ]; then step; else step jump; fi

    X=$(getx)
    if [ -n "$X" ] && [ -n "$PREV" ]; then
        if python3 -c "import sys;sys.exit(0 if abs(float('$X')-float('$PREV'))<20 else 1)" 2>/dev/null; then
            STALL=$((STALL+1))
        else
            STALL=0
        fi
        if python3 -c "import sys;sys.exit(0 if float('$X')>float('$BEST') else 1)" 2>/dev/null; then BEST=$X; fi
        PREV=$X
    fi

    if [ "$ITER" -gt 2 ] && [ "$(inlevel)" -eq 0 ]; then
        GONE=$((GONE+1)); [ "$GONE" -ge 10 ] && { echo "[$TAG] SORTI DU NIVEAU a ${ELAPSED}s x max $BEST"; exit 4; }
    else
        GONE=0
    fi

    [ $((ITER % 6)) -eq 0 ] && echo "  [$TAG] t=${ELAPSED}s x=$X best=$BEST stall=$STALL"
done
