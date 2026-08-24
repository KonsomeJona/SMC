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

/* Is something solid directly ahead, high enough to need a jump? */
static bool Obstacle_Ahead( float look )
{
	if( !pActive_Camera || !pActive_Camera->m_sprite_manager ) return false;

	const float px = pLevel_Player->m_pos_x;
	const float py = pLevel_Player->m_pos_y;

	for( cSprite_List::iterator itr = pActive_Camera->m_sprite_manager->objects.begin();
	     itr != pActive_Camera->m_sprite_manager->objects.end(); ++itr )
	{
		cSprite *obj = (*itr);
		if( !obj || obj->m_massive_type != MASS_MASSIVE ) continue;

		const float dx = obj->m_pos_x - px;
		if( dx < 5.0f || dx > look ) continue;

		// A surface at or above the player's feet blocks him; one well below
		// is just the floor he is standing on.
		if( obj->m_pos_y < py + 20.0f ) return true;
	}

	return false;
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

	if( grounded && now > s_jump_until )
	{
		// Look ahead proportionally to speed: at full run the player covers a
		// lot of ground before a jump has any effect.
		const float speed = ( pLevel_Player->m_velx > 1.0f ) ? pLevel_Player->m_velx : 1.0f;
		// Start the jump well before the obstacle: at a full run the player
		// covers a lot of ground while the jump is still gaining height, and
		// jumping once already touching a wall only scrapes along it.
		const float look  = 90.0f + speed * 14.0f;

		if( Obstacle_Ahead( look ) || Gap_Ahead( look + 30.0f ) || Enemy_Ahead( look + 40.0f ) )
		{
			// Hold the jump zone: height depends on how long the key stays down.
			pTouchControls->Autoplay_Hold( ZONE_BTN_JUMP, true );
			s_jump_until = now + 700;
		}
	}
	else if( now > s_jump_until )
	{
		pTouchControls->Autoplay_Hold( ZONE_BTN_JUMP, false );
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
