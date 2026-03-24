/***************************************************************************
 * touch_controls.cpp  -  On-screen touch controls for mobile/tablet
 ***************************************************************************/
/*
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "../input/touch_controls.h"
#include "../input/keyboard.h"
#include "../core/game_core.h"
#include "../user/preferences.h"
#include "../core/debug_log.h"
#include <GL/glew.h>
#include <cmath>

namespace SMC
{

cTouchControls *pTouchControls = NULL;

cTouchControls :: cTouchControls( void )
{
	m_enabled = false;
	m_visible = false;
	m_opacity = 0.4f;
	m_screen_w = 1024.0f;
	m_screen_h = 768.0f;

	for( int i = 0; i < ZONE_COUNT; i++ )
	{
		m_zones[i].pressed = false;
		m_zones[i].finger_id = -1;
		m_zones[i].active = false;
		m_zones[i].mapped_key = SDLK_UNKNOWN;
	}
}

cTouchControls :: ~cTouchControls( void )
{
	Reset();
}

void cTouchControls :: Init( void )
{
	int num_touch = SDL_GetNumTouchDevices();
	LOG_DEBUG(INPUT, "Touch devices detected: %d", num_touch);

	// On mobile platforms, always enable touch controls.
	// On desktop, only enable if a touch device is actually present.
#if defined(__ANDROID__) || defined(__IPHONEOS__)
	m_enabled = true;
	m_visible = true;
#else
	m_enabled = ( num_touch > 0 );
	m_visible = ( num_touch > 0 );
#endif

	// Disable synthetic mouse events from touches — we inject keyboard events
	// directly via SDL_PushEvent, so synthetic mouse events just leak to CEGUI
	// and cause phantom menu clicks (the "mushroom selector toggle" bug).
	SDL_SetHint( SDL_HINT_TOUCH_MOUSE_EVENTS, "0" );
	// Also prevent mouse clicks from generating synthetic finger events,
	// which would cause double-handling through both mouse and finger paths.
	SDL_SetHint( SDL_HINT_MOUSE_TOUCH_EVENTS, "0" );

	m_screen_w = static_cast<float>( pPreferences->m_video_screen_w );
	m_screen_h = static_cast<float>( pPreferences->m_video_screen_h );

	Init_Zones();
	LOG_INIT("Touch controls initialized: enabled=%d visible=%d screen=%.0fx%.0f",
		m_enabled, m_visible, m_screen_w, m_screen_h);
}

void cTouchControls :: Init_Zones( void )
{
	float sw = m_screen_w;
	float sh = m_screen_h;

	// ===== D-PAD (bottom-left) =====
	// Big buttons, easy to hit. ~18% of screen height each.
	float bs = sh * 0.18f;   // button size
	float pad = sh * 0.01f;  // gap between buttons
	float margin = sw * 0.02f;

	// D-pad center point
	float dcx = margin + bs + pad;
	float dcy = sh - margin - bs - pad;

	// LEFT: left of center
	m_zones[ZONE_DPAD_LEFT].x = margin;
	m_zones[ZONE_DPAD_LEFT].y = dcy - bs * 0.5f;
	m_zones[ZONE_DPAD_LEFT].w = bs;
	m_zones[ZONE_DPAD_LEFT].h = bs;
	m_zones[ZONE_DPAD_LEFT].mapped_key = pPreferences->m_key_left;

	// RIGHT: right of center
	m_zones[ZONE_DPAD_RIGHT].x = dcx + pad;
	m_zones[ZONE_DPAD_RIGHT].y = dcy - bs * 0.5f;
	m_zones[ZONE_DPAD_RIGHT].w = bs;
	m_zones[ZONE_DPAD_RIGHT].h = bs;
	m_zones[ZONE_DPAD_RIGHT].mapped_key = pPreferences->m_key_right;

	// UP: above center
	m_zones[ZONE_DPAD_UP].x = dcx - bs * 0.5f;
	m_zones[ZONE_DPAD_UP].y = dcy - bs * 0.5f - pad - bs;
	m_zones[ZONE_DPAD_UP].w = bs;
	m_zones[ZONE_DPAD_UP].h = bs;
	m_zones[ZONE_DPAD_UP].mapped_key = pPreferences->m_key_up;

	// DOWN: below center
	m_zones[ZONE_DPAD_DOWN].x = dcx - bs * 0.5f;
	m_zones[ZONE_DPAD_DOWN].y = dcy + bs * 0.5f + pad;
	m_zones[ZONE_DPAD_DOWN].w = bs;
	m_zones[ZONE_DPAD_DOWN].h = bs;
	m_zones[ZONE_DPAD_DOWN].mapped_key = pPreferences->m_key_down;

	// ===== ACTION BUTTONS (bottom-right) =====
	float abtn = bs * 1.1f;  // action button size (slightly bigger)
	float agap = abtn * 0.15f;
	float arx = sw - margin - abtn;  // right edge
	float aby = sh - margin - abtn;  // bottom edge

	// JUMP (A) - big green, bottom-right corner
	m_zones[ZONE_BTN_JUMP].x = arx;
	m_zones[ZONE_BTN_JUMP].y = aby - abtn * 0.5f - agap;
	m_zones[ZONE_BTN_JUMP].w = abtn;
	m_zones[ZONE_BTN_JUMP].h = abtn;
	m_zones[ZONE_BTN_JUMP].mapped_key = pPreferences->m_key_jump;

	// SHOOT (B) - red, left of jump
	m_zones[ZONE_BTN_SHOOT].x = arx - abtn - agap;
	m_zones[ZONE_BTN_SHOOT].y = aby;
	m_zones[ZONE_BTN_SHOOT].w = abtn * 0.85f;
	m_zones[ZONE_BTN_SHOOT].h = abtn * 0.85f;
	m_zones[ZONE_BTN_SHOOT].mapped_key = pPreferences->m_key_shoot;

	// ACTION (X) - blue, above shoot
	m_zones[ZONE_BTN_ACTION].x = arx - abtn - agap;
	m_zones[ZONE_BTN_ACTION].y = aby - abtn - agap;
	m_zones[ZONE_BTN_ACTION].w = abtn * 0.85f;
	m_zones[ZONE_BTN_ACTION].h = abtn * 0.85f;
	m_zones[ZONE_BTN_ACTION].mapped_key = pPreferences->m_key_action;

	// ===== SYSTEM BUTTONS (top) =====
	float sysw = sw * 0.10f;
	float sysh = sh * 0.07f;

	// BACK (top-left) - Escape
	m_zones[ZONE_BTN_BACK].x = margin;
	m_zones[ZONE_BTN_BACK].y = margin;
	m_zones[ZONE_BTN_BACK].w = sysw;
	m_zones[ZONE_BTN_BACK].h = sysh;
	m_zones[ZONE_BTN_BACK].mapped_key = SDLK_ESCAPE;

	// START (top-right) - Escape/Pause
	m_zones[ZONE_BTN_START].x = sw - margin - sysw;
	m_zones[ZONE_BTN_START].y = margin;
	m_zones[ZONE_BTN_START].w = sysw;
	m_zones[ZONE_BTN_START].h = sysh;
	m_zones[ZONE_BTN_START].mapped_key = SDLK_ESCAPE;

	// ENTER - for menu selection (center-right, big)
	m_zones[ZONE_BTN_ENTER].x = arx;
	m_zones[ZONE_BTN_ENTER].y = aby;
	m_zones[ZONE_BTN_ENTER].w = abtn;
	m_zones[ZONE_BTN_ENTER].h = abtn;
	m_zones[ZONE_BTN_ENTER].mapped_key = SDLK_RETURN;

	for( int i = 0; i < ZONE_COUNT; i++ )
	{
		m_zones[i].pressed = false;
		m_zones[i].finger_id = -1;
		m_zones[i].active = true;
	}

	Update_Zone_Visibility();
}

void cTouchControls :: Update_Zone_Visibility( void )
{
	if( editor_enabled )
	{
		for( int i = 0; i < ZONE_COUNT; i++ )
			m_zones[i].active = false;
		m_visible = false;
		return;
	}

	m_visible = m_enabled;

	if( Game_Mode == MODE_MENU )
	{
		// Menu: D-pad up/down for navigation, ENTER to confirm, BACK to go back
		m_zones[ZONE_DPAD_LEFT].active  = false;
		m_zones[ZONE_DPAD_RIGHT].active = false;
		m_zones[ZONE_DPAD_UP].active    = true;
		m_zones[ZONE_DPAD_DOWN].active  = true;
		m_zones[ZONE_BTN_JUMP].active   = false;
		m_zones[ZONE_BTN_SHOOT].active  = false;
		m_zones[ZONE_BTN_ACTION].active = false;
		m_zones[ZONE_BTN_START].active  = false;
		m_zones[ZONE_BTN_BACK].active   = true;
		m_zones[ZONE_BTN_ENTER].active  = true;
	}
	else
	{
		// Gameplay: D-pad, action buttons, system buttons — hide ENTER
		m_zones[ZONE_DPAD_LEFT].active  = true;
		m_zones[ZONE_DPAD_RIGHT].active = true;
		m_zones[ZONE_DPAD_UP].active    = true;
		m_zones[ZONE_DPAD_DOWN].active  = true;
		m_zones[ZONE_BTN_JUMP].active   = true;
		m_zones[ZONE_BTN_SHOOT].active  = true;
		m_zones[ZONE_BTN_ACTION].active = true;
		m_zones[ZONE_BTN_START].active  = true;
		m_zones[ZONE_BTN_BACK].active   = true;
		m_zones[ZONE_BTN_ENTER].active  = false;
	}
}

void cTouchControls :: Reset( void )
{
	for( int i = 0; i < ZONE_COUNT; i++ )
	{
		if( m_zones[i].pressed )
			Release_Zone( i );
	}
}

int cTouchControls :: Zone_Hit_Test( float screen_x, float screen_y )
{
	// Check in reverse order so top-drawn buttons have priority
	for( int i = ZONE_COUNT - 1; i >= 0; i-- )
	{
		if( !m_zones[i].active ) continue;
		if( screen_x >= m_zones[i].x && screen_x <= m_zones[i].x + m_zones[i].w &&
		    screen_y >= m_zones[i].y && screen_y <= m_zones[i].y + m_zones[i].h )
		{
			return i;
		}
	}
	return -1;
}

// Inject a real SDL keyboard event into the event queue
static void Inject_SDL_Key( SDLKey keycode, bool down )
{
	SDL_Event ev;
	memset( &ev, 0, sizeof(ev) );
	ev.type = down ? SDL_KEYDOWN : SDL_KEYUP;
	ev.key.state = down ? SDL_PRESSED : SDL_RELEASED;
	ev.key.keysym.sym = keycode;
	ev.key.keysym.scancode = SDL_GetScancodeFromKey( keycode );
	ev.key.keysym.mod = SDL_GetModState();
	ev.key.timestamp = SDL_GetTicks();
	SDL_PushEvent( &ev );
}

void cTouchControls :: Press_Zone( int zone_id, SDL_FingerID finger )
{
	if( zone_id < 0 || zone_id >= ZONE_COUNT ) return;
	if( m_zones[zone_id].pressed ) return;

	m_zones[zone_id].pressed = true;
	m_zones[zone_id].finger_id = finger;

	SDLKey key = m_zones[zone_id].mapped_key;
	LOG_DEBUG(INPUT, "Touch PRESS zone=%d key=%s(%d)", zone_id, SDL_GetKeyName(key), key);

	// Inject real SDL key event — goes through normal Handle_Input_Global
	Inject_SDL_Key( key, true );
}

void cTouchControls :: Release_Zone( int zone_id )
{
	if( zone_id < 0 || zone_id >= ZONE_COUNT ) return;
	if( !m_zones[zone_id].pressed ) return;

	m_zones[zone_id].pressed = false;
	m_zones[zone_id].finger_id = -1;

	SDLKey key = m_zones[zone_id].mapped_key;
	LOG_DEBUG(INPUT, "Touch RELEASE zone=%d key=%s(%d)", zone_id, SDL_GetKeyName(key), key);

	// Inject real SDL key release event
	Inject_SDL_Key( key, false );
}

void cTouchControls :: Release_All_For_Finger( SDL_FingerID finger )
{
	for( int i = 0; i < ZONE_COUNT; i++ )
	{
		if( m_zones[i].pressed && m_zones[i].finger_id == finger )
			Release_Zone( i );
	}
}

bool cTouchControls :: Handle_Finger_Down( SDL_Event *ev )
{
	if( !m_enabled ) return false;
	float sx = ev->tfinger.x * m_screen_w;
	float sy = ev->tfinger.y * m_screen_h;
	SDL_FingerID fid = ev->tfinger.fingerId;

	int zone = Zone_Hit_Test( sx, sy );
	if( zone >= 0 )
	{
		Press_Zone( zone, fid );
		return true;
	}
	return false;
}

bool cTouchControls :: Handle_Finger_Up( SDL_Event *ev )
{
	if( !m_enabled ) return false;

	bool had_press = false;
	for( int i = 0; i < ZONE_COUNT; i++ )
	{
		if( m_zones[i].pressed && m_zones[i].finger_id == ev->tfinger.fingerId )
		{
			had_press = true;
			break;
		}
	}
	Release_All_For_Finger( ev->tfinger.fingerId );
	return had_press;
}

bool cTouchControls :: Handle_Finger_Motion( SDL_Event *ev )
{
	if( !m_enabled ) return false;
	float sx = ev->tfinger.x * m_screen_w;
	float sy = ev->tfinger.y * m_screen_h;
	SDL_FingerID fid = ev->tfinger.fingerId;

	int new_zone = Zone_Hit_Test( sx, sy );

	for( int i = 0; i < ZONE_COUNT; i++ )
	{
		if( m_zones[i].pressed && m_zones[i].finger_id == fid && i != new_zone )
			Release_Zone( i );
	}
	if( new_zone >= 0 && !m_zones[new_zone].pressed )
		Press_Zone( new_zone, fid );

	return true;
}

// Mouse-as-touch support (WSL / Surface / desktop testing)
static const SDL_FingerID MOUSE_FINGER_ID = 999999;

bool cTouchControls :: Handle_Mouse_Down( SDL_Event *ev )
{
	if( !m_enabled || !m_visible ) return false;
	if( ev->button.button != SDL_BUTTON_LEFT ) return false;

	int win_w = 1, win_h = 1;
	if( g_sdl_window ) SDL_GetWindowSize( g_sdl_window, &win_w, &win_h );
	float sx = static_cast<float>( ev->button.x ) * m_screen_w / static_cast<float>( win_w );
	float sy = static_cast<float>( ev->button.y ) * m_screen_h / static_cast<float>( win_h );

	int zone = Zone_Hit_Test( sx, sy );
	if( zone >= 0 )
	{
		Press_Zone( zone, MOUSE_FINGER_ID );
		return true;
	}
	return false;
}

bool cTouchControls :: Handle_Mouse_Up( SDL_Event *ev )
{
	if( !m_enabled || !m_visible ) return false;
	if( ev->button.button != SDL_BUTTON_LEFT ) return false;

	// Only consume if we had a pressed zone
	bool had_press = false;
	for( int i = 0; i < ZONE_COUNT; i++ )
	{
		if( m_zones[i].pressed && m_zones[i].finger_id == MOUSE_FINGER_ID )
		{
			had_press = true;
			break;
		}
	}
	if( had_press )
	{
		Release_All_For_Finger( MOUSE_FINGER_ID );
		return true;
	}
	return false;
}

bool cTouchControls :: Handle_Mouse_Motion( SDL_Event *ev )
{
	if( !m_enabled || !m_visible ) return false;
	if( !(ev->motion.state & SDL_BUTTON_LMASK) ) return false;

	int win_w = 1, win_h = 1;
	if( g_sdl_window ) SDL_GetWindowSize( g_sdl_window, &win_w, &win_h );
	float sx = static_cast<float>( ev->motion.x ) * m_screen_w / static_cast<float>( win_w );
	float sy = static_cast<float>( ev->motion.y ) * m_screen_h / static_cast<float>( win_h );

	int new_zone = Zone_Hit_Test( sx, sy );

	for( int i = 0; i < ZONE_COUNT; i++ )
	{
		if( m_zones[i].pressed && m_zones[i].finger_id == MOUSE_FINGER_ID && i != new_zone )
			Release_Zone( i );
	}
	if( new_zone >= 0 && !m_zones[new_zone].pressed )
		Press_Zone( new_zone, MOUSE_FINGER_ID );

	return true;
}

void cTouchControls :: Update( void )
{
	if( !m_enabled ) return;

	m_screen_w = static_cast<float>( pPreferences->m_video_screen_w );
	m_screen_h = static_cast<float>( pPreferences->m_video_screen_h );

	Update_Zone_Visibility();

	// Re-enforce key state for held zones (safety net for continuous input)
	for( int i = 0; i < ZONE_COUNT; i++ )
	{
		if( m_zones[i].pressed && m_zones[i].active )
		{
			SDL_Scancode sc = SDL_GetScancodeFromKey( m_zones[i].mapped_key );
			if( sc >= 0 && sc < SDL_NUM_SCANCODES )
				pKeyboard->m_keys[sc] = 1;
		}
	}
}

/* *** *** *** Drawing *** *** *** */

