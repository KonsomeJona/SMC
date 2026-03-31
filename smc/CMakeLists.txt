cmake_minimum_required(VERSION 3.14)

# Android uses android/app/jni/CMakeLists.txt — do not use this file for Android builds.
if(ANDROID)
    return()
endif()

project(smc VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use pkg-config for SDL2 and CEGUI
find_package(PkgConfig REQUIRED)

pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(SDL2_IMAGE REQUIRED SDL2_image)
pkg_check_modules(SDL2_MIXER REQUIRED SDL2_mixer)
pkg_check_modules(SDL2_TTF REQUIRED SDL2_ttf)

pkg_check_modules(CEGUI REQUIRED CEGUI-0)
pkg_check_modules(CEGUI_OPENGL REQUIRED CEGUI-0-OPENGL)
pkg_check_modules(CEGUI_NULL REQUIRED CEGUI-0-NULL)

find_package(Boost REQUIRED COMPONENTS filesystem thread system)
find_package(OpenGL REQUIRED)
find_package(PNG REQUIRED)
find_package(X11 REQUIRED)

# Gather all source files
file(GLOB_RECURSE SMC_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_executable(smc ${SMC_SOURCES})

target_include_directories(smc PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_IMAGE_INCLUDE_DIRS}
    ${SDL2_MIXER_INCLUDE_DIRS}
    ${SDL2_TTF_INCLUDE_DIRS}
    ${CEGUI_INCLUDE_DIRS}
    ${CEGUI_OPENGL_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
    ${OPENGL_INCLUDE_DIR}
    ${PNG_INCLUDE_DIRS}
    ${X11_INCLUDE_DIR}
)

target_link_libraries(smc PRIVATE
    ${SDL2_LIBRARIES}
    ${SDL2_IMAGE_LIBRARIES}
    ${SDL2_MIXER_LIBRARIES}
    ${SDL2_TTF_LIBRARIES}
    ${CEGUI_LIBRARIES}
    ${CEGUI_OPENGL_LIBRARIES}
    ${CEGUI_NULL_LIBRARIES}
    GLU
    Boost::filesystem
    Boost::thread
    Boost::system
    OpenGL::GL
    PNG::PNG
    ${X11_LIBRARIES}
)

target_compile_definitions(smc PRIVATE
    DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
    $<$<NOT:$<CONFIG:Debug>>:SMC_DEBUG_LOG_DISABLE>
)
