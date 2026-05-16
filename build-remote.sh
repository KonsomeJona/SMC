#!/usr/bin/env bash
# build-remote.sh — Delegate Android build to remote (Ryzen 7 8845HS).
#
# Usage:
#   ./build-remote.sh                  # build :app:assembleDebug, fetch APK
#   ./build-remote.sh release          # build :app:assembleRelease (signed)
#   ./build-remote.sh -i               # also install on connected device
#   ./build-remote.sh release -i       # release build + install
#
# Workflow:
#   1. rsync project to remote (delta-only after first run)
#   2. gradle build on remote
#   3. rsync APK back to local build/ outputs
#   4. Optional: adb install on first connected device
set -euo pipefail

REMOTE="jona@100.108.188.39"
REMOTE_DIR="~/smc-builds/smc"
LOCAL_DIR="$(cd "$(dirname "$0")" && pwd)"
ADB="/mnt/d/android-studio-data/Sdk/Sdk/platform-tools/adb.exe"

# Parse args
VARIANT="debug"
INSTALL=0
for arg in "$@"; do
    case "$arg" in
        release) VARIANT="release" ;;
        debug)   VARIANT="debug" ;;
        -i|--install) INSTALL=1 ;;
        *) echo "Unknown arg: $arg"; exit 1 ;;
    esac
done

# Capitalize first letter (Debug / Release) for gradle task name
GRADLE_TASK=":app:assemble$(tr '[:lower:]' '[:upper:]' <<< "${VARIANT:0:1}")${VARIANT:1}"
APK_REL="android/app/build/outputs/apk/${VARIANT}/app-${VARIANT}.apk"

echo "==> [1/4] rsync project → remote"
time rsync -az --delete-after \
    --exclude='.git/' \
    --exclude='android/app/build/' \
    --exclude='android/.gradle/' \
    --exclude='android/app/.cxx/' \
    --exclude='build-remote.sh' \
    --exclude='*.apk' \
    --exclude='*.aab' \
    "$LOCAL_DIR/" "$REMOTE:$REMOTE_DIR/"

echo "==> [2/4] gradle $GRADLE_TASK on remote"
time ssh "$REMOTE" "bash -lc '
    export ANDROID_SDK_ROOT=\$HOME/Android/Sdk
    export ANDROID_HOME=\$ANDROID_SDK_ROOT
    cd $REMOTE_DIR/android
    ./gradlew $GRADLE_TASK
'"

echo "==> [3/4] fetch APK ← remote"
mkdir -p "$LOCAL_DIR/$(dirname "$APK_REL")"
time rsync -az --progress \
    "$REMOTE:$REMOTE_DIR/$APK_REL" \
    "$LOCAL_DIR/$APK_REL"

ls -lh "$LOCAL_DIR/$APK_REL"

if [ "$INSTALL" = "1" ]; then
    echo "==> [4/4] adb install on device"
    # Convert /mnt/e/... to E:\... for Windows adb
    APK_WIN=$(echo "$LOCAL_DIR/$APK_REL" | sed -E 's|^/mnt/([a-z])/|\U\1:\\|; s|/|\\|g')
    # Pick the phone (skip the Pixel Watch and other Wear devices)
    PHONE=$("$ADB" devices -l 2>/dev/null \
        | awk '/device .*model:/ && !/Watch|wear/I {print $1; exit}')
    if [ -z "$PHONE" ]; then
        echo "ERROR: no phone device found in 'adb devices'"
        "$ADB" devices -l
        exit 1
    fi
    echo "  → target: $PHONE"
    "$ADB" -s "$PHONE" install -r "$APK_WIN"
fi

echo "==> done."
