/***************************************************************************
 * autoplay.cpp  -  Scripted player for automated runs (Android)
 ***************************************************************************/
#include "autoplay.h"

#ifdef __ANDROID__

#include "../core/global_basic.h"
#include "../core/game_core.h"
#include "../core/framerate.h"
#include "../input/touch_controls.h"
#include "../level/level_player.h"
#include "../core/camera.h"
#include "../core/sprite_manager.h"
#include "../objects/sprite.h"
#include "../objects/level_exit.h"
#include "../core/filesystem/filesystem.h"
#include "../core/debug_log.h"
#include <SDL.h>

namespace SMC
{

static bool s_enabled = false;
static bool s_god = true;         // cleared by a "nogod" file next to the flag
static Uint32 s_jump_until = 0;   // keep the jump zone held until this tick
static Uint32 s_back_until = 0;   // backing up to take a run-up
static float  s_last_x = 0.0f;
static Uint32 s_stuck_since = 0;

void Autoplay_Init( void )
{
	std::string flag = Get_User_Directory() + "autoplay";
	s_enabled = File_Exists( flag );
	s_god = !File_Exists( Get_User_Directory() + "nogod" );
	SDL_Log( "SMCTEST AUTOPLAY %s (%s)", s_enabled ? "on" : "off", flag.c_str() );
}

bool Autoplay_Enabled( void )
{
	return s_enabled;
}

/* How far above the player's feet the nearest solid thing ahead rises.
 * 0 means nothing blocking within reach. */
static float Obstacle_Height( float look )
{
	if( !pActive_Camera || !pActive_Camera->m_sprite_manager ) return 0.0f;

	const float px = pLevel_Player->m_pos_x;
	const float py = pLevel_Player->m_pos_y;
	float highest = 0.0f;

	for( cSprite_List::iterator itr = pActive_Camera->m_sprite_manager->objects.begin();
	     itr != pActive_Camera->m_sprite_manager->objects.end(); ++itr )
	{
		cSprite *obj = (*itr);
		if( !obj || obj->m_massive_type != MASS_MASSIVE ) continue;

		const float dx = obj->m_pos_x - px;
		// Include what the player is already touching: once he is flush
		// against a wall dx is ~0, and skipping it meant he pushed into the
		// wall forever without ever jumping.
		if( dx < -30.0f || dx > look ) continue;

		// posy grows downward: a smaller posy is higher up.
		const float rise = py - obj->m_pos_y;
		if( rise > highest ) highest = rise;
	}

	return highest;
}

/* Is the ground about to disappear in front of the player? */
static bool Gap_Ahead( float look )
{
	if( !pActive_Camera || !pActive_Camera->m_sprite_manager ) return false;

	const float px = pLevel_Player->m_pos_x + look;
	const float py = pLevel_Player->m_pos_y;

	for( cSprite_List::iterator itr = pActive_Camera->m_sprite_manager->objects.begin();
	     itr != pActive_Camera->m_sprite_manager->objects.end(); ++itr )
	{
		cSprite *obj = (*itr);
		if( !obj || obj->m_massive_type != MASS_MASSIVE ) continue;

		if( obj->m_pos_x <= px && obj->m_pos_x + obj->m_col_rect.m_w >= px &&
		    obj->m_pos_y > py )
		{
			return false;   // there is floor over there
		}
	}

	return true;
}

/* Standing on or next to a level exit? Reaching the end is not enough: a beam
 * exit only fires when the player presses UP while on the ground on it. */
/* Which zone opens the level exit the player is standing on, if any.
 *
 * A beam exit answers INP_UP; a warp exit answers the key matching its own
 * direction — level 2 ends on a "down" warp inside a pipe, and pressing up
 * there does nothing at all. Returns -1 when the player is on no exit. */
static int Level_Exit_Zone( void )
{
	if( !pActive_Camera || !pActive_Camera->m_sprite_manager ) return -1;

	for( cSprite_List::iterator itr = pActive_Camera->m_sprite_manager->objects.begin();
	     itr != pActive_Camera->m_sprite_manager->objects.end(); ++itr )
	{
		cSprite *obj = (*itr);
		if( !obj || obj->m_type != TYPE_LEVEL_EXIT ) continue;

		if( !pLevel_Player->m_col_rect.Intersects( obj->m_col_rect ) ) continue;

		cLevel_Exit *exit_obj = static_cast<cLevel_Exit *>( obj );

		if( exit_obj->m_exit_type == LEVEL_EXIT_WARP )
		{
			switch( exit_obj->m_direction )
			{
				case DIR_DOWN:  return ZONE_DPAD_DOWN;
				case DIR_RIGHT: return ZONE_DPAD_RIGHT;
				case DIR_LEFT:  return ZONE_DPAD_LEFT;
				default:        return ZONE_DPAD_UP;
			}
		}

		return ZONE_DPAD_UP;
	}

	return -1;
}

/* How far ahead the next exit is, or -1 when there is none in front.
 *
 * Jumping over an exit is how level 1 used to be overrun: the player has to
 * be on the ground and standing on it for the interact to fire. */
static float Exit_Ahead( float look )
{
	if( !pActive_Camera || !pActive_Camera->m_sprite_manager ) return -1.0f;

	const float px = pLevel_Player->m_pos_x;

	for( cSprite_List::iterator itr = pActive_Camera->m_sprite_manager->objects.begin();
	     itr != pActive_Camera->m_sprite_manager->objects.end(); ++itr )
	{
		cSprite *obj = (*itr);
		if( !obj || obj->m_type != TYPE_LEVEL_EXIT ) continue;

		const float d = obj->m_pos_x - px;
		if( d > 0.0f && d < look ) return d;
	}

	return -1.0f;
}

/* Any enemy close enough ahead to be dangerous? */
static bool Enemy_Ahead( float look )
{
	if( !pActive_Camera || !pActive_Camera->m_sprite_manager ) return false;

	const float px = pLevel_Player->m_pos_x;

	for( cSprite_List::iterator itr = pActive_Camera->m_sprite_manager->objects.begin();
	     itr != pActive_Camera->m_sprite_manager->objects.end(); ++itr )
	{
		cSprite *obj = (*itr);
		if( !obj || obj->m_sprite_array != ARRAY_ENEMY ) continue;

		const float dx = obj->m_pos_x - px;
		if( dx > 0.0f && dx < look ) return true;
	}

	return false;
}

void Autoplay_Update( void )
{
	if( !s_enabled || !pTouchControls ) return;

	// Game time, not the machine's. Under the offline-render speed factor a
	// frame takes ~100 ms of wall clock but advances 1/60 s of the world; a
	// jump scheduled in real milliseconds would last one frame instead of
	// seven, and the pilot would walk into every pit.
	const Uint32 now = pFramerate->m_game_ticks;

	// On the overworld: press the pad's jump zone, which is what enters the
	// level the player stands on.
	if( Game_Mode == MODE_OVERWORLD )
	{
		static Uint32 next_enter = 0;
		if( now > next_enter )
		{
			pTouchControls->Autoplay_Tap( ZONE_BTN_JUMP, 120 );
			next_enter = now + 1500;
		}
		return;
	}

	if( Game_Mode != MODE_LEVEL || !pLevel_Player ) return;

	// The run is about proving the level can be traversed with the pad, not
	// about the pilot's survival skills: small Maryo dies to a single touch,
	// so a scripted run spends its lives long before the exit. God mode is
	// the engine's own debug switch and changes nothing about the geometry —
	// every wall, gap and pipe still has to be cleared by pressing the pad.
	// It is only ever set while the autoplay flag file exists.
	if( s_god && !pLevel_Player->m_god_mode )
	{
		pLevel_Player->m_god_mode = true;
		SDL_Log( "SMCTEST AUTOPLAY god_mode on (traversal run)" );
	}

	// Stuck detector: a wall that needs a run-up leaves the player pressed
	// against it with no progress at all. Back off, then charge it.
	const float x_now = pLevel_Player->m_pos_x;

	if( x_now > s_last_x + 6.0f || x_now < s_last_x - 6.0f )
	{
		s_last_x = x_now;
		s_stuck_since = now;
	}
	else if( s_stuck_since == 0 )
	{
		s_stuck_since = now;
	}

	if( s_back_until > now )
	{
		pTouchControls->Autoplay_Hold( ZONE_DPAD_RIGHT, false );
		pTouchControls->Autoplay_Hold( ZONE_BTN_JUMP, false );
		pTouchControls->Autoplay_Hold( ZONE_DPAD_LEFT, true );
		return;
	}

	pTouchControls->Autoplay_Hold( ZONE_DPAD_LEFT, false );

	if( now - s_stuck_since > 2500 )
	{
		s_back_until = now + 450;
		s_stuck_since = now;
		s_jump_until = 0;
		SDL_Log( "SMCTEST AUTOPLAY stuck at x=%.0f, backing up", x_now );
		return;
	}

	// On an exit: press UP, which is what actually ends the level. Without
	// this the pilot simply runs past the end of the level and keeps going.
	const int exit_zone = Level_Exit_Zone();

	if( exit_zone >= 0 )
	{
		pTouchControls->Autoplay_Hold( ZONE_DPAD_RIGHT, false );
		pTouchControls->Autoplay_Hold( ZONE_BTN_JUMP, false );
		pTouchControls->Autoplay_Hold( exit_zone, true );
		s_stuck_since = now;      // standing still here is the point, not a jam
		return;
	}

	pTouchControls->Autoplay_Hold( ZONE_DPAD_UP, false );
	pTouchControls->Autoplay_Hold( ZONE_DPAD_DOWN, false );

	// Always run right.
	pTouchControls->Autoplay_Hold( ZONE_DPAD_RIGHT, true );

	const bool grounded = ( pLevel_Player->m_ground_object != NULL );

	// Release the jump zone as soon as the hold is over, whether or not the
	// player is on the ground. Leaving it pressed kept the key down forever,
	// and the engine will not start a second jump until it has been released
	// — the player climbed onto the step and then stood there.
	if( now > s_jump_until )
	{
		pTouchControls->Autoplay_Hold( ZONE_BTN_JUMP, false );
	}

	// Never leave the ground right before an exit: the interact only fires
	// while standing on it, so a jump sails straight over the end of the
	// level. Level 1 was overrun exactly this way, at 8290.
	const float exit_dist = Exit_Ahead( 120.0f );

	if( grounded && exit_dist >= 0.0f && now > s_jump_until + 90 )
	{
		pTouchControls->Autoplay_Hold( ZONE_BTN_JUMP, false );
	}
	else if( grounded && now > s_jump_until + 90 )
	{
		const float speed = ( pLevel_Player->m_velx > 1.0f ) ? pLevel_Player->m_velx : 1.0f;

		// A wall has to be taken early and held long: height grows with how
		// long the jump key stays down. An enemy is the opposite — jump late
		// and briefly, so the player comes down on its head instead of
		// sailing over and landing in front of the next one.
		const float wall_look = 45.0f + speed * 7.0f;
		const float rise = Obstacle_Height( wall_look );

		if( rise > 20.0f )
		{
			Uint32 hold = 380 + (Uint32)( rise * 4.5f );
			if( hold > 900 ) hold = 900;
			pTouchControls->Autoplay_Hold( ZONE_BTN_JUMP, true );
			s_jump_until = now + hold;
		}
		else if( Gap_Ahead( wall_look + 30.0f ) )
		{
			pTouchControls->Autoplay_Hold( ZONE_BTN_JUMP, true );
			s_jump_until = now + 700;
		}
		else if( Enemy_Ahead( 45.0f + speed * 5.0f ) )
		{
			pTouchControls->Autoplay_Hold( ZONE_BTN_JUMP, true );
			s_jump_until = now + 300;
		}
	}
}

} // namespace SMC

#else   // !__ANDROID__

namespace SMC
{
void Autoplay_Init( void ) {}
bool Autoplay_Enabled( void ) { return false; }
void Autoplay_Update( void ) {}
}

#endif
