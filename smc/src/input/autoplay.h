/***************************************************************************
 * autoplay.h  -  Scripted player for automated runs (Android)
 *
 * Drives the game by pressing the ON-SCREEN PAD, the same zones a thumb
 * touches, so a run exercises the real input path: zone hit -> SDL key event
 * -> cLevel::Key_Down. It does not call gameplay functions directly.
 *
 * The decisions come from the live level: the sprite manager knows where the
 * ground ends, where a wall rises and where the enemies are, which is exactly
 * what a pilot driving through adb cannot see in time.
 *
 * Enabled by creating the file <user dir>/autoplay (SMCActivity's files dir).
 ***************************************************************************/
#ifndef SMC_AUTOPLAY_H
#define SMC_AUTOPLAY_H

namespace SMC
{

// Reads the enable flag once, at startup.
void Autoplay_Init( void );
// True when the scripted player is driving.
bool Autoplay_Enabled( void );
// Called once per frame from the main loop, after Update_Game().
void Autoplay_Update( void );

} // namespace SMC

#endif
