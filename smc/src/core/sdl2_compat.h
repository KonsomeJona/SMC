#ifndef SMC_SDL2_COMPAT_H
#define SMC_SDL2_COMPAT_H

// SDL2 compatibility layer for SDL 1.2 code

// GL headers — GLEW on desktop, GLES2 on Android
#ifdef __ANDROID__
  #include <GLES2/gl2.h>
  // GLEW is not available on Android; GLES2 headers are sufficient
#else
  // GLEW must be included before any other GL headers (SDL2 may pull in gl.h)
  #include <GL/glew.h>
#endif
#include <SDL2/SDL.h>

// Type renames
typedef SDL_Keycode SDLKey;

// SDLK_LAST doesn't exist in SDL2, use a reasonable max for key arrays
#define SDLK_LAST SDL_NUM_SCANCODES

// Renamed key constants
#define SDLK_KP0 SDLK_KP_0
#define SDLK_KP1 SDLK_KP_1
#define SDLK_KP2 SDLK_KP_2
#define SDLK_KP3 SDLK_KP_3
#define SDLK_KP4 SDLK_KP_4
#define SDLK_KP5 SDLK_KP_5
#define SDLK_KP6 SDLK_KP_6
#define SDLK_KP7 SDLK_KP_7
#define SDLK_KP8 SDLK_KP_8
#define SDLK_KP9 SDLK_KP_9
#define SDLK_SCROLLOCK SDLK_SCROLLLOCK
#define SDLK_NUMLOCK SDLK_NUMLOCKCLEAR
#define SDLK_LSUPER SDLK_LGUI
#define SDLK_RSUPER SDLK_RGUI
#define SDLK_PRINT SDLK_PRINTSCREEN
#define SDLK_COMPOSE SDLK_APPLICATION

// SDL2 removed SDL_VIDEORESIZE, use SDL_WINDOWEVENT instead
#define SDL_VIDEORESIZE SDL_WINDOWEVENT

// SDL2 removed SDL_ACTIVEEVENT
// Handled via SDL_WINDOWEVENT_FOCUS_GAINED/LOST

// SDL_SWSURFACE is 0 in SDL2 (software surfaces are default)
#ifndef SDL_SWSURFACE
#define SDL_SWSURFACE 0
#endif

// Global SDL2 window and GL context (replaces SDL_Surface *screen)
extern SDL_Window *g_sdl_window;
extern SDL_GLContext g_gl_context;

#endif
