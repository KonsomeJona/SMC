cmake_minimum_required(VERSION 3.22)
project(smc VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Source root — relative to this CMakeLists.txt (android/app/jni/ → smc/src/)
# ---------------------------------------------------------------------------
set(SMC_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../../../smc/src")

# Fail loudly: an out-of-tree SMC_SRC makes the GLOB below return nothing and
# CMake then reports the far less obvious "No SOURCES given to target: smc".
if(NOT EXISTS "${SMC_SRC}/core/main.cpp")
    message(FATAL_ERROR "SMC sources not found at ${SMC_SRC} — expected <repo>/smc/src")
endif()

# ---------------------------------------------------------------------------
# Collect all C++ sources, then remove files that are not compatible with
# the Android / no-editor / no-CEGUI build.
# ---------------------------------------------------------------------------
file(GLOB_RECURSE ALL_SMC_SOURCES CONFIGURE_DEPENDS "${SMC_SRC}/*.cpp")

# Files that depend on desktop GL / CEGUI / editor UI — excluded on Android.
set(ANDROID_EXCLUDED_SOURCES
    # Editor files (CEGUI-heavy, excluded via SMC_NO_EDITOR)
    "${SMC_SRC}/core/editor.cpp"
    "${SMC_SRC}/level/level_editor.cpp"
    "${SMC_SRC}/overworld/world_editor.cpp"
    # Spinner widget (CEGUI, editor-only)
    "${SMC_SRC}/gui/spinner.cpp"
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
# SMCActivity.getLibraries() loads SDL2, SDL2_image, SDL2_mixer, SDL2_ttf and
# smc as separate .so files, and SDL2's own JNI entry points (nativeInit &co)
# have to live in libSDL2.so for that to link up. Building SDL2 statically
# also makes the satellite libraries disagree (INTERFACE_SDL2_SHARED), so the
# whole set stays shared.
set(SDL_SHARED ON  CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
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
    # <SDL2/SDL_image.h> style includes, kept from the desktop build
    sdl2_compat_include
    # libintl.h no-op shim — bionic has no gettext
    posix_compat
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
    # DATA_DIR is baked into 141 literal concatenations (DATA_DIR "/" GUI_FONT_DIR
    # "/x.ttf"), so it cannot carry the runtime path. It keeps its default value
    # of "data" and main() chdir()s to SMC_USER_DIR, the parent of the extracted
    # data/ tree — see core/main.cpp.
    # Suppress debug logging in release only. The debug variant is compiled as
    # RelWithDebInfo (see build.gradle: -O2 plus symbols, the plain Debug -O0
    # runs 5-10x slower), so testing for CONFIG:Debug silenced the debug build
    # too — which is exactly where the logs are needed.
    $<$<CONFIG:Release>:SMC_DEBUG_LOG_DISABLE>
)

# Android requires position-independent code for shared libs
set_target_properties(smc PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

target_link_libraries(smc PRIVATE
    # SDL2 family (shared, built from source — see SDL_SHARED above)
    SDL2
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
