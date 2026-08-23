/***************************************************************************
 * menu_data.h
 *
 * Copyright (C) 2004 - 2011 Florian Richter
 ***************************************************************************/
/*
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef SMC_MENU_DATA_H
#define SMC_MENU_DATA_H

#include "../core/global_basic.h"
#include "../gui/menu.h"
#include "../gui/hud.h"

namespace SMC
{

/* *** *** *** *** *** *** *** cMenu_Base *** *** *** *** *** *** *** *** *** *** */

class cMenu_Base
{
public:
	cMenu_Base( void );
	virtual ~cMenu_Base( void );

	virtual void Init( void );
	virtual void Init_GUI( void );
	// Enter mode
	virtual void Enter( const GameMode old_mode = MODE_NOTHING );
	// Leave mode
	virtual void Leave( const GameMode next_mode = MODE_NOTHING );
	// Exit menu
	virtual void Exit( void );
	virtual void Update( void );
	virtual void Draw( void );
	void Draw_End( void );

	// Set the game mode to return on exit
	void Set_Exit_To_Game_Mode( GameMode gamemode );

	// gui layout filename (kept for reference; no longer used to load CEGUI layouts)
	std::string m_layout_file;

	// if button/key action
	bool m_action;

	// menu position
	float m_menu_pos_y;
	// default text color
	Color m_text_color;
	// value text color
	Color m_text_color_value;
	// return to this game mode on exit
	GameMode m_exit_to_gamemode;

	// current menu sprites
	typedef vector<cHudSprite *> HudSpriteList;
	HudSpriteList m_draw_list;
};

/* *** *** *** *** *** *** *** cMenu_Main *** *** *** *** *** *** *** *** *** *** */

class cMenu_Main : public cMenu_Base
{
public:
	cMenu_Main( void );
	virtual ~cMenu_Main( void );

	virtual void Init( void );
	virtual void Init_GUI( void );
	virtual void Exit( void );
	virtual void Update( void );
	virtual void Draw( void );
};

/* *** *** *** *** *** *** *** cMenu_Start *** *** *** *** *** *** *** *** *** *** */

class cMenu_Start : public cMenu_Base
{
public:
	cMenu_Start( void );
	virtual ~cMenu_Start( void );

	virtual void Init( void );
	virtual void Init_GUI( void );
	virtual void Exit( void );
	virtual void Update( void );
	virtual void Draw( void );

	/* Highlight the given level (selects level tab and item).
	 * Called from game_core.cpp when returning to start menu after a level.
	 * Returns true if the level was found.
	*/
	bool Highlight_Level( std::string lvl_name );

	/* Load the item at idx in the current tab; no-op if idx is out of range */
	void Load_Item( int idx );

	/* Load the Campaign and exit if successful */
	void Load_Campaign( std::string name );
	/* Load the World and exit if successful */
	void Load_World( std::string name );
	/* Load the Level and exit if successful */
	bool Load_Level( std::string name );

	// ModernUI state
	std::vector<std::string> m_campaign_names;
	std::vector<std::string> m_world_names;
	std::vector<std::string> m_level_names;
	int m_active_tab;        // 0=Campaign, 1=World, 2=Level
	int m_scroll_offset;     // current tab's scroll offset
	int m_selected_item;     // currently selected item index (-1 = none)
	// Surfaces rendered this frame; deleted at the start of the next Draw()
	// after the game loop has called pVideo->Render()
	std::vector<cGL_Surface*> m_pending_delete;
};

/* *** *** *** *** *** *** *** cMenu_Options *** *** *** *** *** *** *** *** *** *** */

class cMenu_Options : public cMenu_Base
{
public:
	cMenu_Options( void );
	virtual ~cMenu_Options( void );

	virtual void Init( void );
	virtual void Init_GUI( void );
	void Init_GUI_Game( void );
	void Init_GUI_Video( void );
	void Init_GUI_Audio( void );
	void Init_GUI_Keyboard( void );
	void Init_GUI_Joystick( void );
	void Init_GUI_Editor( void );
	virtual void Exit( void );
	virtual void Update( void );
	void Change_Game_Setting( int setting );
	void Change_Video_Setting( int setting );
	void Change_Audio_Setting( int setting );
	void Change_Keyboard_Setting( int setting );
	void Change_Joystick_Setting( int setting );
	void Change_Editor_Setting( int setting );
	virtual void Draw( void );

	/* Build the shortcut list
	 * joystick : if true build it for joystick
	*/
	void Build_Shortcut_List( bool joystick = 0 );
	/* Set the given Shortcut
	 * and exit if successful
	 * joystick : if true set it for joystick
	*/
	void Set_Shortcut( std::string name, void *data, bool joystick = 0 );

