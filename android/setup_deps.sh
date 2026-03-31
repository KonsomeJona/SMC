#!/bin/bash
# Download SDL2 dependencies for Android build
set -e
cd "$(dirname "$0")/app/jni"
[ -d SDL2 ]       || git clone --depth=1 --branch release-2.28.5 https://github.com/libsdl-org/SDL.git SDL2
[ -d SDL2_image ] || git clone --depth=1 --branch SDL2 https://github.com/libsdl-org/SDL_image.git SDL2_image
[ -d SDL2_mixer ] || git clone --depth=1 --branch SDL2 https://github.com/libsdl-org/SDL_mixer.git SDL2_mixer
[ -d SDL2_ttf ]   || git clone --depth=1 --branch SDL2 https://github.com/libsdl-org/SDL_ttf.git SDL2_ttf
echo "SDL2 dependencies ready."