void cTouchControls :: Draw( void )
{
	if( !m_visible ) return;

	glPushAttrib( GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_CURRENT_BIT );
	glDisable( GL_TEXTURE_2D );
	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	glDisable( GL_DEPTH_TEST );

	glMatrixMode( GL_PROJECTION );
	glPushMatrix();
	glLoadIdentity();
	glOrtho( 0, m_screen_w, m_screen_h, 0, -1, 1 );
	glMatrixMode( GL_MODELVIEW );
	glPushMatrix();
	glLoadIdentity();

	Uint8 base_a = static_cast<Uint8>( m_opacity * 255.0f );
	Uint8 press_a = base_a * 2 > 240 ? 240 : base_a * 2;

	// ---- D-PAD ----
	for( int i = ZONE_DPAD_LEFT; i <= ZONE_DPAD_DOWN; i++ )
	{
		if( !m_zones[i].active ) continue;
		Uint8 a = m_zones[i].pressed ? press_a : base_a;
		Draw_Rounded_Rect( m_zones[i].x, m_zones[i].y, m_zones[i].w, m_zones[i].h,
			180, 180, 180, a );

		float cx = m_zones[i].x + m_zones[i].w * 0.5f;
		float cy = m_zones[i].y + m_zones[i].h * 0.5f;
		float as = m_zones[i].w * 0.25f;
		Uint8 aa = m_zones[i].pressed ? 255 : 160;

		if( i == ZONE_DPAD_LEFT )
			Draw_Triangle( cx + as, cy - as, cx + as, cy + as, cx - as, cy, 40, 40, 40, aa );
		else if( i == ZONE_DPAD_RIGHT )
			Draw_Triangle( cx - as, cy - as, cx - as, cy + as, cx + as, cy, 40, 40, 40, aa );
		else if( i == ZONE_DPAD_UP )
			Draw_Triangle( cx - as, cy + as, cx + as, cy + as, cx, cy - as, 40, 40, 40, aa );
		else if( i == ZONE_DPAD_DOWN )
			Draw_Triangle( cx - as, cy - as, cx + as, cy - as, cx, cy + as, 40, 40, 40, aa );
	}

	// ---- ACTION BUTTONS ----
	struct { int id; Uint8 r, g, b; } buttons[] = {
		{ ZONE_BTN_JUMP,   40, 200,  40 },  // green
		{ ZONE_BTN_SHOOT, 200,  50,  50 },  // red
		{ ZONE_BTN_ACTION, 50, 100, 220 },  // blue
		{ ZONE_BTN_ENTER, 220, 200,  40 },  // yellow
	};

	for( int b = 0; b < 4; b++ )
	{
		int id = buttons[b].id;
		if( !m_zones[id].active ) continue;
		Uint8 a = m_zones[id].pressed ? press_a : base_a;
		float r = m_zones[id].w * 0.5f;
		float cx = m_zones[id].x + r;
		float cy = m_zones[id].y + m_zones[id].h * 0.5f;
		Draw_Circle( cx, cy, r, buttons[b].r, buttons[b].g, buttons[b].b, a );

		// Label indicator
		Uint8 la = m_zones[id].pressed ? 255 : 180;
		float ls = r * 0.35f;
		if( id == ZONE_BTN_JUMP )
			Draw_Triangle( cx, cy - ls, cx - ls, cy + ls * 0.7f, cx + ls, cy + ls * 0.7f, 255, 255, 255, la );
		else if( id == ZONE_BTN_ENTER )
			Draw_Triangle( cx - ls, cy - ls, cx - ls, cy + ls, cx + ls, cy, 255, 255, 255, la );
	}

	// ---- SYSTEM BUTTONS ----
	int sysbtns[] = { ZONE_BTN_BACK, ZONE_BTN_START };
	for( int s = 0; s < 2; s++ )
	{
		int id = sysbtns[s];
		if( !m_zones[id].active ) continue;
		Uint8 a = m_zones[id].pressed ? press_a : base_a;
		Draw_Rounded_Rect( m_zones[id].x, m_zones[id].y, m_zones[id].w, m_zones[id].h,
			80, 80, 80, a );

		float cx = m_zones[id].x + m_zones[id].w * 0.5f;
		float cy = m_zones[id].y + m_zones[id].h * 0.5f;
		float ts = m_zones[id].h * 0.25f;
		Uint8 la = m_zones[id].pressed ? 255 : 180;

		if( id == ZONE_BTN_BACK )
			Draw_Triangle( cx + ts, cy - ts, cx + ts, cy + ts, cx - ts, cy, 220, 220, 220, la );
		else
			Draw_Triangle( cx - ts, cy - ts, cx - ts, cy + ts, cx + ts, cy, 220, 220, 220, la );
	}

	glMatrixMode( GL_MODELVIEW );
	glPopMatrix();
	glMatrixMode( GL_PROJECTION );
	glPopMatrix();
	glMatrixMode( GL_MODELVIEW );
	glPopAttrib();
}