	// Select given Joystick
	void Joy_Default( unsigned int index );
	// Disable Joystick
	void Joy_Disable( void );

	// ModernUI tab index: 0=Game 1=Video 2=Audio 3=Keyboard 4=Joystick 5=Editor
	int m_active_tab;

	// ModernUI Game tab state
	std::vector<std::string> m_game_languages;   // language codes (first entry = "default")
	int m_game_language_idx;                     // selected index in m_game_languages
	std::vector<std::string> m_game_menu_levels; // menu level names
	int m_game_menu_level_idx;                   // selected index in m_game_menu_levels
	float m_game_camera_hor_speed;               // cached camera hor speed
	float m_game_camera_ver_speed;               // cached camera ver speed
	// ModernUI Keyboard tab state
	std::vector<std::string> m_kbd_items;  // "Name  [Key]" display strings
	int m_kbd_selected;                    // selected row index (-1 = none)
	int m_kbd_scroll;                      // scroll offset
	float m_kbd_scroll_speed;             // cached scroll speed
	// ModernUI Joystick tab state
	std::vector<std::string> m_joy_names_list; // "None" + joystick names
	int m_joy_selected_idx;                    // selected joystick index in m_joy_names_list
	std::vector<std::string> m_joy_items;      // "Name  [Btn]" display strings
	int m_joy_btn_selected;                    // selected button row (-1 = none)
	int m_joy_btn_scroll;                      // scroll offset
	float m_joy_sensitivity;                   // cached axis threshold (0-32767)
	int m_joy_axis_hor;                        // cached horizontal axis
	int m_joy_axis_ver;                        // cached vertical axis
	// ModernUI Editor tab state
	unsigned int m_editor_item_image_size;     // cached item image size (5-60)
	// video settings
	unsigned int m_vid_w;
	unsigned int m_vid_h;
	unsigned int m_vid_bpp;
	bool m_vid_fullscreen;
	bool m_vid_vsync;
	float m_vid_geometry_detail;
	float m_vid_texture_detail;
	// ModernUI Video tab state
	std::vector<std::string> m_vid_resolutions;  // "WxH" strings
	int m_vid_res_idx;                            // selected index in m_vid_resolutions
	// ModernUI Audio tab state
	int   m_audio_hz_idx;     // 0=22050, 1=44100, 2=48000
	float m_audio_music_vol;  // 0–128
	float m_audio_sound_vol;  // 0–128
	// ModernUI shared: deferred surface delete
	std::vector<cGL_Surface*> m_opt_pending_delete;
	// ModernUI post-CEGUI rendering callback (set when Video/Audio tab active)
	void Post_GUI_Draw( void );
	static void S_Post_GUI_Draw( void );

	// Shortcut item (used by Build_Shortcut_List — now dead code, kept for reference)
	class cShortcut_item
	{
	public:
		cShortcut_item( const std::string &name, void *key, const void *key_default )
		{
			m_name = name;
			m_key = key;
			m_key_default = key_default;
		}

		std::string m_name;
		void *m_key;
		const void *m_key_default;
	};
};

/* *** *** *** *** *** *** *** cMenu_Savegames *** *** *** *** *** *** *** *** *** *** */

class cMenu_Savegames : public cMenu_Base
{
public:
	cMenu_Savegames( bool type );
	virtual ~cMenu_Savegames( void );

	virtual void Init( void );
	virtual void Init_GUI( void );
	virtual void Exit( void );
	virtual void Update( void );
	virtual void Draw( void );

	void Update_Load( void );
	void Update_Save( void );

	// Set Savegame Description
	std::string Set_Save_Description( unsigned int save_slot );
	// Update Savegame Descriptions
	void Update_Saved_Games_Text( void );

	// Savegame images
	HudSpriteList m_savegame_temp;

	// if save menu
	bool m_type_save;
};

/* *** *** *** *** *** *** *** cMenu_Credits *** *** *** *** *** *** *** *** *** *** */

class cMenu_Credits : public cMenu_Base
{
public:
	cMenu_Credits( void );
	virtual ~cMenu_Credits( void );

	virtual void Init( void );
	virtual void Init_GUI( void );
	virtual void Enter( const GameMode old_mode = MODE_NOTHING );
	virtual void Leave( const GameMode next_mode = MODE_NOTHING );
	virtual void Exit( void );
	virtual void Update( void );
	virtual void Draw( void );


	// Add a line to the credits text
	void Add_Credits_Line( const std::string &text, float posx, float posy, const Color &shadow_color = black, float shadow_pos = 0.0f );
	/* fade from the normal menu to the the credits menu
	 * fade_in : if set fade in instead of fade out
	*/
	void Menu_Fade( bool fade_in = 1 );
};

/* *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** */

} // namespace SMC

#endif
