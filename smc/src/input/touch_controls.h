/***************************************************************************
 * touch_controls.h  -  On-screen touch controls for mobile/tablet
 ***************************************************************************/
/*
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef SMC_TOUCH_CONTROLS_H
#define SMC_TOUCH_CONTROLS_H

#include "../core/global_basic.h"
#include "../core/global_game.h"
#include "core/sdl2_compat.h"

namespace SMC
{

enum TouchZoneID {
	ZONE_DPAD_LEFT = 0,
	ZONE_DPAD_RIGHT,
	ZONE_DPAD_UP,
	ZONE_DPAD_DOWN,
	ZONE_BTN_JUMP,
	ZONE_BTN_SHOOT,
	ZONE_BTN_ACTION,
	ZONE_BTN_START,
	ZONE_BTN_BACK,
	ZONE_BTN_ENTER,
	ZONE_COUNT
};

struct TouchZone {
	float x, y, w, h;       // screen-space rect (in glOrtho coords)
	SDLKey mapped_key;       // key to inject
	bool pressed;            // currently touched
	SDL_FingerID finger_id;  // finger on this zone (-1 = none)
	bool active;             // visible and accepting input
};

class cTouchControls
{
public:
	cTouchControls( void );
	~cTouchControls( void );

	void Init( void );
	bool Handle_Finger_Down( SDL_Event *ev );
	bool Handle_Finger_Up( SDL_Event *ev );
	bool Handle_Finger_Motion( SDL_Event *ev );
	bool Handle_Mouse_Down( SDL_Event *ev );
	bool Handle_Mouse_Up( SDL_Event *ev );
	bool Handle_Mouse_Motion( SDL_Event *ev );
	void Update( void );
	void Draw( void );
	void Reset( void );

	bool m_enabled;
	bool m_visible;

private:
	TouchZone m_zones[ZONE_COUNT];

	void Init_Zones( void );
	void Update_Zone_Visibility( void );
	int Zone_Hit_Test( float screen_x, float screen_y );
	void Press_Zone( int zone_id, SDL_FingerID finger );
	void Release_Zone( int zone_id );
	void Release_All_For_Finger( SDL_FingerID finger );

	void Draw_Circle( float cx, float cy, float radius, Uint8 r, Uint8 g, Uint8 b, Uint8 a );
	void Draw_Rounded_Rect( float x, float y, float w, float h, Uint8 r, Uint8 g, Uint8 b, Uint8 a );
	void Draw_Triangle( float x1, float y1, float x2, float y2, float x3, float y3, Uint8 r, Uint8 g, Uint8 b, Uint8 a );

	// In-universe skin helpers — relief 3D pixel-art (highlight TL + shadow BR + black outline)
	void Draw_Beveled_Plate( float x, float y, float w, float h,
		Uint8 r, Uint8 g, Uint8 b, Uint8 a, bool pressed );
	void Draw_Brick_Tile( float x, float y, float w, float h, Uint8 a, bool pressed );
	void Draw_Itembox_Tile( float x, float y, float w, float h, Uint8 a, bool pressed );
	void Draw_Metal_Plate( float x, float y, float w, float h, Uint8 a, bool pressed );
	void Draw_Wood_Sign( float x, float y, float w, float h, Uint8 a, bool pressed );

	// Glyphs drawn in pixel-art style on top of plates
	void Draw_Arrow_Glyph( float cx, float cy, float size, int dir,
		Uint8 r, Uint8 g, Uint8 b, Uint8 a );  // dir: 0=L 1=R 2=U 3=D
	void Draw_QMark_Glyph( float cx, float cy, float size,
		Uint8 r, Uint8 g, Uint8 b, Uint8 a );
	void Draw_Pause_Glyph( float cx, float cy, float size,
		Uint8 r, Uint8 g, Uint8 b, Uint8 a );

	float m_screen_w;  // current glOrtho width
	float m_screen_h;  // current glOrtho height
	float m_opacity;   // base opacity 0-1
	int m_last_game_mode;  // detect Game_Mode changes → release stale zones
};

// global touch controls pointer
extern cTouchControls *pTouchControls;

} // namespace SMC

#endif
