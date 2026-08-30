#!/usr/bin/env bash
# instrumental-test.sh — automated pass over the Android build.
#
# Every check asserts on engine output (logcat) or on measured state, never on
# a screenshot someone has to interpret. Run this before touching the device
# by hand; a red line here is a bug you would otherwise hunt with your thumb.
#
# Usage:
#   ./instrumental-test.sh [-s DEVICE] [--apk PATH] [--skip-install]
#
# Exit code 0 = every test passed.

set -uo pipefail

ADB="${ADB:-/mnt/d/android-studio-data/Sdk/Sdk/platform-tools/adb.exe}"
PKG="me.takohi.bandagoo"
ACT="$PKG/org.smc.SMCActivity"
APK_WIN="${APK_WIN:-D:\\tools\\tmp\\smc-debug.apk}"
DEVICE=""
SKIP_INSTALL=0

while [ $# -gt 0 ]; do
    case "$1" in
        -s) DEVICE="$2"; shift 2 ;;
        --apk) APK_WIN="$2"; shift 2 ;;
        --skip-install) SKIP_INSTALL=1; shift ;;
        *) echo "argument inconnu: $1" >&2; exit 2 ;;
    esac
done

if [ -z "$DEVICE" ]; then
    DEVICE=$("$ADB" devices | awk 'NR>1 && $2=="device" {print $1; exit}')
fi
[ -n "$DEVICE" ] || { echo "aucun appareil connecte"; exit 2; }

A() { "$ADB" -s "$DEVICE" "$@"; }

# A dropped Wi-Fi ADB link makes every later command hang on its own timeout
# and the run looks frozen. Fail fast and say why instead.
if ! A shell true >/dev/null 2>&1; then
    echo "l appareil $DEVICE ne repond pas (lien ADB tombe ?)"
    echo "  -> bash /mnt/d/tools/adb-connect-all.sh  puis relancer"
    exit 2
fi

PASS=0; FAIL=0; FAILED_NAMES=""
ok()   { PASS=$((PASS+1)); printf '  \033[32mPASS\033[0m  %s\n' "$1"; }
ko()   { FAIL=$((FAIL+1)); FAILED_NAMES="$FAILED_NAMES\n    - $1: $2"
         printf '  \033[31mFAIL\033[0m  %s\n        %s\n' "$1" "$2"; }
phase(){ printf '\n\033[1m== %s ==\033[0m\n' "$1"; }

# assert_log NAME PATTERN DESC   — pattern must appear in the captured log
assert_log() {
    if grep -qE "$2" /tmp/smc-test.log; then ok "$1"
    else ko "$1" "attendu dans les logs: $2"; fi
}
# assert_no_log NAME PATTERN DESC — pattern must NOT appear
assert_no_log() {
    local n; n=$(grep -cE "$2" /tmp/smc-test.log)
    if [ "$n" -eq 0 ]; then ok "$1"; else ko "$1" "$n occurrence(s) de: $2"; fi
}
grab_log() { A logcat -d 2>/dev/null | grep -E "SDL/APP|SMC |AndroidRuntime|libc  |CompatChange" > /tmp/smc-test.log; }

# ---------------------------------------------------------------- geometry
# Touch zone centres, derived from cTouchControls::Init_Zones so the taps land
# where the pad is actually drawn whatever the screen size.
read -r SW SH <<< "$(A shell wm size | awk -F'[ x]' '/Override|Physical/ {w=$(NF-1); h=$NF} END {print h" "w}' | tr -d '\r')"
read -r DPAD_L_X DPAD_L_Y DPAD_R_X DPAD_R_Y JUMP_X JUMP_Y SHOOT_X SHOOT_Y PAUSE_X PAUSE_Y <<< "$(
python3 - "$SW" "$SH" <<'PY'
import sys
sw, sh = float(sys.argv[1]), float(sys.argv[2])
bs   = sh * 0.14
pad  = sh * 0.004
mL   = sw * 0.035
mB   = sh * 0.07
top  = sh - mB - bs*3 - pad*2
dcx  = mL + bs + pad
dcy  = top + bs + pad
abtn = bs * 1.20
agap = abtn * 0.60
mR   = sw * 0.08
mAB  = sh * 0.07
jx   = sw - mR - abtn
jy   = sh - mAB - abtn
sysS = sh * 0.14
print(int(mL+bs/2), int(dcy+bs/2),
      int(dcx+bs+pad+bs/2), int(dcy+bs/2),
      int(jx+abtn/2), int(jy+abtn/2),
      int(jx-abtn-agap+abtn/2), int(jy+abtn/2),
      int(sw-mR-sysS/2), int(sh*0.25+sysS/2))