void cTouchControls :: Draw_Circle( float cx, float cy, float radius, Uint8 r, Uint8 g, Uint8 b, Uint8 a )
{
	const int seg = 32;
	glColor4ub( r, g, b, a );
	glBegin( GL_TRIANGLE_FAN );
	glVertex2f( cx, cy );
	for( int i = 0; i <= seg; i++ )
	{
		float ang = 2.0f * 3.14159f * static_cast<float>(i) / static_cast<float>(seg);
		glVertex2f( cx + cosf(ang) * radius, cy + sinf(ang) * radius );
	}
	glEnd();
}

void cTouchControls :: Draw_Rounded_Rect( float x, float y, float w, float h, Uint8 r, Uint8 g, Uint8 b, Uint8 a )
{
	float rad = (w < h ? w : h) * 0.15f;
	const int segs = 6;
	glColor4ub( r, g, b, a );
	glBegin( GL_TRIANGLE_FAN );
	float cx = x + w * 0.5f, cy = y + h * 0.5f;
	glVertex2f( cx, cy );

	float corners[4][2] = {
		{ x + rad, y + rad },
		{ x + w - rad, y + rad },
		{ x + w - rad, y + h - rad },
		{ x + rad, y + h - rad }
	};
	float start_angles[4] = { 3.14159f, 3.14159f * 1.5f, 0.0f, 3.14159f * 0.5f };

	for( int c = 0; c < 4; c++ )
	{
		for( int i = 0; i <= segs; i++ )
		{
			float ang = start_angles[c] + 3.14159f * 0.5f * static_cast<float>(i) / static_cast<float>(segs);
			glVertex2f( corners[c][0] + cosf(ang) * rad, corners[c][1] + sinf(ang) * rad );
		}
	}
	// close
	glVertex2f( x, y + rad );
	glEnd();
}

void cTouchControls :: Draw_Triangle( float x1, float y1, float x2, float y2, float x3, float y3,
	Uint8 r, Uint8 g, Uint8 b, Uint8 a )
{
	glColor4ub( r, g, b, a );
	glBegin( GL_TRIANGLES );
	glVertex2f( x1, y1 );
	glVertex2f( x2, y2 );
	glVertex2f( x3, y3 );
	glEnd();
}

} // namespace SMC
