/***************************************************************************
 * haptics.cpp  -  short haptic feedback on the touch controls
 ***************************************************************************/
#include "haptics.h"
#include "../core/global_basic.h"
#include "../user/preferences.h"

#include <SDL.h>

#ifdef __ANDROID__
#include <jni.h>
#endif

namespace SMC
{

#ifdef __ANDROID__

namespace
{
	jclass    s_activity_class = NULL;   // global ref, held for the process
	jmethodID s_vibrate_method = NULL;
	Uint32    s_last_play      = 0;

	// The actuator cannot keep up with one command per frame: a finger sliding
	// across the d-pad would turn a series of ticks into a continuous buzz.
	const Uint32 MIN_INTERVAL_MS = 35;
}

void Haptics_Init( void )
{
	JNIEnv *env = static_cast<JNIEnv *>( SDL_AndroidGetJNIEnv() );

	if( !env ) return;

	// The SDL thread is already attached to the VM, so no AttachCurrentThread.
	jclass local = env->FindClass( "org/smc/SMCActivity" );

	if( !local )
	{
		env->ExceptionClear();
		SDL_Log( "Haptics: SMCActivity not found, haptics disabled" );
		return;
	}

	// A local reference dies with the current frame; the class has to outlive
	// it, hence the global one.
	s_activity_class = static_cast<jclass>( env->NewGlobalRef( local ) );
	env->DeleteLocalRef( local );

	s_vibrate_method = env->GetStaticMethodID( s_activity_class, "nativeVibrate", "(I)V" );

	if( !s_vibrate_method )
	{
		env->ExceptionClear();
		SDL_Log( "Haptics: nativeVibrate not found, haptics disabled" );
		return;
	}

	SDL_Log( "Haptics: ready" );
}

void Haptics_Play( Haptic_Kind kind )
{
	if( !s_vibrate_method || !pPreferences || !pPreferences->m_touch_vibration ) return;

	const Uint32 now = SDL_GetTicks();

	if( now - s_last_play < MIN_INTERVAL_MS ) return;

	s_last_play = now;

	JNIEnv *env = static_cast<JNIEnv *>( SDL_AndroidGetJNIEnv() );

	if( !env ) return;

	env->CallStaticVoidMethod( s_activity_class, s_vibrate_method, static_cast<jint>( kind ) );

	if( env->ExceptionCheck() ) env->ExceptionClear();
}

#else

void Haptics_Init( void ) {}
void Haptics_Play( Haptic_Kind ) {}

#endif

} // namespace SMC