PY
)"

echo "appareil : $DEVICE   ecran ${SW}x${SH} (paysage)"
echo "zones    : dpadL=$DPAD_L_X,$DPAD_L_Y dpadR=$DPAD_R_X,$DPAD_R_Y jump=$JUMP_X,$JUMP_Y shoot=$SHOOT_X,$SHOOT_Y"

# ---------------------------------------------------------------- install
phase "1. Installation et demarrage"
A shell am force-stop "$PKG" >/dev/null 2>&1
if [ "$SKIP_INSTALL" = "0" ]; then
    if A install -r -t "$APK_WIN" 2>&1 | grep -q Success; then ok "install de l APK"
    else ko "install de l APK" "adb install a echoue"; fi
fi
A logcat -c >/dev/null 2>&1
A shell am start -n "$ACT" >/dev/null 2>&1
sleep 30
grab_log

PID=$(A shell pidof "$PKG" | tr -d '\r')
[ -n "$PID" ] && ok "le process tourne (pid $PID)" || ko "le process tourne" "aucun pid"

assert_no_log "aucune exception Java"        "FATAL EXCEPTION|AndroidRuntime: .*Error"
assert_no_log "aucun abort natif"            "FORTIFY|SIGSEGV|SIGABRT"
assert_no_log "aucun rejet de buffer EGL"    "BLASTBufferQueue.*rejecting"
APP_UID=$(A shell "dumpsys package $PKG | grep -m1 userId" | grep -oE '[0-9]+' | head -1)
assert_no_log "pas de mode compat 16 Ko"     "242716250; UID ${APP_UID:-99999}"

phase "2. Demarrage du moteur"
assert_log "assets extraits"                 "DATA_DIR *= */data"
assert_log "video initialisee"               "Init_Video: SDL_GetCurrentDisplayMode"
assert_log "contexte GLES2 cree"             "Init_OpenGL: window="
assert_log "controles tactiles initialises"  "Touch controls initialized: enabled=1"
assert_log "niveau du menu charge"           "cLevel::Load: parsing .*menu_green_1"
assert_log "sprites du menu presents"        "cLevel::Load: [0-9]+ sections, [1-9][0-9]* sprites"

phase "3. Performance"
FPS=$(grep -oE "FPS avg=[0-9]+" /tmp/smc-test.log | tail -1 | cut -d= -f2)
if [ -n "$FPS" ] && [ "$FPS" -ge 50 ]; then ok "framerate >= 50 (mesure: $FPS)"
else ko "framerate >= 50" "mesure: ${FPS:-aucune}"; fi

JOY=$(grep -cE "event type=1536" /tmp/smc-test.log)
if [ "$JOY" -lt 50 ]; then ok "pas de flot d evenements joystick ($JOY)"
else ko "pas de flot d evenements joystick" "$JOY evenements d axe: l accelerometre est expose comme joystick et relache les touches"; fi

phase "4. Navigation jusqu au niveau"
# Restart so the run always begins on the main menu: without this the taps
# below are replayed against whatever screen the previous run left behind.
A shell am force-stop "$PKG" >/dev/null 2>&1
A logcat -c >/dev/null 2>&1
A shell am start -n "$ACT" >/dev/null 2>&1
sleep 26
A shell input tap $((SW*497/1000)) $((SH*273/1000)) >/dev/null 2>&1   # Start
sleep 5
A shell input tap $((SW*146/1000)) $((SH*285/1000)) >/dev/null 2>&1   # World 1
sleep 2
A shell input tap $((SW*448/1000)) $((SH*801/1000)) >/dev/null 2>&1   # Enter
sleep 8
grab_log
assert_no_log "pas de crash a l entree du monde" "FATAL EXCEPTION|FORTIFY"

