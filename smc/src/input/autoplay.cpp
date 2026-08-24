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
#include "../core/filesystem/filesystem.h"
#include "../core/debug_log.h"
#include <SDL.h>

namespace SMC
{

static bool s_enabled = false;
static Uint32 s_jump_until = 0;   // keep the jump zone held until this tick

void Autoplay_Init( void )
{
	std::string flag = Get_User_Directory() + "autoplay";
	s_enabled = File_Exists( flag );
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

	const Uint32 now = SDL_GetTicks();

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

	if( grounded && now > s_jump_until + 90 )
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
