/***************************************************************************
 * debug_log.h  -  Debug logging macros for SMC
 *
 * Provides LOG_DEBUG(category, fmt, ...) and LOG_INIT(fmt, ...).
 *
 * On Android the messages go through liblog: stderr is not attached to
 * logcat there, so every LOG_DEBUG was silently dropped — which hid the
 * engine's own diagnostics exactly where they are hardest to get otherwise.
 * Everywhere else they still go to stderr.
 *
 * Disable all logging by defining SMC_DEBUG_LOG_DISABLE before including.
 ***************************************************************************/
#ifndef SMC_DEBUG_LOG_H
#define SMC_DEBUG_LOG_H

#include <cstdio>

#ifdef __ANDROID__
	#include <android/log.h>
#endif

// Define SMC_DEBUG_LOG_DISABLE to turn off all debug logging at compile time
#ifdef SMC_DEBUG_LOG_DISABLE
	#define LOG_DEBUG(category, fmt, ...) ((void)0)
	#define LOG_INIT(fmt, ...) ((void)0)
#elif defined(__ANDROID__)
	#define LOG_DEBUG(category, fmt, ...) \
		__android_log_print( ANDROID_LOG_DEBUG, "SMC", "[" #category "] " fmt, ##__VA_ARGS__ )
	#define LOG_INIT(fmt, ...) \
		__android_log_print( ANDROID_LOG_INFO, "SMC", "[INIT] " fmt, ##__VA_ARGS__ )
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
