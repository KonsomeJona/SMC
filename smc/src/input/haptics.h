/***************************************************************************
 * haptics.h  -  short haptic feedback on the touch controls
 *
 * A single integer crosses the JNI boundary; everything about Android's
 * vibration APIs stays in SMCActivity.java. On any other platform every
 * call here is a no-op.
 ***************************************************************************/
#ifndef SMC_HAPTICS_H
#define SMC_HAPTICS_H

namespace SMC
{

enum Haptic_Kind
{
	HAPTIC_TICK  = 0,   // d-pad press
	HAPTIC_CLICK = 1,   // jump, shoot
	HAPTIC_HEAVY = 2    // taking damage
};

/* Look up the Java side once. Safe to call when there is no vibrator. */
void Haptics_Init( void );

/* Play one effect, unless the player turned haptics off or the last one was
 * too recent. */
void Haptics_Play( Haptic_Kind kind );

} // namespace SMC

#endif
