cmake_minimum_required(VERSION 3.22)
project(smc VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Source root — relative to this CMakeLists.txt (android/app/jni/ → smc/src/)
# ---------------------------------------------------------------------------
set(SMC_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../../../../smc/src")

# ---------------------------------------------------------------------------
# Collect all C++ sources, then remove files that are not compatible with
# the Android / no-editor / no-CEGUI build.
# ---------------------------------------------------------------------------
file(GLOB_RECURSE ALL_SMC_SOURCES "${SMC_SRC}/*.cpp")

# Files that depend on desktop GL / CEGUI / editor UI — excluded on Android.
set(ANDROID_EXCLUDED_SOURCES
    # video.cpp has desktop GL and CEGUI calls that need to be ported
    "${SMC_SRC}/video/video.cpp"
    # Editor files (CEGUI-heavy)
    "${SMC_SRC}/core/editor.cpp"
    "${SMC_SRC}/level/level_editor.cpp"
    "${SMC_SRC}/overworld/world_editor.cpp"
    # Level settings UI (CEGUI)
    "${SMC_SRC}/level/level_settings.cpp"
    # Help card (CEGUI)
    "${SMC_SRC}/gui/help_card.cpp"
)

# Remove excluded sources from the full list
foreach(_excl ${ANDROID_EXCLUDED_SOURCES})
    list(REMOVE_ITEM ALL_SMC_SOURCES "${_excl}")
endforeach()

# gles2_renderer.cpp is picked up automatically by the GLOB_RECURSE above.
# It is guarded by #ifdef __ANDROID__ so it only compiles on Android.

# ---------------------------------------------------------------------------
# SDL2 — built from source (subprojects cloned by setup_deps.sh)
# ---------------------------------------------------------------------------
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON  CACHE BOOL "" FORCE)
add_subdirectory(SDL2)
add_subdirectory(SDL2_image)
add_subdirectory(SDL2_mixer)
add_subdirectory(SDL2_ttf)

# ---------------------------------------------------------------------------
# Android system libraries
# ---------------------------------------------------------------------------
find_library(ANDROID_LIB    android  REQUIRED)
find_library(LOG_LIB        log      REQUIRED)
find_library(GLESV2_LIB     GLESv2   REQUIRED)
find_library(OPENSLES_LIB   OpenSLES REQUIRED)
find_library(EGL_LIB        EGL      REQUIRED)

# ---------------------------------------------------------------------------
# Build the shared library that SDL2 will dlopen()
# ---------------------------------------------------------------------------
add_library(smc SHARED ${ALL_SMC_SOURCES})

target_include_directories(smc PRIVATE
    SDL2/include
    SDL2_image
    SDL2_mixer
    SDL2_ttf
    ${SMC_SRC}
    ${SMC_SRC}/core  # for tinyxml2.h, sdl2_compat.h
)

target_compile_definitions(smc PRIVATE
    # Platform guards
    __ANDROID__
    # Disable subsystems not yet ported
    SMC_NO_CEGUI
    SMC_NO_EDITOR
    SMC_NO_BOOST
    # Data directory is set at runtime via env var SMC_DATA_DIR (see SMCActivity.java)
    DATA_DIR=""
    # Suppress debug logging in release; remove for debug builds
    $<$<NOT:$<CONFIG:Debug>>:SMC_DEBUG_LOG_DISABLE>
)

# Android requires position-independent code for shared libs
set_target_properties(smc PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

target_link_libraries(smc PRIVATE
    # SDL2 family (static, built from source)
    SDL2-static
    SDL2_image
    SDL2_mixer
    SDL2_ttf
    # Android / OpenGL ES 2.0
    ${GLESV2_LIB}
    ${EGL_LIB}
    ${ANDROID_LIB}
    ${LOG_LIB}
    ${OPENSLES_LIB}
    # C++ standard library (shared, matched to ANDROID_STL=c++_shared)
    c++_shared
)
