#!/bin/bash
# enter-level.sh — from a cold start, walk the menus into World 1 level 1.
A=~/Library/Android/sdk/platform-tools/adb
DEV="${1:-emulator-5554}"
$A -s "$DEV" shell am force-stop com.takohi.secretchronicles
$A -s "$DEV" logcat -c
$A -s "$DEV" shell am start -n com.takohi.secretchronicles/org.smc.SMCActivity >/dev/null 2>&1
sleep 26
$A -s "$DEV" shell input tap 1073 270; sleep 5    # Start
$A -s "$DEV" shell input tap 319 309;  sleep 2    # World 1
$A -s "$DEV" shell input tap 961 867;  sleep 8    # Enter
$A -s "$DEV" shell input tap 1896 913; sleep 11   # door -> level
$A -s "$DEV" logcat -d -t 200 2>/dev/null | grep -oE "PLAYERPOS x=-?[0-9.]+" | tail -1
