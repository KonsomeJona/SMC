/***************************************************************************
 * debug_log.h  -  Debug logging macros for SMC
 *
 * Provides LOG_DEBUG(category, fmt, ...) and LOG_INIT(fmt, ...) macros
 * that print to stderr. Disable all logging by defining SMC_DEBUG_LOG_DISABLE
 * before including this header.
 ***************************************************************************/
#ifndef SMC_DEBUG_LOG_H
#define SMC_DEBUG_LOG_H

#include <cstdio>

// Define SMC_DEBUG_LOG_DISABLE to turn off all debug logging at compile time
#ifdef SMC_DEBUG_LOG_DISABLE
	#define LOG_DEBUG(category, fmt, ...) ((void)0)
	#define LOG_INIT(fmt, ...) ((void)0)
#else
	#define LOG_DEBUG(category, fmt, ...) \
		fprintf(stderr, "[" #category "] " fmt "\n", ##__VA_ARGS__)
	#define LOG_INIT(fmt, ...) \
		fprintf(stderr, "[INIT] " fmt "\n", ##__VA_ARGS__)
#endif

// Category names for reference:
// INPUT    - keyboard/joystick/mouse input events
// VIDEO    - video/rendering/OpenGL
// AUDIO    - audio init, sound/music playback
// MENU     - menu navigation and actions
// PLAYER   - player actions (jumping, movement)
// GAME     - game mode changes, update loop
// GUI      - GUI/HUD events
// CEGUI_LOG - CEGUI input injection

#endif // SMC_DEBUG_LOG_H
