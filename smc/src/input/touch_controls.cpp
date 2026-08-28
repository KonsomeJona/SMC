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
#include "../core/sdl2_compat.h"
#ifdef __ANDROID__
#include "../video/gles2_renderer.h"
#endif
#include <cmath>

namespace SMC
{

cTouchControls *pTouchControls = NULL;

cTouchControls :: cTouchControls( void )
{
	m_enabled = false;
	m_visible = false;
	// 0.85 rendait les plaques quasi opaques : le decor disparaissait derriere
	// le pad, ce qui gene surtout sur grand ecran ou le pad couvre du jeu.
	m_opacity = 0.45f;
	m_screen_w = 1024.0f;
	m_screen_h = 768.0f;
	m_last_game_mode = -1;

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

	// Enable synthetic mouse events from touches so the ModernUI menus
	// (which listen for SDL_MOUSEBUTTONDOWN / SDL_MOUSEMOTION) receive
	// touch taps as clicks.  In-game, finger events are still handled
	// directly by cTouchControls via SDL_FINGERDOWN, which takes priority.
	SDL_SetHint( SDL_HINT_TOUCH_MOUSE_EVENTS, "1" );
	// Prevent mouse clicks from generating synthetic finger events,
	// which would cause double-handling through both mouse and finger paths.
	SDL_SetHint( SDL_HINT_MOUSE_TOUCH_EVENTS, "0" );

	m_screen_w = static_cast<float>( pPreferences->m_video_screen_w );
	m_screen_h = static_cast<float>( pPreferences->m_video_screen_h );

	Init_Zones();
	SDL_Log( "Touch controls initialized: enabled=%d visible=%d screen=%.0fx%.0f",
		m_enabled, m_visible, m_screen_w, m_screen_h );
}

void cTouchControls :: Init_Zones( void )
{
	float sw = m_screen_w;
	float sh = m_screen_h;

	// Ergonomic layout (Delta emulator-inspired):
	//  - D-pad anchored to bottom-left, sized so the thumb rests naturally
	//    over the center of the cross without strain.
	//  - Action buttons (SHOOT/JUMP) bottom-right, offset diagonally
	//    (SNES style) so JUMP is closer to the corner / lower than SHOOT —
	//    the right thumb naturally curls toward the lower-right; JUMP is
	//    the most-used button so it gets the most comfortable spot.
	//  - Bigger separation between JUMP and SHOOT to avoid accidental
	//    cross-press during a rapid tap.
	//  - Top edge left clear (system gesture / status bar swipe safe zone).

	// ===== D-PAD (bottom-left) =====
	// Each arm ≈ 14% of screen height → cross spans ~43% vertically. That
	// keeps thumb comfort while leaving room for the game viewport above.
	float bs       = sh * 0.14f;   // arm size (one direction button)
	float pad      = sh * 0.004f;  // tiny gap between arms (joints overlap visually)
	// Generous edge margins so pads visibly breathe off the bezel.
	float marginL  = sw * 0.035f;
	float marginB  = sh * 0.07f;   // lift off the bottom edge for gesture safety

	// D-pad cross is 3 arms wide / 3 arms tall.
	float dpad_left = marginL;
	float dpad_top  = sh - marginB - bs * 3.0f - pad * 2.0f;

	float dcx = dpad_left + bs + pad;        // center column X
	float dcy = dpad_top  + bs + pad;        // center row Y

	// LEFT
	m_zones[ZONE_DPAD_LEFT].x = dpad_left;
	m_zones[ZONE_DPAD_LEFT].y = dcy;
	m_zones[ZONE_DPAD_LEFT].w = bs;
	m_zones[ZONE_DPAD_LEFT].h = bs;
	m_zones[ZONE_DPAD_LEFT].mapped_key = pPreferences->m_key_left;

	// RIGHT
	m_zones[ZONE_DPAD_RIGHT].x = dcx + bs + pad;
	m_zones[ZONE_DPAD_RIGHT].y = dcy;
	m_zones[ZONE_DPAD_RIGHT].w = bs;
	m_zones[ZONE_DPAD_RIGHT].h = bs;
	m_zones[ZONE_DPAD_RIGHT].mapped_key = pPreferences->m_key_right;

	// UP
	m_zones[ZONE_DPAD_UP].x = dcx;
	m_zones[ZONE_DPAD_UP].y = dpad_top;
	m_zones[ZONE_DPAD_UP].w = bs;
	m_zones[ZONE_DPAD_UP].h = bs;
	m_zones[ZONE_DPAD_UP].mapped_key = pPreferences->m_key_up;

	// DOWN
	m_zones[ZONE_DPAD_DOWN].x = dcx;
	m_zones[ZONE_DPAD_DOWN].y = dcy + bs + pad;
	m_zones[ZONE_DPAD_DOWN].w = bs;
	m_zones[ZONE_DPAD_DOWN].h = bs;
	m_zones[ZONE_DPAD_DOWN].mapped_key = pPreferences->m_key_down;

	// ===== ACTION BUTTONS (bottom-right) — SHOOT (?) + JUMP (brick) =====
	// Horizontal alignment with a wide gap (60% of button size) — the
	// previous tight diagonal made the two buttons read as a single blob.
	float abtn     = bs * 1.20f;     // slightly bigger than a D-pad arm
	float agap     = abtn * 0.60f;
	// Use a bigger right margin so the JUMP plate visibly clears the
	// bezel / cutout area on Pixel-class devices. 8% pushes the brick
	// well inside the safe rendering region.
	float marginR  = sw * 0.08f;
	float marginAB = sh * 0.07f;

	// JUMP: bottom-right corner (the natural thumb rest)
	float jx = sw - marginR - abtn;
	float jy = sh - marginAB - abtn;
	m_zones[ZONE_BTN_JUMP].x = jx;
	m_zones[ZONE_BTN_JUMP].y = jy;
	m_zones[ZONE_BTN_JUMP].w = abtn;
	m_zones[ZONE_BTN_JUMP].h = abtn;
	m_zones[ZONE_BTN_JUMP].mapped_key = pPreferences->m_key_jump;

	// SHOOT: same row as JUMP, separated by a wide gap
	m_zones[ZONE_BTN_SHOOT].x = jx - abtn - agap;
	m_zones[ZONE_BTN_SHOOT].y = jy;
	m_zones[ZONE_BTN_SHOOT].w = abtn;
	m_zones[ZONE_BTN_SHOOT].h = abtn;
	m_zones[ZONE_BTN_SHOOT].mapped_key = pPreferences->m_key_shoot;

	// ACTION — disabled (user asked for 2 action buttons only)
	m_zones[ZONE_BTN_ACTION].x = 0;
	m_zones[ZONE_BTN_ACTION].y = 0;
	m_zones[ZONE_BTN_ACTION].w = 0;
	m_zones[ZONE_BTN_ACTION].h = 0;
	m_zones[ZONE_BTN_ACTION].mapped_key = pPreferences->m_key_action;

	// ===== SYSTEM BUTTONS (bottom — top is gesture/cutout area) =====
	// PAUSE (square, top-right) — Escape. Android reserves the LEFT edge
	// for the back-gesture and the TOP edge for the status-bar swipe, so a
	// pause button glued to the top-left corner is very hard to hit
	// (taps in those bands are consumed before they reach the app).
	// The right-side band is much safer because that gesture is not
	// system-reserved on Pixel landscape.
	// Bigger square + generous tap-area so the user never has to be pixel-
	// perfect: visible icon is sysSize, but we extend the hit-test rect a
	// bit further toward the corner so taps in the corner area also count.
	float sysSize  = sh * 0.14f;   // square button, 14% of screen height (visible)
	// PAUSE plate top-right. The rendering shifts ~130px upward relative
	// to projection coords on Pixel-class devices (compositor cutout
	// math we don't query), so the visible plate ends up well below the
	// status bar even though z.y looks far down. Edge-snap in
	// Zone_Hit_Test extends the tap area all the way to the top of the
	// screen so taps on the visible plate register.
	float sysTop   = sh * 0.25f;
	float sysLeft  = sw - marginR - sysSize;
	m_zones[ZONE_BTN_BACK].x = sysLeft - sysSize * 0.30f;
	m_zones[ZONE_BTN_BACK].y = sysTop;
	m_zones[ZONE_BTN_BACK].w = sysSize * 1.30f + marginR;
	m_zones[ZONE_BTN_BACK].h = sysSize * 1.30f;
	m_zones[ZONE_BTN_BACK].mapped_key = SDLK_ESCAPE;

	// START (disabled — replaced by single PAUSE in top-left)
	m_zones[ZONE_BTN_START].x = 0;
	m_zones[ZONE_BTN_START].y = 0;
	m_zones[ZONE_BTN_START].w = 0;
	m_zones[ZONE_BTN_START].h = 0;
	m_zones[ZONE_BTN_START].mapped_key = SDLK_ESCAPE;

	// ENTER — menu validate (reuses JUMP slot position)
	m_zones[ZONE_BTN_ENTER].x = jx;
	m_zones[ZONE_BTN_ENTER].y = jy;
	m_zones[ZONE_BTN_ENTER].w = abtn;
	m_zones[ZONE_BTN_ENTER].h = abtn;
	m_zones[ZONE_BTN_ENTER].mapped_key = SDLK_RETURN;

	for( int i = 0; i < ZONE_COUNT; i++ )
	{
		m_zones[i].pressed = false;
		m_zones[i].finger_id = -1;
		// Start inactive. Init_Zones() runs before Game_Mode settles on
		// MODE_MENU, and defaulting to true made the whole pad flash for one
		// frame at startup before Update_Zone_Visibility() switched it off.
		m_zones[i].active = false;
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
		// Menu: ALL touch zones off so taps fall through to the menu's
		// own mouse handler. The pause/BACK zone in particular has a
		// generous gameplay-tap area that would eat clicks on the right
		// side of the panel (tabs, Back button, etc.) once the user
		// enters the Options screen. Inside menus there is already a
		// "Back" button rendered by ModernUI, plus the Android system
		// back gesture, so a touch-overlay pause is redundant here.
		m_zones[ZONE_DPAD_LEFT].active  = false;
		m_zones[ZONE_DPAD_RIGHT].active = false;
		m_zones[ZONE_DPAD_UP].active    = false;
		m_zones[ZONE_DPAD_DOWN].active  = false;
		m_zones[ZONE_BTN_JUMP].active   = false;
		m_zones[ZONE_BTN_SHOOT].active  = false;
		m_zones[ZONE_BTN_ACTION].active = false;
		m_zones[ZONE_BTN_START].active  = false;
		m_zones[ZONE_BTN_BACK].active   = false;
		m_zones[ZONE_BTN_ENTER].active  = false;
	}
	else
	{
		// Gameplay: 4-way D-pad, JUMP + SHOOT action buttons, single Pause
		// (BACK on the left). The right-side START button was redundant
		// (both mapped to ESC) and the arrow icon was confusing.
		m_zones[ZONE_DPAD_LEFT].active  = true;
		m_zones[ZONE_DPAD_RIGHT].active = true;
		m_zones[ZONE_DPAD_UP].active    = true;
		m_zones[ZONE_DPAD_DOWN].active  = true;
		m_zones[ZONE_BTN_JUMP].active   = true;
		m_zones[ZONE_BTN_SHOOT].active  = true;
		m_zones[ZONE_BTN_ACTION].active = false;
		m_zones[ZONE_BTN_START].active  = false;
		m_zones[ZONE_BTN_BACK].active   = true;
		m_zones[ZONE_BTN_ENTER].active  = false;
	}
}

void cTouchControls :: Autoplay_Hold( int zone_id, bool down )
{
	if( zone_id < 0 || zone_id >= ZONE_COUNT ) return;
	if( !m_zones[zone_id].active ) return;

	if( down )
	{
		if( !m_zones[zone_id].pressed ) Press_Zone( zone_id, -100 - zone_id );
	}
	else if( m_zones[zone_id].pressed )
	{
		Release_Zone( zone_id );
	}
}

void cTouchControls :: Autoplay_Tap( int zone_id, Uint32 /* ms */ )
{
	Autoplay_Hold( zone_id, true );
	Autoplay_Hold( zone_id, false );
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
	// Edge-snapping hit-test: a zone whose actual edge sits within ~1×
	// its own size from a screen edge has its hit area extended to that
	// edge. The Android compositor renders the GL surface with a small
	// device-specific X/Y offset (cutout, safe area) we can't easily
	// query, so visible plates drift away from the projection coords
	// used here. Snapping the hit-test to the screen edges absorbs that
	// drift on edge-anchored buttons (D-pad outer arms, action pair,
	// PAUSE) without affecting non-edge buttons.
	for( int i = ZONE_COUNT - 1; i >= 0; i-- )
	{
		if( !m_zones[i].active ) continue;
		const float zx = m_zones[i].x;
		const float zy = m_zones[i].y;
		const float zw = m_zones[i].w;
		const float zh = m_zones[i].h;

		// Base slop: 20% of the zone size in each direction.
		float hx = zx - zw * 0.20f;
		float hy = zy - zh * 0.20f;
		float hw = zw * 1.40f;
		float hh = zh * 1.40f;

		// Edge-snap when the zone sits anywhere in the outer 40% of
		// the screen on a given axis. This is wide enough to catch the
		// PAUSE plate's offset-corrected position near the top while
		// still keeping the D-pad arms from snapping toward each other
		// horizontally (their column anchors are around 10% / 17% of
		// the screen width).
		const float snap_x = m_screen_w * 0.40f;
		const float snap_y = m_screen_h * 0.40f;

		// Only snap toward an edge when no other active zone lies between
		// this one and that edge. Without this test SHOOT, which sits in the
		// right-hand band, stretched all the way to the right edge and
		// swallowed JUMP — whose plate is closer to that edge — so pressing
		// jump fired a fireball instead and the player never left the ground.
		bool blocked_left = false, blocked_right = false;
		bool blocked_up = false, blocked_down = false;

		for( int j = 0; j < ZONE_COUNT; j++ )
		{
			if( j == i || !m_zones[j].active ) continue;

			const bool overlaps_rows = ( m_zones[j].y < zy + zh ) && ( m_zones[j].y + m_zones[j].h > zy );
			const bool overlaps_cols = ( m_zones[j].x < zx + zw ) && ( m_zones[j].x + m_zones[j].w > zx );

			if( overlaps_rows && m_zones[j].x + m_zones[j].w <= zx ) blocked_left  = true;
			if( overlaps_rows && m_zones[j].x >= zx + zw )           blocked_right = true;
			if( overlaps_cols && m_zones[j].y + m_zones[j].h <= zy ) blocked_up    = true;
			if( overlaps_cols && m_zones[j].y >= zy + zh )           blocked_down  = true;
		}

		if( zx < snap_x && !blocked_left )
		{
			hx = 0;
			hw = zx + zw + zw * 0.20f;
		}
		if( ( m_screen_w - ( zx + zw ) ) < snap_x && !blocked_right )
		{
			hw = m_screen_w - hx;
		}
		if( zy < snap_y && !blocked_up )
		{
			hy = 0;
			hh = zy + zh + zh * 0.20f;
		}
		if( ( m_screen_h - ( zy + zh ) ) < snap_y && !blocked_down )
		{
			hh = m_screen_h - hy;
		}

		if( screen_x >= hx && screen_x <= hx + hw &&
		    screen_y >= hy && screen_y <= hy + hh )
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
	LOG_DEBUG(INPUT, "Finger DOWN raw x=%.4f y=%.4f (screen %.0fx%.0f)",
		ev->tfinger.x, ev->tfinger.y, m_screen_w, m_screen_h);
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

	float new_w = static_cast<float>( pPreferences->m_video_screen_w );
	float new_h = static_cast<float>( pPreferences->m_video_screen_h );

	// Screen size changed (Android surface resize, window resize on desktop) —
	// rebuild zone rects so hit-tests and drawing match the new surface.
	// Release any held zone first so its finger_id doesn't point at stale coords.
	if( new_w != m_screen_w || new_h != m_screen_h )
	{
		Reset();
		m_screen_w = new_w;
		m_screen_h = new_h;
		Init_Zones();
	}

	// Release any held zones on Game_Mode transitions. Without this, a
	// zone that was pressed when the player died (e.g. D-pad RIGHT) stays
	// `pressed=true` forever — Press_Zone's early-return on pressed=true
	// then prevents new key injections, so the overworld arrows appear
	// dead after a level→overworld transition.
	int cur_mode = static_cast<int>( Game_Mode );
	if( cur_mode != m_last_game_mode )
	{
		Reset();
		m_last_game_mode = cur_mode;
	}

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

#ifdef __ANDROID__
	// GLES2: set screen-space projection for the touch overlay.
	// Zones are positioned in m_screen_w × m_screen_h space; the game
	// projection is game_res (800×600).  Switch to overlay projection,
	// draw directly via GLES2, then restore.
	GLES2::Set_Projection( m_screen_w, m_screen_h );
	glDisable( GL_DEPTH_TEST );
	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
#else
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
#endif

	Uint8 base_a = static_cast<Uint8>( m_opacity * 255.0f );

	// ---- D-PAD ---- (metal plate per arm + white arrow glyph)
	for( int i = ZONE_DPAD_LEFT; i <= ZONE_DPAD_DOWN; i++ )
	{
		if( !m_zones[i].active ) continue;
		bool pressed = m_zones[i].pressed;
		Draw_Metal_Plate( m_zones[i].x, m_zones[i].y, m_zones[i].w, m_zones[i].h,
			base_a, pressed );
		float cx = m_zones[i].x + m_zones[i].w * 0.5f;
		float cy = m_zones[i].y + m_zones[i].h * 0.5f;
		float gs = m_zones[i].w * 0.30f;
		Uint8 ga = pressed ? 255 : 190;
		int dir = (i == ZONE_DPAD_LEFT) ? 0 :
		          (i == ZONE_DPAD_RIGHT) ? 1 :
		          (i == ZONE_DPAD_UP) ? 2 : 3;
		Draw_Arrow_Glyph( cx, cy, gs, dir, 255, 255, 255, ga );
	}

	// ---- JUMP (brick-red tile + ↑ arrow) ----
	if( m_zones[ZONE_BTN_JUMP].active )
	{
		const TouchZone &z = m_zones[ZONE_BTN_JUMP];
		Draw_Brick_Tile( z.x, z.y, z.w, z.h, base_a, z.pressed );
		float cx = z.x + z.w * 0.5f;
		float cy = z.y + z.h * 0.5f;
		if( Game_Mode == MODE_OVERWORLD )
		{
			// Same key, different meaning: here it walks into the level.
			Draw_Door_Glyph( cx, cy, z.w * 0.42f, z.pressed ? 255 : 235 );
		}
		else
		{
			Draw_Arrow_Glyph( cx, cy, z.w * 0.32f, 2,
				255, 255, 255, z.pressed ? 255 : 235 );
		}
	}

	// ---- SHOOT (itembox `?` tile) ----
	if( m_zones[ZONE_BTN_SHOOT].active )
	{
		const TouchZone &z = m_zones[ZONE_BTN_SHOOT];
		Draw_Itembox_Tile( z.x, z.y, z.w, z.h, base_a, z.pressed );
		float cx = z.x + z.w * 0.5f;
		float cy = z.y + z.h * 0.5f;
		Draw_Flame_Glyph( cx, cy, z.w * 0.60f, z.pressed ? 255 : 245 );
	}

	// ---- PAUSE / BACK (wood sign + pause bars) ----
	if( m_zones[ZONE_BTN_BACK].active )
	{
		const TouchZone &z = m_zones[ZONE_BTN_BACK];
		float vis_w = z.w * 0.75f;
		float vis_h = z.h * 0.85f;
		float vis_x = z.x + (z.w - vis_w);
		float vis_y = z.y;
		Draw_Wood_Sign( vis_x, vis_y, vis_w, vis_h, base_a, z.pressed );
		float cx = vis_x + vis_w * 0.5f;
		float cy = vis_y + vis_h * 0.5f;
		Draw_Pause_Glyph( cx, cy, vis_h * 0.45f,
			255, 250, 230, z.pressed ? 255 : 230 );
	}

#ifdef __ANDROID__
	// Restore game projection and depth test
	GLES2::Set_Projection( static_cast<float>(game_res_w),
	                        static_cast<float>(game_res_h) );
	glEnable( GL_DEPTH_TEST );
#else
	glMatrixMode( GL_MODELVIEW );
	glPopMatrix();
	glMatrixMode( GL_PROJECTION );
	glPopMatrix();
	glMatrixMode( GL_MODELVIEW );
	glPopAttrib();
#endif
}

#ifndef __ANDROID__

// Desktop: immediate-mode OpenGL drawing helpers
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

#else // __ANDROID__ — GLES2: draw directly (bypass renderer queue / camera)

void cTouchControls :: Draw_Circle( float cx, float cy, float radius, Uint8 r, Uint8 g, Uint8 b, Uint8 a )
{
	GLES2::Draw_Circle( cx, cy, radius, r, g, b, a );
}

void cTouchControls :: Draw_Rounded_Rect( float x, float y, float w, float h, Uint8 r, Uint8 g, Uint8 b, Uint8 a )
{
	GLES2::Draw_Rect( x, y, w, h, r, g, b, a );
}

void cTouchControls :: Draw_Triangle( float x1, float y1, float x2, float y2, float x3, float y3,
	Uint8 r, Uint8 g, Uint8 b, Uint8 a )
{
	GLES2::Draw_Triangle( x1, y1, x2, y2, x3, y3, r, g, b, a );
}

#endif // __ANDROID__

/* *** *** *** In-universe SMC skin helpers *** *** *** */

// Generic beveled plate. Uses a single vertical gradient to bake the
// highlight (top) and shadow (bottom) into ONE draw call, plus one thin
// dark line at the bottom to suggest the dark outline. Total: 2 draws
// per plate (was 9). This is critical for mobile FPS — the per-frame
// draw call budget is small.
void cTouchControls :: Draw_Beveled_Plate( float x, float y, float w, float h,
	Uint8 r, Uint8 g, Uint8 b, Uint8 a, bool pressed )
{
	// Light rim drawn slightly larger than the plate, so the button keeps a
	// visible edge over black caves as well as over bright skies.
	{
		float rim = ( w < h ? w : h ) * 0.06f;
		Uint8 rim_a = static_cast<Uint8>( a * 0.55f );
		Draw_Rounded_Rect( x - rim, y - rim, w + rim * 2.0f, h + rim * 2.0f,
			255, 255, 255, rim_a );
	}

	if( pressed )
	{
		r = static_cast<Uint8>( r * 0.75f );
		g = static_cast<Uint8>( g * 0.75f );
		b = static_cast<Uint8>( b * 0.75f );
	}
	// Top color: lighter (highlight baked in)
	Uint8 tr = static_cast<Uint8>( r + (255 - r) * 0.40f );
	Uint8 tg = static_cast<Uint8>( g + (255 - g) * 0.40f );
	Uint8 tb = static_cast<Uint8>( b + (255 - b) * 0.40f );
	// Bottom color: darker (shadow baked in)
	Uint8 br = static_cast<Uint8>( r * 0.55f );
	Uint8 bg = static_cast<Uint8>( g * 0.55f );
	Uint8 bb = static_cast<Uint8>( b * 0.55f );
#ifdef __ANDROID__
	GLES2::Draw_Gradient_Vertical( x, y, w, h, tr, tg, tb, a, br, bg, bb, a );
#else
	// Desktop fallback: flat color (gradient helpers not duplicated here)
	Draw_Rounded_Rect( x, y, w, h, r, g, b, a );
#endif
	// One dark bottom line for crisp pixel-art definition
	float ol = ( w < h ? w : h ) * 0.04f;
	if( ol < 1.0f ) ol = 1.0f;
	Draw_Rounded_Rect( x, y + h - ol, w, ol, 15, 10, 5, a );
}

// Mario brick tile: warm red-brown gradient + one horizontal mortar joint.
// Cost: 3 draws (plate=2 + 1 mortar).
void cTouchControls :: Draw_Brick_Tile( float x, float y, float w, float h, Uint8 a, bool pressed )
{
	Draw_Beveled_Plate( x, y, w, h, 200, 85, 55, a, pressed );
	float mt = ( w < h ? w : h ) * 0.05f;
	if( mt < 2.0f ) mt = 2.0f;
	Uint8 mr = pressed ? 60 : 90;
	Uint8 mg = pressed ? 30 : 50;
	Uint8 mb = pressed ? 15 : 25;
	Draw_Rounded_Rect( x + mt, y + h * 0.5f - mt * 0.5f, w - mt * 2.0f, mt, mr, mg, mb, a );
}

// Itembox `?`-block tile: golden gradient + dark inner border ring drawn
// as a single inset frame (1 inner darker rect). The `?` glyph goes on
// top. Cost: 3 draws (plate=2 + 1 inner fill).
void cTouchControls :: Draw_Itembox_Tile( float x, float y, float w, float h, Uint8 a, bool pressed )
{
	Draw_Beveled_Plate( x, y, w, h, 235, 185, 50, a, pressed );
	float ins = ( w < h ? w : h ) * 0.13f;
	Uint8 br = pressed ? 80 : 130;
	Uint8 bg = pressed ? 45 : 75;
	Uint8 bb = pressed ? 10 : 20;
	// Single thin dark frame using a 1-pixel-tall horizontal "shelf" at
	// the inset top edge — visually evokes the itembox engraved border
	// without 4 separate rects.
	float bt = ( w < h ? w : h ) * 0.04f;
	if( bt < 2.0f ) bt = 2.0f;
	Draw_Rounded_Rect( x + ins, y + ins, w - ins * 2.0f, bt, br, bg, bb, a );
}

// Dark metal plate for the D-pad arms. Cool gunmetal gray.
void cTouchControls :: Draw_Metal_Plate( float x, float y, float w, float h, Uint8 a, bool pressed )
{
	Draw_Beveled_Plate( x, y, w, h, 75, 75, 85, a, pressed );
}

// Pause button background — warm wood-brown so it reads as an SMC level
// sign against any background.
void cTouchControls :: Draw_Wood_Sign( float x, float y, float w, float h, Uint8 a, bool pressed )
{
	Draw_Beveled_Plate( x, y, w, h, 175, 110, 55, a, pressed );
}

// Chunky arrow glyph — single-pass (no shadow). Plate is dark enough that
// a white arrow contrasts on its own. Cost: 2 draws (shaft + tip).
// dir: 0=Left 1=Right 2=Up 3=Down
void cTouchControls :: Draw_Arrow_Glyph( float cx, float cy, float size, int dir,
	Uint8 r, Uint8 g, Uint8 b, Uint8 a )
{
	float s = size;
	float sh = s * 0.4f;       // shaft thickness

	if( dir == 0 )      // LEFT
	{
		Draw_Rounded_Rect( cx - s * 0.20f, cy - sh * 0.5f, s * 0.85f, sh, r, g, b, a );
		Draw_Triangle( cx - s, cy,
			cx - s * 0.20f, cy - s * 0.55f,
			cx - s * 0.20f, cy + s * 0.55f, r, g, b, a );
	}
	else if( dir == 1 ) // RIGHT
	{
		Draw_Rounded_Rect( cx - s * 0.65f, cy - sh * 0.5f, s * 0.85f, sh, r, g, b, a );
		Draw_Triangle( cx + s, cy,
			cx + s * 0.20f, cy - s * 0.55f,
			cx + s * 0.20f, cy + s * 0.55f, r, g, b, a );
	}
	else if( dir == 2 ) // UP
	{
		Draw_Rounded_Rect( cx - sh * 0.5f, cy - s * 0.20f, sh, s * 0.85f, r, g, b, a );
		Draw_Triangle( cx, cy - s,
			cx - s * 0.55f, cy - s * 0.20f,
			cx + s * 0.55f, cy - s * 0.20f, r, g, b, a );
	}
	else                // DOWN
	{
		Draw_Rounded_Rect( cx - sh * 0.5f, cy - s * 0.65f, sh, s * 0.85f, r, g, b, a );
		Draw_Triangle( cx, cy + s,
			cx - s * 0.55f, cy + s * 0.20f,
			cx + s * 0.55f, cy + s * 0.20f, r, g, b, a );
	}
}

// `?` glyph — simplified to 4 primitives (vs 15 cells of pixel art).
// Builds the question mark from: top horizontal bar, right descender,
// diagonal hook, and bottom dot. Less pixel-accurate but stays readable
// and is 4× cheaper at draw time.
void cTouchControls :: Draw_Flame_Glyph( float cx, float cy, float size, Uint8 a )
{
	// Stacked rounded blocks, widest at the base, tapering upward: reads as a
	// flame at thumb size where a detailed sprite would turn to mush.
	float w = size * 0.62f;
	float h = size;
	float top = cy - h * 0.5f;

	// Outer flame, warm red
	Draw_Rounded_Rect( cx - w * 0.50f, top + h * 0.45f, w,         h * 0.55f, 220,  60,  20, a );
	Draw_Rounded_Rect( cx - w * 0.34f, top + h * 0.20f, w * 0.68f, h * 0.40f, 235,  90,  25, a );
	Draw_Rounded_Rect( cx - w * 0.16f, top,             w * 0.32f, h * 0.32f, 245, 130,  30, a );

	// Inner core, bright yellow
	Draw_Rounded_Rect( cx - w * 0.26f, top + h * 0.55f, w * 0.52f, h * 0.38f, 255, 215,  70, a );
	Draw_Rounded_Rect( cx - w * 0.13f, top + h * 0.34f, w * 0.26f, h * 0.30f, 255, 240, 140, a );
}

void cTouchControls :: Draw_Door_Glyph( float cx, float cy, float size, Uint8 a )
{
	// Doorway with a lighter opening and a knob. On the overworld this button
	// enters the level, which an up arrow never conveyed.
	float w = size * 0.70f;
	float h = size;
	float left = cx - w * 0.5f;
	float top  = cy - h * 0.5f;
	float t    = size * 0.14f;

	// Frame
	Draw_Rounded_Rect( left, top, w, h, 255, 255, 255, a );
	// Opening
	Draw_Rounded_Rect( left + t, top + t, w - t * 2.0f, h - t, 40, 30, 20, a );
	// Knob
	Draw_Rounded_Rect( left + w - t * 1.9f, cy, t * 0.7f, t * 0.7f, 255, 255, 255, a );
}

void cTouchControls :: Draw_QMark_Glyph( float cx, float cy, float size,
	Uint8 r, Uint8 g, Uint8 b, Uint8 a )
{
	float w  = size * 0.55f;
	float h  = size;
	float t  = size * 0.18f;            // stroke thickness
	float left = cx - w * 0.5f;
	float top  = cy - h * 0.5f;

	// Top horizontal bar
	Draw_Rounded_Rect( left, top, w, t, r, g, b, a );
	// Right descender (from top-right down to mid)
	Draw_Rounded_Rect( left + w - t, top, t, h * 0.45f, r, g, b, a );
	// Diagonal hook → fake with one small rect leaning toward center
	Draw_Rounded_Rect( cx - t * 0.5f, top + h * 0.40f, t, h * 0.25f, r, g, b, a );
	// Bottom dot
	Draw_Rounded_Rect( cx - t * 0.5f, top + h * 0.80f, t, t, r, g, b, a );
}

// Pause icon: two vertical bars.
void cTouchControls :: Draw_Pause_Glyph( float cx, float cy, float size,
	Uint8 r, Uint8 g, Uint8 b, Uint8 a )
{
	float bw = size * 0.30f;
	float bh = size * 1.05f;
	float gap = bw * 0.85f;
	Draw_Rounded_Rect( cx - gap * 0.5f - bw, cy - bh * 0.5f, bw, bh, r, g, b, a );
	Draw_Rounded_Rect( cx + gap * 0.5f,       cy - bh * 0.5f, bw, bh, r, g, b, a );
}

} // namespace SMC
