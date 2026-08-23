#!/bin/bash
# Download SDL2 dependencies for Android build
set -e
cd "$(dirname "$0")/app/jni"
[ -d SDL2 ]       || git clone --depth=1 --branch release-2.28.5 https://github.com/libsdl-org/SDL.git SDL2
[ -d SDL2_image ] || git clone --depth=1 --branch SDL2 https://github.com/libsdl-org/SDL_image.git SDL2_image
[ -d SDL2_mixer ] || git clone --depth=1 --branch SDL2 https://github.com/libsdl-org/SDL_mixer.git SDL2_mixer
[ -d SDL2_ttf ]   || git clone --depth=1 --branch SDL2 https://github.com/libsdl-org/SDL_ttf.git SDL2_ttf

# SDL2_ttf builds freetype from its vendored submodule, which a plain
# --depth=1 clone does not fetch. Without it CMake fails with
# "No cmake project for freetype found in external/freetype".
# Idempotent: a no-op once the submodule is checked out.
git -C SDL2_ttf submodule update --init --depth 1 external/freetype

# SDL2_mixer's vendored decoders (ogg, vorbis, opus, flac, libxmp, wavpack)
# are submodules too, but none are fetched: app/build.gradle configures the
# mixer with -DSDL2MIXER_VORBIS=STB and turns every other decoder off, which
# is all SMC needs (.ogg + .wav only).

echo "SDL2 dependencies ready."