A logcat -c >/dev/null 2>&1
A shell input tap "$JUMP_X" "$JUMP_Y" >/dev/null 2>&1                 # porte -> niveau
sleep 12
grab_log
assert_log "niveau charge depuis la carte"   "cLevel::Load: parsing .*lvl_"
LVLSPR=$(grep -oE "cLevel::Load: [0-9]+ sections, [0-9]+ sprites" /tmp/smc-test.log | tail -1 | grep -oE "[0-9]+ sprites" | cut -d' ' -f1)
if [ -n "$LVLSPR" ] && [ "$LVLSPR" -gt 100 ]; then ok "le niveau contient du decor ($LVLSPR sprites)"
else ko "le niveau contient du decor" "sprites: ${LVLSPR:-0} — un niveau vide s affiche noir"; fi
assert_log "mode niveau actif"               "PLAYERPOS "

phase "5. Entrees tactiles"
A logcat -c >/dev/null 2>&1
A shell input swipe "$DPAD_R_X" "$DPAD_R_Y" "$DPAD_R_X" "$DPAD_R_Y" 200 >/dev/null 2>&1
sleep 2
grab_log
assert_log "appui detecte sur le D-pad"      "Touch PRESS zone="

# Le test qui compte : le joueur se deplace-t-il vraiment ?
X0=$(A logcat -d 2>/dev/null | grep -oE "PLAYERPOS x=[0-9.]+" | tail -1 | cut -d= -f2)
A logcat -c >/dev/null 2>&1
A shell input swipe "$DPAD_R_X" "$DPAD_R_Y" "$DPAD_R_X" "$DPAD_R_Y" 2500 >/dev/null 2>&1
sleep 3
X1=$(A logcat -d 2>/dev/null | grep -oE "PLAYERPOS x=[0-9.]+" | tail -1 | cut -d= -f2)
if [ -n "$X0" ] && [ -n "$X1" ] && python3 -c "import sys; sys.exit(0 if float('$X1') > float('$X0') + 20 else 1)"; then
    ok "le joueur avance vers la droite ($X0 -> $X1)"
else
    ko "le joueur avance vers la droite" "x avant=$X0 apres=$X1 — l appui n a pas produit de deplacement"
fi

A logcat -c >/dev/null 2>&1
A shell input swipe "$JUMP_X" "$JUMP_Y" "$JUMP_X" "$JUMP_Y" 400 >/dev/null 2>&1
sleep 1
VY=$(A logcat -d 2>/dev/null | grep -oE "PLAYERPOS .*vy=-?[0-9.]+" | tail -1 | grep -oE "vy=-?[0-9.]+" | cut -d= -f2)
if [ -n "$VY" ] && python3 -c "import sys; sys.exit(0 if float('$VY') < 0 else 1)"; then
    ok "le saut decolle (vy=$VY)"
else
    ko "le saut decolle" "vy=${VY:-aucune} — attendu negatif juste apres l appui"
fi

phase "6. Cycle de vie"
A logcat -c >/dev/null 2>&1
A shell input keyevent KEYCODE_HOME >/dev/null 2>&1
sleep 3
A shell am start -n "$ACT" >/dev/null 2>&1
sleep 8
grab_log
PID2=$(A shell pidof "$PKG" | tr -d '\r')
[ -n "$PID2" ] && ok "survit a un passage en arriere-plan" || ko "survit a un passage en arriere-plan" "process mort"
assert_no_log "pas de perte de contexte GL"  "FATAL EXCEPTION|FORTIFY|call to OpenGL ES API with no current context"

# ---------------------------------------------------------------- resume
printf '\n\033[1m== Resultat ==\033[0m\n'
printf '  %d reussis, %d echoues\n' "$PASS" "$FAIL"
[ "$FAIL" -gt 0 ] && printf '  echecs:%b\n' "$FAILED_NAMES"
exit $([ "$FAIL" -eq 0 ] && echo 0 || echo 1)
