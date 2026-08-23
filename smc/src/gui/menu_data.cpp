/***************************************************************************
 * menu_data.cpp  -  menu data and handling classes
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

#include "../core/global_basic.h"
#include "../gui/menu_data.h"
#include "../audio/audio.h"
#include "../core/game_core.h"
#include "../gui/generic.h"
#include "../video/font.h"
#include "../overworld/overworld.h"
#include "../core/campaign_manager.h"
#include "../user/preferences.h"
#include "../input/joystick.h"
#include "../input/mouse.h"
#include "../core/framerate.h"
#include "../user/savegame.h"
#include "../video/renderer.h"
#include "../level/level.h"
#include "../input/keyboard.h"
#include "../level/level_editor.h"
#include "../core/math/utilities.h"
#include "../core/i18n.h"
#include "../core/math/size.h"
#include "../core/filesystem/filesystem.h"
#include "../core/filesystem/resource_manager.h"
#include "../gui/modern_ui.h"
// (CEGUI includes removed in M12 — menu shell now uses ModernUI exclusively)

// GL_MODULATE is a fixed-function constant not defined in GLES2 headers.
// On Android the renderer already skips the glTexEnvi combine path (guarded
// by #ifndef __ANDROID__), so passing the numeric value 0x2100 is harmless —
// m_combine_type is stored but never consumed on GLES2.
#ifndef GL_MODULATE
#  define GL_MODULATE 0x2100
#endif

namespace SMC
{

#include "../gui/ui_palette.h"

/* *** *** *** *** *** *** *** *** cMenu_Base *** *** *** *** *** *** *** *** *** */

cMenu_Base :: cMenu_Base( void )
{
	m_action = 0;
	m_menu_pos_y = 140.0f;
	m_text_color = Color( static_cast<Uint8>(255), 251, 98 );
	m_text_color_value = Color( static_cast<Uint8>(255), 190, 30 );

	m_exit_to_gamemode = MODE_NOTHING;
}

cMenu_Base :: ~cMenu_Base( void )
{
	for( HudSpriteList::iterator itr = m_draw_list.begin(); itr != m_draw_list.end(); ++itr )
	{
		delete *itr;
	}
	
	m_draw_list.clear();
}

void cMenu_Base :: Init( void )
{
	m_layout_file = "";
}

void cMenu_Base :: Init_GUI( void )
{
	// All screens now use ModernUI; CEGUI layout loading removed in M12.
}

void cMenu_Base :: Enter( const GameMode old_mode /* = MODE_NOTHING */ )
{
	// virtual
}

void cMenu_Base :: Leave( const GameMode next_mode /* = MODE_NOTHING */ )
{
	// virtual
}

void cMenu_Base :: Exit( void )
{
	// virtual
}

void cMenu_Base :: Update( void )
{
	if( m_exit_to_gamemode != MODE_LEVEL && m_exit_to_gamemode != MODE_OVERWORLD )
	{
		// animation
		pMenuCore->m_animation_manager->Update();
	}

	// hud
	pHud_Manager->Update();
}

void cMenu_Base :: Draw( void )
{
	pVideo->Clear_Screen();

	if( m_exit_to_gamemode == MODE_LEVEL )
	{
		pActive_Level->m_sprite_manager->Update_Items_Valid_Draw();
		// draw level layer 1
		pActive_Level->Draw_Layer_1();
		// draw alpha rect
		pVideo->Draw_Rect( NULL, 0.125f, &blackalpha128 );

		// gui
		pMenuCore->m_handler->Draw( 0 );
	}
	else if( m_exit_to_gamemode == MODE_OVERWORLD )
	{
		pActive_Overworld->m_sprite_manager->Update_Items_Valid_Draw();
		// draw world layer 1
		pActive_Overworld->Draw_Layer_1();
		// draw alpha rect
		pVideo->Draw_Rect( NULL, 0.125f, &blackalpha128 );

		// gui
		pMenuCore->m_handler->Draw( 0 );
	}
	else
	{
		// animation
		pMenuCore->m_animation_manager->Draw();
		// gui
		pMenuCore->m_handler->Draw();
	}

	// menu items
	for( HudSpriteList::iterator itr = m_draw_list.begin(); itr != m_draw_list.end(); ++itr )
	{
		(*itr)->Draw();
	}
}

void cMenu_Base :: Draw_End( void )
{
	// hud
	pHud_Manager->Draw();
}

void cMenu_Base :: Set_Exit_To_Game_Mode( GameMode gamemode )
{
	m_exit_to_gamemode = gamemode;
}

/* *** *** *** *** *** *** *** *** cMenu_Main *** *** *** *** *** *** *** *** *** */

cMenu_Main :: cMenu_Main( void )
: cMenu_Base()
{

}

cMenu_Main :: ~cMenu_Main( void )
{

}

void cMenu_Main :: Init( void )
{
	cMenu_Base::Init();

	cMenu_Item *temp_item = NULL;

	// No CEGUI layout — version text is rendered directly in Init_GUI
	m_layout_file = "";

	// Start
	temp_item = pMenuCore->Auto_Menu( "start.png", "start.png", m_menu_pos_y );
	temp_item->m_image_menu->Set_Pos( temp_item->m_pos_x + ( temp_item->m_image_default->m_col_rect.m_w + 16 ), temp_item->m_pos_y );
	pMenuCore->m_handler->Add_Menu_Item( temp_item );
	// Options
	m_menu_pos_y += 60;
	temp_item = pMenuCore->Auto_Menu( "options.png", "options.png", m_menu_pos_y );
	temp_item->m_image_menu->Set_Pos( temp_item->m_pos_x - temp_item->m_image_menu->m_col_rect.m_w - 16, temp_item->m_pos_y );
	pMenuCore->m_handler->Add_Menu_Item( temp_item );
	// Load
	m_menu_pos_y += 60;
	temp_item = pMenuCore->Auto_Menu( "load.png", "load.png", m_menu_pos_y );
	temp_item->m_image_menu->Set_Pos( temp_item->m_pos_x + ( temp_item->m_image_default->m_col_rect.m_w + 16 ), temp_item->m_pos_y );
	pMenuCore->m_handler->Add_Menu_Item( temp_item );
	// Save
	m_menu_pos_y += 60;
	temp_item = pMenuCore->Auto_Menu( "save.png", "save.png", m_menu_pos_y );
	temp_item->m_image_menu->Set_Pos( temp_item->m_pos_x - temp_item->m_image_menu->m_col_rect.m_w - 16, temp_item->m_pos_y );
	pMenuCore->m_handler->Add_Menu_Item( temp_item );
	// Quit
	m_menu_pos_y += 60;
	temp_item = pMenuCore->Auto_Menu( "quit.png", "", m_menu_pos_y, 1 );
	temp_item->m_image_menu->Set_Pos( temp_item->m_pos_x + temp_item->m_col_rect.m_w + 16, temp_item->m_pos_y );
	pMenuCore->m_handler->Add_Menu_Item( temp_item );

	if( m_exit_to_gamemode == MODE_NOTHING )
	{
		// Credits
		cGL_Surface *credits = pFont->Render_Text( pFont->m_font_normal, _("Credits"), yellow );
		temp_item = new cMenu_Item( pMenuCore->m_handler->m_level->m_sprite_manager );
		temp_item->m_image_default->Set_Image( credits );
		temp_item->Set_Pos( static_cast<float>(game_res_w) * 0.45f, static_cast<float>(game_res_h) - 30.0f );
		pMenuCore->m_handler->Add_Menu_Item( temp_item, 1.5f, grey );

		cHudSprite *hud_sprite = new cHudSprite( pMenuCore->m_handler->m_level->m_sprite_manager );
		hud_sprite->Set_Image( credits, 0, 1 );
		hud_sprite->Set_Pos( -200, 0 );
		m_draw_list.push_back( hud_sprite );
		// SDL logo
		hud_sprite = new cHudSprite( pMenuCore->m_handler->m_level->m_sprite_manager );
		hud_sprite->Set_Image( pVideo->Get_Surface( "menu/logo_sdl.png" ) );
		hud_sprite->Set_Pos( static_cast<float>(game_res_w) * 0.04f, static_cast<float>(game_res_h) * 0.935f );
		m_draw_list.push_back( hud_sprite );
	}

	Init_GUI();
}

void cMenu_Main :: Init_GUI( void )
{
	// No CEGUI layout — render version text and website URL as HUD sprites

	std::string version_str = std::string("Version ") + int_to_string(SMC_VERSION_MAJOR) + "." + int_to_string(SMC_VERSION_MINOR) + "." + int_to_string(SMC_VERSION_PATCH);
	cGL_Surface *version_surf = pFont->Render_Text( pFont->m_font_small, version_str, green );
	cHudSprite *version_sprite = new cHudSprite( pMenuCore->m_handler->m_level->m_sprite_manager );
	version_sprite->Set_Image( version_surf, 1, 1 );
	version_sprite->Set_Pos( static_cast<float>(game_res_w) * 0.80f, static_cast<float>(game_res_h) * 0.945f );
	m_draw_list.push_back( version_sprite );

	// website URL — only shown when not in a level/world
	if( m_exit_to_gamemode == MODE_NOTHING )
	{
		cGL_Surface *website_surf = pFont->Render_Text( pFont->m_font_small, "www.secretmaryo.org", orange );
		cHudSprite *website_sprite = new cHudSprite( pMenuCore->m_handler->m_level->m_sprite_manager );
		website_sprite->Set_Image( website_surf, 1, 1 );
		website_sprite->Set_Pos( static_cast<float>(game_res_w) * 0.70f, static_cast<float>(game_res_h) * 0.135f );
		m_draw_list.push_back( website_sprite );
	}
}

void cMenu_Main :: Exit( void )
{
	if( m_exit_to_gamemode == MODE_LEVEL )
	{
		Game_Action = GA_ENTER_LEVEL;
#ifndef SMC_NO_CEGUI
		Game_Action_Data_Middle.add( "unload_menu", "1" );
#endif
	}
	else if( m_exit_to_gamemode == MODE_OVERWORLD )
	{
		Game_Action = GA_ENTER_WORLD;
#ifndef SMC_NO_CEGUI
		Game_Action_Data_Middle.add( "unload_menu", "1" );
#endif
	}
}

void cMenu_Main :: Update( void )
{
	cMenu_Base::Update();

	if( !m_action )
	{
		return;
	}

	m_action = 0;

	// Start
	if( pMenuCore->m_handler->m_active == 0 )
	{
		Game_Action = GA_ENTER_MENU;
#ifndef SMC_NO_CEGUI
		Game_Action_Data_Middle.add( "load_menu", int_to_string( MENU_START ) );
#else
		g_android_next_menu = MENU_START;
#endif
	}
	// Options
	else if( pMenuCore->m_handler->m_active == 1 )
	{
		Game_Action = GA_ENTER_MENU;
#ifndef SMC_NO_CEGUI
		Game_Action_Data_Middle.add( "load_menu", int_to_string( MENU_OPTIONS ) );
#else
		g_android_next_menu = MENU_OPTIONS;
#endif
	}
	// Load
	else if( pMenuCore->m_handler->m_active == 2 )
	{
		Game_Action = GA_ENTER_MENU;
#ifndef SMC_NO_CEGUI
		Game_Action_Data_Middle.add( "load_menu", int_to_string( MENU_LOAD ) );
#else
		g_android_next_menu = MENU_LOAD;
#endif
	}
	// Save
	else if( pMenuCore->m_handler->m_active == 3 )
	{
		Game_Action = GA_ENTER_MENU;
#ifndef SMC_NO_CEGUI
		Game_Action_Data_Middle.add( "load_menu", int_to_string( MENU_SAVE ) );
#else
		g_android_next_menu = MENU_SAVE;
#endif
	}
	// Quit
	else if( pMenuCore->m_handler->m_active == 4 )
	{
		game_exit = 1;
	}
	// Credits
	else if( pMenuCore->m_handler->m_active == 5 )
	{
		Game_Action = GA_ENTER_MENU;
#ifndef SMC_NO_CEGUI
		Game_Action_Data_Middle.add( "load_menu", int_to_string( MENU_CREDITS ) );
		Game_Action_Data_Start.add( "music_fadeout", "500" );
#else
		g_android_next_menu = MENU_CREDITS;
#endif
	}

	if( m_exit_to_gamemode != MODE_NOTHING )
	{
#ifndef SMC_NO_CEGUI
		Game_Action_Data_Middle.add( "menu_exit_back_to", int_to_string( m_exit_to_gamemode ) );
#endif
	}
}

void cMenu_Main :: Draw( void )
{
	cMenu_Base::Draw();
	Draw_End();
}

/* *** *** *** *** *** *** *** *** cMenu_Start *** *** *** *** *** *** *** *** *** */

cMenu_Start :: cMenu_Start( void )
: cMenu_Base()
, m_active_tab( 1 )
, m_scroll_offset( 0 )
, m_selected_item( -1 )
{

}

cMenu_Start :: ~cMenu_Start( void )
{

}

void cMenu_Start :: Init( void )
{
	cMenu_Base::Init();

	// m_layout_file left empty — ModernUI replaces the CEGUI layout (see Init_GUI)

	cHudSprite *hud_sprite = new cHudSprite( pMenuCore->m_handler->m_level->m_sprite_manager );
	hud_sprite->Set_Image( pVideo->Get_Surface( "menu/start.png" ) );
	hud_sprite->Set_Pos( static_cast<float>(game_res_w) * 0.02f, 140 );
	m_draw_list.push_back( hud_sprite );
	hud_sprite = new cHudSprite( pMenuCore->m_handler->m_level->m_sprite_manager );
	hud_sprite->Set_Image( pVideo->Get_Surface( "menu/items/overworld.png" ) );
	hud_sprite->Set_Pos( static_cast<float>(game_res_w) / 20, 210 );
	m_draw_list.push_back( hud_sprite );

	Init_GUI();
}

void cMenu_Start :: Init_GUI( void )
{
	// No CEGUI layout — we use ModernUI directly
	m_layout_file = "";  // prevent cMenu_Base::Init_GUI() from loading a CEGUI layout

	// Populate campaign list
	m_campaign_names.clear();
	for( vector<cCampaign *>::const_iterator itr = pCampaign_Manager->objects.begin(); itr != pCampaign_Manager->objects.end(); ++itr )
		m_campaign_names.push_back( (*itr)->m_name );

	// Populate world list (visible worlds only, except in debug)
	m_world_names.clear();
	for( vector<cOverworld *>::const_iterator itr = pOverworld_Manager->objects.begin(); itr != pOverworld_Manager->objects.end(); ++itr )
	{
		const cOverworld_description *world = (*itr)->m_description;
#ifndef _DEBUG
		if( !world->m_visible ) continue;
#endif
		m_world_names.push_back( world->m_name );
	}

	// Populate level list (game + user levels)
	m_level_names.clear();
	auto add_levels = [&]( const std::string &dir ) {
		vector<std::string> files = Get_Directory_Files( dir, "smclvl", 0, 0 );
		for( auto &f : files )
			m_level_names.push_back( Trim_Filename( f, false, false ) );
	};
	add_levels( DATA_DIR "/" GAME_LEVEL_DIR );
	add_levels( pResource_Manager->user_data_dir + USER_LEVEL_DIR );
	std::sort( m_level_names.begin(), m_level_names.end() );
}

void cMenu_Start :: Exit( void )
{
	Game_Action = GA_ENTER_MENU;
#ifndef SMC_NO_CEGUI
	Game_Action_Data_Middle.add( "load_menu", int_to_string( MENU_MAIN ) );
	if( m_exit_to_gamemode != MODE_NOTHING )
	{
		Game_Action_Data_Middle.add( "menu_exit_back_to", int_to_string( m_exit_to_gamemode ) );
	}
#else
	g_android_next_menu = MENU_MAIN;
#endif
}

void cMenu_Start :: Update( void )
{
	cMenu_Base::Update();
}

static const std::vector<std::string> &Get_Tab_Labels( void )
{
	static const std::vector<std::string> labels = { _("Campaign"), _("World"), _("Level") };
	return labels;
}

void cMenu_Start :: Load_Item( int idx )
{
	const std::vector<std::string> *items = nullptr;
	if( m_active_tab == 0 )      items = &m_campaign_names;
	else if( m_active_tab == 1 ) items = &m_world_names;
	else                         items = &m_level_names;

	if( idx < 0 || idx >= static_cast<int>(items->size()) ) return;

	if( m_active_tab == 0 )      Load_Campaign( (*items)[idx] );
	else if( m_active_tab == 1 ) Load_World( (*items)[idx] );
	else                         Load_Level( (*items)[idx] );
}

void cMenu_Start :: Draw( void )
{
	// Delete surfaces from the previous frame now that the game loop has called Render
	for( unsigned int i = 0; i < m_pending_delete.size(); ++i )
		delete m_pending_delete[i];
	m_pending_delete.clear();

	cMenu_Base::Draw();  // draws background, HUD sprites

	// Panel layout: centered panel
	const float PANEL_W = game_res_w * 0.75f;
	const float PANEL_H = game_res_h * 0.70f;
	const float PANEL_X = ( game_res_w - PANEL_W ) * 0.5f;
	const float PANEL_Y = ( game_res_h - PANEL_H ) * 0.5f;
	const float PAD = 10.0f;
	const float TAB_H = 36.0f;
	const float BTN_H = 34.0f;
	const float BTN_W = 110.0f;
	const float LIST_H = PANEL_H - TAB_H - BTN_H - PAD * 3.0f;

	// Background panel
	Color bg( static_cast<Uint8>(255), 248, 220, 240 );
	pVideo->Draw_Rect( PANEL_X, PANEL_Y, PANEL_W, PANEL_H, 0.89f, &bg );
	Color border( static_cast<Uint8>(139), 90, 43, 200 );
	pVideo->Draw_Rect( PANEL_X - 2, PANEL_Y - 2, PANEL_W + 4, PANEL_H + 4, 0.889f, &border );

	// Tab bar
	int new_tab = ModernUI::Tab_Bar( PANEL_X, PANEL_Y, PANEL_W, TAB_H,
	                                 Get_Tab_Labels(), m_active_tab, m_pending_delete );
	if( new_tab != m_active_tab )
	{
		m_active_tab    = new_tab;
		m_scroll_offset = 0;
		m_selected_item = -1;
	}

	// Item list
	const std::vector<std::string> *items = nullptr;
	if( m_active_tab == 0 )      items = &m_campaign_names;
	else if( m_active_tab == 1 ) items = &m_world_names;
	else                         items = &m_level_names;

	float list_y = PANEL_Y + TAB_H + PAD;
	int result = ModernUI::Scroll_List( PANEL_X, list_y, PANEL_W, LIST_H,
	                                    *items, m_selected_item,
	                                    m_scroll_offset, m_pending_delete );
	if( result >= 0 )
	{
		int idx = result & ~ModernUI::SCROLL_LIST_DCLICK_FLAG;
		m_selected_item = idx;
		if( result & ModernUI::SCROLL_LIST_DCLICK_FLAG )
			Load_Item( idx );
	}

	// Enter / Back buttons
	float btn_y   = list_y + LIST_H + PAD;
	float enter_x = PANEL_X + ( PANEL_W * 0.5f ) - BTN_W - PAD;
	float back_x  = PANEL_X + ( PANEL_W * 0.5f ) + PAD;
	if( ModernUI::Button( enter_x, btn_y, BTN_W, BTN_H, _("Enter"), m_pending_delete ) )
		Load_Item( m_selected_item );
	if( ModernUI::Button( back_x, btn_y, BTN_W, BTN_H, _("Back"), m_pending_delete ) )
		Exit();

	Draw_End();
}

bool cMenu_Start :: Highlight_Level( std::string lvl_name )
{
	if( lvl_name.empty() )
	{
		return 0;
	}

	// Switch to Level tab (tab index 2)
	m_active_tab    = 2;
	m_scroll_offset = 0;
	m_selected_item = -1;

	// Find the level in the list and select it
	for( int i = 0; i < static_cast<int>(m_level_names.size()); i++ )
	{
		if( m_level_names[i] == lvl_name )
		{
			m_selected_item = i;
			// Scroll so the item is visible (approximate: items are ~26px each)
			m_scroll_offset = ( i > 3 ) ? ( i - 2 ) : 0;
			return 1;
		}
	}

	return 0;
}

void cMenu_Start :: Load_Campaign( std::string name )
{
	if( pLevel_Player->m_points > 0 && !Box_Question( _("This will reset your current progress.\nContinue ?") ) )
	{
		return;
	}

	cCampaign *new_campaign = pCampaign_Manager->Get_from_Name( name );

	// if not available
	if( !new_campaign )
	{
		pHud_Debug->Set_Text( _("Couldn't load campaign ") + name, static_cast<float>(speedfactor_fps) );
	}
	else
	{
		// enter level
		if( new_campaign->m_is_target_level )
		{
			Game_Action = GA_ENTER_LEVEL;
			Game_Mode_Type = MODE_TYPE_LEVEL_CUSTOM;
#ifndef SMC_NO_CEGUI
			Game_Action_Data_Middle.add( "load_level", new_campaign->m_target.c_str() );
#endif
		}
		// enter world
		else
		{
			Game_Action = GA_ENTER_WORLD;
#ifndef SMC_NO_CEGUI
			Game_Action_Data_Middle.add( "enter_world", new_campaign->m_target.c_str() );
#endif
		}

#ifndef SMC_NO_CEGUI
		Game_Action_Data_Start.add( "music_fadeout", "1000" );
		Game_Action_Data_Start.add( "screen_fadeout", int_to_string( EFFECT_OUT_BLACK ) );
		Game_Action_Data_Start.add( "screen_fadeout_speed", "3" );
		Game_Action_Data_Middle.add( "unload_menu", "1" );
		Game_Action_Data_Middle.add( "reset_save", "1" );
		Game_Action_Data_End.add( "screen_fadein", int_to_string( EFFECT_IN_RANDOM ) );
		Game_Action_Data_End.add( "screen_fadein_speed", "3" );
#endif
	}
}

void cMenu_Start :: Load_World( std::string name )
{
	if( pLevel_Player->m_points > 0 && !Box_Question( _("This will reset your current progress.\nContinue ?") ) )
	{
		return;
	}

	cOverworld *new_world = pOverworld_Manager->Get_from_Name( name );

	// if not available
	if( !new_world )
	{
		pHud_Debug->Set_Text( _("Couldn't load overworld ") + name, static_cast<float>(speedfactor_fps) );
	}
	else
	{
		// enter world
		Game_Action = GA_ENTER_WORLD;
#ifndef SMC_NO_CEGUI
		Game_Action_Data_Start.add( "music_fadeout", "1000" );
		Game_Action_Data_Start.add( "screen_fadeout", int_to_string( EFFECT_OUT_BLACK ) );
		Game_Action_Data_Start.add( "screen_fadeout_speed", "3" );
		Game_Action_Data_Middle.add( "enter_world", name.c_str() );
		Game_Action_Data_Middle.add( "unload_menu", "1" );
		Game_Action_Data_Middle.add( "reset_save", "1" );
		Game_Action_Data_End.add( "screen_fadein", int_to_string( EFFECT_IN_RANDOM ) );
		Game_Action_Data_End.add( "screen_fadein_speed", "3" );
#endif
	}
}

bool cMenu_Start :: Load_Level( std::string level_name )
{
	if( pLevel_Player->m_points > 0 && !Box_Question( _("This will reset your current progress.\nContinue ?") ) )
	{
		return 0;
	}

	// if not available
	if( !pLevel_Manager->Get_Path( level_name ) )
	{
		pAudio->Play_Sound( "error.ogg" );
		pHud_Debug->Set_Text( _("Couldn't load level ") + level_name, static_cast<float>(speedfactor_fps) );
		return 0;
	}

	// enter level
	Game_Action = GA_ENTER_LEVEL;
	Game_Mode_Type = MODE_TYPE_LEVEL_CUSTOM;
#ifndef SMC_NO_CEGUI
	Game_Action_Data_Start.add( "music_fadeout", "1000" );
	Game_Action_Data_Start.add( "screen_fadeout", int_to_string( EFFECT_OUT_BLACK ) );
	Game_Action_Data_Start.add( "screen_fadeout_speed", "3" );
	Game_Action_Data_Middle.add( "load_level", level_name.c_str() );
	Game_Action_Data_Middle.add( "unload_menu", "1" );
	Game_Action_Data_Middle.add( "reset_save", "1" );
	Game_Action_Data_End.add( "screen_fadein", int_to_string( EFFECT_IN_RANDOM ) );
	Game_Action_Data_End.add( "screen_fadein_speed", "3" );
#endif

	return 1;
}

// Note: All CEGUI event handlers for cMenu_Start were removed in M7.
// The screen is fully rendered via ModernUI; see Draw() above.

/* *** *** *** *** *** *** *** *** cMenu_Options *** *** *** *** *** *** *** *** *** */

// Static pointer used by Post-GUI render callback (at most one options menu active at a time)
static cMenu_Options *s_active_opts = NULL;

cMenu_Options :: cMenu_Options( void )
: cMenu_Base()
, m_active_tab( 0 )
, m_vid_res_idx( 0 )
, m_audio_hz_idx( 1 )
, m_audio_music_vol( 0.0f )
, m_audio_sound_vol( 0.0f )
, m_game_language_idx( 0 )
, m_game_menu_level_idx( 0 )
, m_game_camera_hor_speed( 0.0f )
, m_game_camera_ver_speed( 0.0f )
, m_kbd_selected( -1 )
, m_kbd_scroll( 0 )
, m_kbd_scroll_speed( 1.0f )
, m_joy_selected_idx( 0 )
, m_joy_btn_selected( -1 )
, m_joy_btn_scroll( 0 )
, m_joy_sensitivity( 10000.0f )
, m_joy_axis_hor( 0 )
, m_joy_axis_ver( 1 )
, m_editor_item_image_size( 50 )
{
}

cMenu_Options :: ~cMenu_Options( void )
{
	for( unsigned int i = 0; i < m_opt_pending_delete.size(); i++ )
		delete m_opt_pending_delete[i];
	m_opt_pending_delete.clear();
	if( s_active_opts == this )
	{
		s_active_opts = NULL;
		pVideo->Set_Post_GUI_Render( NULL );
	}
}

void cMenu_Options :: Init( void )
{
	cMenu_Base::Init();
	m_layout_file = "";  // no CEGUI layout

	// video settings
	m_vid_w = pPreferences->m_video_screen_w;
	m_vid_h = pPreferences->m_video_screen_h;
	m_vid_bpp = pPreferences->m_video_screen_bpp;
	m_vid_fullscreen = pPreferences->m_video_fullscreen;
	m_vid_vsync = pPreferences->m_video_vsync;
	m_vid_geometry_detail = pVideo->m_geometry_quality;
	m_vid_texture_detail = pVideo->m_texture_quality;

	cMenu_Item *temp_item = NULL;

	// options image
	cHudSprite *hud_sprite = new cHudSprite( pMenuCore->m_handler->m_level->m_sprite_manager );
	hud_sprite->Set_Image( pVideo->Get_Surface( "menu/options.png" ) );
	hud_sprite->Set_Pos( game_res_w * 0.01f, 100 );
	m_draw_list.push_back( hud_sprite );

	Init_GUI();
	s_active_opts = this;
}

void cMenu_Options :: Init_GUI( void )
{
	cMenu_Base::Init_GUI();
	// Populate ModernUI state for all tabs — no CEGUI layouts loaded
	Init_GUI_Game();
	Init_GUI_Video();
	Init_GUI_Audio();
	Init_GUI_Keyboard();
	Init_GUI_Joystick();
	Init_GUI_Editor();
}

void cMenu_Options :: Init_GUI_Game( void )
{
	// Populate ModernUI state for Game tab (no CEGUI widgets created)

	// Camera speeds
	m_game_camera_hor_speed = pLevel_Manager->m_camera->m_hor_offset_speed;
	m_game_camera_ver_speed = pLevel_Manager->m_camera->m_ver_offset_speed;

	// Language list — "default" first, then discovered locale directories
	m_game_languages.clear();
	m_game_languages.push_back( "default" );
	vector<std::string> language_files = Get_Directory_Files( DATA_DIR "/" GAME_TRANSLATION_DIR, ".none", 1, 0 );
	language_files.push_back( DATA_DIR "/" GAME_TRANSLATION_DIR "/" "en" );
	for( vector<std::string>::iterator itr = language_files.begin(); itr != language_files.end(); ++itr )
	{
		std::string filename = (*itr);
		if( filename.rfind( "." ) != std::string::npos )
			continue;
		filename.erase( 0, strlen( DATA_DIR "/" GAME_TRANSLATION_DIR "/" ) );
		m_game_languages.push_back( filename );
	}
	// find current language in list
	m_game_language_idx = 0;
	if( !pPreferences->m_language.empty() )
	{
		for( int i = 0; i < static_cast<int>( m_game_languages.size() ); i++ )
		{
			if( m_game_languages[i] == pPreferences->m_language )
			{ m_game_language_idx = i; break; }
		}
	}

	// Menu level list
	m_game_menu_levels.clear();
	m_game_menu_levels.push_back( "menu_green_1" );
	m_game_menu_levels.push_back( "menu_blue_1" );
	m_game_menu_level_idx = 0;
	for( int i = 0; i < static_cast<int>( m_game_menu_levels.size() ); i++ )
	{
		if( m_game_menu_levels[i] == pPreferences->m_menu_level )
		{ m_game_menu_level_idx = i; break; }
	}
}

void cMenu_Options :: Init_GUI_Video( void )
{
	// Populate resolution list for ModernUI Select_Row (no CEGUI widgets created)
	m_vid_resolutions.clear();
	std::string current_res = int_to_string( pPreferences->m_video_screen_w ) + "x" + int_to_string( pPreferences->m_video_screen_h );
	m_vid_res_idx = 0;
	vector<cSize_Int> valid_resolutions = pVideo->Get_Supported_Resolutions();
	for( unsigned int i = 0; i < valid_resolutions.size(); i++ )
	{
		cSize_Int res = valid_resolutions[i];
		if( res.m_width <= 0 || res.m_height <= 0 )
			continue;
		std::string s = int_to_string( res.m_width ) + "x" + int_to_string( res.m_height );
		if( s == current_res )
			m_vid_res_idx = static_cast<int>( m_vid_resolutions.size() );
		m_vid_resolutions.push_back( s );
	}
}

void cMenu_Options :: Init_GUI_Audio( void )
{
	// Initialise audio state for ModernUI (no CEGUI widgets created)
	static const int hz_vals[] = { 22050, 44100, 48000 };
	m_audio_hz_idx = 1;
	for( int i = 0; i < 3; i++ )
	{
		if( pPreferences->m_audio_hz == hz_vals[i] )
		{ m_audio_hz_idx = i; break; }
	}
	m_audio_music_vol = static_cast<float>( pAudio->m_music_volume );
	m_audio_sound_vol = static_cast<float>( pAudio->m_sound_volume );
}

// Helper: rebuild the keyboard shortcut display list (name + current key name)
static void Build_Kbd_Items( std::vector<std::string> &out )
{
	out.clear();
	// pairs of (display name, SDL key pointer) — same order as the old CEGUI list
	struct KbdEntry { const char *name; SDLKey *key; };
	KbdEntry entries[] = {
		{ "Up",                    &pPreferences->m_key_up },
		{ "Down",                  &pPreferences->m_key_down },
		{ "Left",                  &pPreferences->m_key_left },
		{ "Right",                 &pPreferences->m_key_right },
		{ "Jump",                  &pPreferences->m_key_jump },
		{ "Shoot",                 &pPreferences->m_key_shoot },
		{ "Item",                  &pPreferences->m_key_item },
		{ "Action",                &pPreferences->m_key_action },
		{ "Screenshot",            &pPreferences->m_key_screenshot },
		{ "Editor copy up",        &pPreferences->m_key_editor_fast_copy_up },
		{ "Editor copy down",      &pPreferences->m_key_editor_fast_copy_down },
		{ "Editor copy left",      &pPreferences->m_key_editor_fast_copy_left },
		{ "Editor copy right",     &pPreferences->m_key_editor_fast_copy_right },
		{ "Editor pixel up",       &pPreferences->m_key_editor_pixel_move_up },
		{ "Editor pixel down",     &pPreferences->m_key_editor_pixel_move_down },
		{ "Editor pixel left",     &pPreferences->m_key_editor_pixel_move_left },
		{ "Editor pixel right",    &pPreferences->m_key_editor_pixel_move_right },
	};
	for( unsigned int i = 0; i < sizeof(entries)/sizeof(entries[0]); i++ )
	{
		std::string key_name = SDL_GetKeyName( *entries[i].key );
		out.push_back( std::string(entries[i].name) + "  [" + key_name + "]" );
	}
}

// Helper: get the SDL key pointer for keyboard shortcut at index idx
static SDLKey* Kbd_Entry_Ptr( int idx )
{
	SDLKey *ptrs[] = {
		&pPreferences->m_key_up,
		&pPreferences->m_key_down,
		&pPreferences->m_key_left,
		&pPreferences->m_key_right,
		&pPreferences->m_key_jump,
		&pPreferences->m_key_shoot,
		&pPreferences->m_key_item,
		&pPreferences->m_key_action,
		&pPreferences->m_key_screenshot,
		&pPreferences->m_key_editor_fast_copy_up,
		&pPreferences->m_key_editor_fast_copy_down,
		&pPreferences->m_key_editor_fast_copy_left,
		&pPreferences->m_key_editor_fast_copy_right,
		&pPreferences->m_key_editor_pixel_move_up,
		&pPreferences->m_key_editor_pixel_move_down,
		&pPreferences->m_key_editor_pixel_move_left,
		&pPreferences->m_key_editor_pixel_move_right,
	};
	int n = static_cast<int>( sizeof(ptrs)/sizeof(ptrs[0]) );
	if( idx < 0 || idx >= n ) return NULL;
	return ptrs[idx];
}

void cMenu_Options :: Init_GUI_Keyboard( void )
{
	// Populate ModernUI state for Keyboard tab (no CEGUI widgets created)
	m_kbd_scroll_speed = pPreferences->m_scroll_speed;
	m_kbd_selected = -1;
	m_kbd_scroll   = 0;
	Build_Kbd_Items( m_kbd_items );
}

// Helper: rebuild the joystick button display list
static void Build_Joy_Items( std::vector<std::string> &out )
{
	out.clear();
	struct JoyEntry { const char *name; Uint8 *btn; };
	JoyEntry entries[] = {
		{ "Jump",   &pPreferences->m_joy_button_jump },
		{ "Shoot",  &pPreferences->m_joy_button_shoot },
		{ "Action", &pPreferences->m_joy_button_action },
		{ "Item",   &pPreferences->m_joy_button_item },
		{ "Exit",   &pPreferences->m_joy_button_exit },
	};
	for( unsigned int i = 0; i < sizeof(entries)/sizeof(entries[0]); i++ )
		out.push_back( std::string(entries[i].name) + "  [" + int_to_string( *entries[i].btn ) + "]" );
}

// Helper: get joystick button pointer for index idx
static Uint8* Joy_Entry_Ptr( int idx )
{
	Uint8 *ptrs[] = {
		&pPreferences->m_joy_button_jump,
		&pPreferences->m_joy_button_shoot,
		&pPreferences->m_joy_button_action,
		&pPreferences->m_joy_button_item,
		&pPreferences->m_joy_button_exit,
	};
	int n = static_cast<int>( sizeof(ptrs)/sizeof(ptrs[0]) );
	if( idx < 0 || idx >= n ) return NULL;
	return ptrs[idx];
}

void cMenu_Options :: Init_GUI_Joystick( void )
{
	// Populate ModernUI state for Joystick tab (no CEGUI widgets created)

	// Build joystick name list: "None" first, then actual joystick names
	m_joy_names_list.clear();
	m_joy_names_list.push_back( _("None") );
	vector<std::string> hw_names = pJoystick->Get_Names();
	for( unsigned int i = 0; i < hw_names.size(); i++ )
		m_joy_names_list.push_back( hw_names[i] );

	// Find current selection
	m_joy_selected_idx = 0;
	if( pPreferences->m_joy_enabled )
	{
		std::string cur = pJoystick->Get_Name();
		for( int i = 1; i < static_cast<int>( m_joy_names_list.size() ); i++ )
		{
			if( m_joy_names_list[i] == cur )
			{ m_joy_selected_idx = i; break; }
		}
	}

	m_joy_sensitivity = static_cast<float>( pPreferences->m_joy_axis_threshold );
	m_joy_axis_hor    = pPreferences->m_joy_axis_hor;
	m_joy_axis_ver    = pPreferences->m_joy_axis_ver;
	m_joy_btn_selected = -1;
	m_joy_btn_scroll   = 0;
	Build_Joy_Items( m_joy_items );
}

void cMenu_Options :: Init_GUI_Editor( void )
{
	// Populate ModernUI state for Editor tab (no CEGUI widgets created)
	m_editor_item_image_size = pPreferences->m_editor_item_image_size;
}

void cMenu_Options :: Exit( void )
{
	pVideo->Set_Post_GUI_Render( NULL );
	pPreferences->Save();
	Game_Action = GA_ENTER_MENU;
#ifndef SMC_NO_CEGUI
	Game_Action_Data_Middle.add( "load_menu", int_to_string( MENU_MAIN ) );
	if( m_exit_to_gamemode != MODE_NOTHING )
	{
		Game_Action_Data_Middle.add( "menu_exit_back_to", int_to_string( m_exit_to_gamemode ) );
	}
#else
	g_android_next_menu = MENU_MAIN;
#endif
}

void cMenu_Options :: Update( void )
{
	cMenu_Base::Update();

	if( !m_action )
	{
		return;
	}

	m_action = 0;

	// only menu actions
	if( pMenuCore->m_handler->m_active > 0 )
	{
		return;
	}

	// todo : use this functionality again
	Change_Game_Setting( pMenuCore->m_handler->m_active );
	Change_Video_Setting( pMenuCore->m_handler->m_active );
	Change_Audio_Setting( pMenuCore->m_handler->m_active );
	Change_Keyboard_Setting( pMenuCore->m_handler->m_active );
	Change_Joystick_Setting( pMenuCore->m_handler->m_active );
	Change_Editor_Setting( pMenuCore->m_handler->m_active );
}

void cMenu_Options :: Change_Game_Setting( int setting )
{
	// No-op: settings handled by ModernUI widgets in Post_GUI_Draw (M12).
}

void cMenu_Options :: Change_Video_Setting( int setting )
{
	// No-op: settings handled by ModernUI widgets in Post_GUI_Draw (M12).
}

void cMenu_Options :: Change_Audio_Setting( int setting )
{
	// No-op: settings handled by ModernUI widgets in Post_GUI_Draw (M12).
}

void cMenu_Options :: Change_Keyboard_Setting( int setting )
{
	// No-op: settings handled by ModernUI widgets in Post_GUI_Draw (M12).
}

void cMenu_Options :: Change_Joystick_Setting( int setting )
{
	// No-op: settings handled by ModernUI widgets in Post_GUI_Draw (M12).
}

void cMenu_Options :: Change_Editor_Setting( int setting )
{
	// No-op: settings handled by ModernUI widgets in Post_GUI_Draw (M12).
}

void cMenu_Options :: Draw( void )
{
	// Delete font surfaces rendered in the previous frame (after Render() has flushed them)
	for( unsigned int i = 0; i < m_opt_pending_delete.size(); i++ )
		delete m_opt_pending_delete[i];
	m_opt_pending_delete.clear();

	cMenu_Base::Draw();

	// Draw the options panel and all ModernUI tab content directly (no CEGUI)
	Post_GUI_Draw();

	Draw_End();
}

void cMenu_Options :: S_Post_GUI_Draw( void )
{
	// No longer used as a CEGUI post-render callback; Post_GUI_Draw is called directly from Draw().
}

void cMenu_Options :: Post_GUI_Draw( void )
{
	// Panel layout (same geometry as before, without CEGUI frame)
	const float TAB_BAR_H = 30.0f;
	float content_x = 0.05f * game_res_w;
	float content_y = 0.26f * game_res_h;
	float content_w = 0.90f * game_res_w;
	float content_h = 0.62f * game_res_h * 0.93f;

	// Background panel
	Color bg = COL_CARD_BG;
	pVideo->Draw_Rect( content_x, content_y, content_w, content_h, 0.851f, &bg );
	Color border( static_cast<Uint8>(139), 90, 43, 200 );
	pVideo->Draw_Rect( content_x - 2, content_y - 2, content_w + 4, content_h + 4, 0.8509f, &border );

	// Tab bar — 6 tabs, ModernUI handles click-to-switch
	static const std::vector<std::string> opt_tab_labels = {
		_("Game"), _("Video"), _("Audio"), _("Keyboard"), _("Joystick"), _("Editor")
	};
	int new_tab = ModernUI::Tab_Bar( content_x, content_y, content_w, TAB_BAR_H,
	                                 opt_tab_labels, m_active_tab, m_opt_pending_delete );
	if( new_tab != m_active_tab )
		m_active_tab = new_tab;

	// Back button — top-right of panel
	float btn_back_w = 70.0f;
	float btn_back_h = 24.0f;
	float btn_back_x = content_x + content_w - btn_back_w - 4.0f;
	float btn_back_y = content_y + content_h - btn_back_h - 4.0f;
	if( ModernUI::Button( btn_back_x, btn_back_y, btn_back_w, btn_back_h, _("Back"), m_opt_pending_delete ) )
		Exit();

	int tab = m_active_tab;

	// Shift content area below the tab bar
	content_y += TAB_BAR_H;
	content_h  -= TAB_BAR_H + btn_back_h + 8.0f;

	float row_x    = content_x + 12.0f;
	float row_w    = content_w - 24.0f;
	float row_y    = content_y + 8.0f;
	float row_h    = 26.0f;
	float row_step = row_h + 6.0f;

	if( tab == 1 )  // Video
	{
		const std::vector<std::string> bpp_opts = { "16", "32" };

		int new_res_idx = ModernUI::Select_Row( row_x, row_y, row_w, _("Resolution"), m_vid_resolutions, m_vid_res_idx, m_opt_pending_delete );
		if( new_res_idx != m_vid_res_idx )
		{
			m_vid_res_idx = new_res_idx;
			const std::string &s = m_vid_resolutions[m_vid_res_idx];
			size_t xp = s.find( 'x' );
			if( xp != std::string::npos )
			{
				m_vid_w = static_cast<unsigned int>( string_to_int( s.substr( 0, xp ) ) );
				m_vid_h = static_cast<unsigned int>( string_to_int( s.substr( xp + 1 ) ) );
			}
		}
		row_y += row_step;

		int bpp_idx = (m_vid_bpp == 32) ? 1 : 0;
		int new_bpp = ModernUI::Select_Row( row_x, row_y, row_w, _("Bits Per Pixel"), bpp_opts, bpp_idx, m_opt_pending_delete );
		if( new_bpp != bpp_idx )
			m_vid_bpp = (new_bpp == 1) ? 32 : 16;
		row_y += row_step;

		bool new_fs = ModernUI::Toggle_Row( row_x, row_y, row_w, _("Fullscreen"), m_vid_fullscreen, m_opt_pending_delete );
		if( new_fs != m_vid_fullscreen ) m_vid_fullscreen = new_fs;
		row_y += row_step;

		bool new_vs = ModernUI::Toggle_Row( row_x, row_y, row_w, _("VSync"), m_vid_vsync, m_opt_pending_delete );
		if( new_vs != m_vid_vsync ) m_vid_vsync = new_vs;
		row_y += row_step;

		float new_geo = ModernUI::Slider_Row( row_x, row_y, row_w, _("Geometry Quality"), m_vid_geometry_detail, 0.0f, 1.0f, m_opt_pending_delete );
		if( new_geo != m_vid_geometry_detail ) m_vid_geometry_detail = new_geo;
		row_y += row_step;

		float new_tex = ModernUI::Slider_Row( row_x, row_y, row_w, _("Texture Quality"), m_vid_texture_detail, 0.0f, 1.0f, m_opt_pending_delete );
		if( new_tex != m_vid_texture_detail ) m_vid_texture_detail = new_tex;
		row_y += row_step + 4.0f;

		float btn_w   = 100.0f;
		float btns_w  = btn_w * 2.0f + 8.0f;
		float btn_x   = content_x + (content_w - btns_w) * 0.5f;
		if( ModernUI::Button( btn_x, row_y, btn_w, row_h, _("Apply"), m_opt_pending_delete ) )
		{
			pPreferences->Apply_Video( m_vid_w, m_vid_h, m_vid_bpp, m_vid_fullscreen, m_vid_vsync, m_vid_geometry_detail, m_vid_texture_detail );
			Game_Action = GA_ENTER_MENU;
#ifndef SMC_NO_CEGUI
			Game_Action_Data_Middle.add( "load_menu", int_to_string( MENU_OPTIONS ) );
			if( m_exit_to_gamemode != MODE_NOTHING )
				Game_Action_Data_Middle.add( "menu_exit_back_to", int_to_string( m_exit_to_gamemode ) );
#else
			g_android_next_menu = MENU_OPTIONS;
#endif
		}
		if( ModernUI::Button( btn_x + btn_w + 8.0f, row_y, btn_w, row_h, _("Reset"), m_opt_pending_delete ) )
		{
			pPreferences->Reset_Video();
			Game_Action = GA_ENTER_MENU;
#ifndef SMC_NO_CEGUI
			Game_Action_Data_Middle.add( "load_menu", int_to_string( MENU_OPTIONS ) );
			if( m_exit_to_gamemode != MODE_NOTHING )
				Game_Action_Data_Middle.add( "menu_exit_back_to", int_to_string( m_exit_to_gamemode ) );
#else
			g_android_next_menu = MENU_OPTIONS;
#endif
		}
	}
	else if( tab == 2 )  // Audio
	{
		const std::vector<std::string> hz_opts = { "22050", "44100", "48000" };
		const int hz_vals[] = { 22050, 44100, 48000 };

		int new_hz = ModernUI::Select_Row( row_x, row_y, row_w, _("Hertz (Hz)"), hz_opts, m_audio_hz_idx, m_opt_pending_delete );
		if( new_hz != m_audio_hz_idx )
		{
			m_audio_hz_idx = new_hz;
			pPreferences->m_audio_hz = hz_vals[m_audio_hz_idx];
			pAudio->Close();
			pSound_Manager->Delete_All();
			pAudio->Init();
			Preload_Sounds();
		}
		row_y += row_step;

		bool new_music = ModernUI::Toggle_Row( row_x, row_y, row_w, _("Music"), pAudio->m_music_enabled, m_opt_pending_delete );
		if( new_music != pAudio->m_music_enabled )
			pAudio->Toggle_Music();
		row_y += row_step;

		float new_mvol = ModernUI::Slider_Row( row_x, row_y, row_w, _("Music Volume"), m_audio_music_vol, 0.0f, 128.0f, m_opt_pending_delete );
		if( new_mvol != m_audio_music_vol )
		{
			m_audio_music_vol = new_mvol;
			pAudio->m_music_volume = static_cast<Uint8>( m_audio_music_vol );
			pAudio->Set_Music_Volume( pAudio->m_music_volume );
		}
		row_y += row_step;

		bool new_sound = ModernUI::Toggle_Row( row_x, row_y, row_w, _("Sound"), pAudio->m_sound_enabled, m_opt_pending_delete );
		if( new_sound != pAudio->m_sound_enabled )
			pAudio->Toggle_Sounds();
		row_y += row_step;

		float new_svol = ModernUI::Slider_Row( row_x, row_y, row_w, _("Sound Volume"), m_audio_sound_vol, 0.0f, 128.0f, m_opt_pending_delete );
		if( new_svol != m_audio_sound_vol )
		{
			m_audio_sound_vol = new_svol;
			pAudio->m_sound_volume = static_cast<Uint8>( m_audio_sound_vol );
			pAudio->Set_Sound_Volume( pAudio->m_sound_volume );
		}
		row_y += row_step + 4.0f;

		float btn_w = 100.0f;
		float btn_x = content_x + (content_w - btn_w) * 0.5f;
		if( ModernUI::Button( btn_x, row_y, btn_w, row_h, _("Reset"), m_opt_pending_delete ) )
		{
			pPreferences->Reset_Audio();
			Game_Action = GA_ENTER_MENU;
#ifndef SMC_NO_CEGUI
			Game_Action_Data_Middle.add( "load_menu", int_to_string( MENU_OPTIONS ) );
			if( m_exit_to_gamemode != MODE_NOTHING )
				Game_Action_Data_Middle.add( "menu_exit_back_to", int_to_string( m_exit_to_gamemode ) );
#else
			g_android_next_menu = MENU_OPTIONS;
#endif
		}
	}
	else if( tab == 0 )  // Game
	{
		bool new_always_run = ModernUI::Toggle_Row( row_x, row_y, row_w, _("Always Run"), pPreferences->m_always_run, m_opt_pending_delete );
		if( new_always_run != pPreferences->m_always_run )
			pPreferences->m_always_run = new_always_run;
		row_y += row_step;

		float new_hor = ModernUI::Slider_Row( row_x, row_y, row_w, _("Camera Hor Speed"), m_game_camera_hor_speed, 0.0f, 10.0f, m_opt_pending_delete );
		if( new_hor != m_game_camera_hor_speed )
		{
			m_game_camera_hor_speed = new_hor;
			pLevel_Manager->m_camera->m_hor_offset_speed = m_game_camera_hor_speed;
			pPreferences->m_camera_hor_speed = m_game_camera_hor_speed;
		}
		row_y += row_step;

		float new_ver = ModernUI::Slider_Row( row_x, row_y, row_w, _("Camera Ver Speed"), m_game_camera_ver_speed, 0.0f, 10.0f, m_opt_pending_delete );
		if( new_ver != m_game_camera_ver_speed )
		{
			m_game_camera_ver_speed = new_ver;
			pLevel_Manager->m_camera->m_ver_offset_speed = m_game_camera_ver_speed;
			pPreferences->m_camera_ver_speed = m_game_camera_ver_speed;
		}
		row_y += row_step;

		int new_lang = ModernUI::Select_Row( row_x, row_y, row_w, _("Language"), m_game_languages, m_game_language_idx, m_opt_pending_delete );
		if( new_lang != m_game_language_idx )
		{
			m_game_language_idx = new_lang;
			if( m_game_language_idx == 0 )
				pPreferences->m_language = "";
			else
				pPreferences->m_language = m_game_languages[m_game_language_idx];
		}
		row_y += row_step;

		int new_lvl = ModernUI::Select_Row( row_x, row_y, row_w, _("Menu Level"), m_game_menu_levels, m_game_menu_level_idx, m_opt_pending_delete );
		if( new_lvl != m_game_menu_level_idx )
		{
			m_game_menu_level_idx = new_lvl;
			pPreferences->m_menu_level = m_game_menu_levels[m_game_menu_level_idx];
		}
		row_y += row_step + 4.0f;

		float btn_w_g = 100.0f;
		float btn_x_g = content_x + (content_w - btn_w_g) * 0.5f;
		if( ModernUI::Button( btn_x_g, row_y, btn_w_g, row_h, _("Reset"), m_opt_pending_delete ) )
		{
			pPreferences->Reset_Game();
			Game_Action = GA_ENTER_MENU;
#ifndef SMC_NO_CEGUI
			Game_Action_Data_Middle.add( "load_menu", int_to_string( MENU_OPTIONS ) );
			if( m_exit_to_gamemode != MODE_NOTHING )
				Game_Action_Data_Middle.add( "menu_exit_back_to", int_to_string( m_exit_to_gamemode ) );
#else
			g_android_next_menu = MENU_OPTIONS;
#endif
		}
	}
	else if( tab == 3 )  // Keyboard
	{
		// Scroll speed slider at the top
		float new_spd = ModernUI::Slider_Row( row_x, row_y, row_w, _("Scroll Speed"), m_kbd_scroll_speed, 0.0f, 2.0f, m_opt_pending_delete );
		if( new_spd != m_kbd_scroll_speed )
		{
			m_kbd_scroll_speed = new_spd;
			pPreferences->m_scroll_speed = m_kbd_scroll_speed;
		}
		row_y += row_step + 4.0f;

		// Key bindings list
		float list_h = content_h - (row_y - content_y) - row_step - 8.0f;
		int result = ModernUI::Scroll_List( row_x, row_y, row_w, list_h, m_kbd_items, m_kbd_selected, m_kbd_scroll, m_opt_pending_delete );
		if( result >= 0 )
		{
			int idx = result & ~ModernUI::SCROLL_LIST_DCLICK_FLAG;
			m_kbd_selected = idx;
			if( result & ModernUI::SCROLL_LIST_DCLICK_FLAG )
			{
				// Enter "press key" mode for the selected binding
				SDLKey *key_ptr = Kbd_Entry_Ptr( idx );
				if( key_ptr )
				{
					// Determine display name (everything before "  [")
					std::string entry = m_kbd_items[idx];
					std::string name = entry.substr( 0, entry.find( "  [" ) );
					Set_Shortcut( name, key_ptr, false );
					Build_Kbd_Items( m_kbd_items );
					m_kbd_selected = -1;
				}
			}
		}
		row_y += list_h + 4.0f;

		float btn_w_k = 100.0f;
		float btn_x_k = content_x + (content_w - btn_w_k) * 0.5f;
		if( ModernUI::Button( btn_x_k, row_y, btn_w_k, row_h, _("Reset"), m_opt_pending_delete ) )
		{
			pPreferences->Reset_Keyboard();
			Game_Action = GA_ENTER_MENU;
#ifndef SMC_NO_CEGUI
			Game_Action_Data_Middle.add( "load_menu", int_to_string( MENU_OPTIONS ) );
			if( m_exit_to_gamemode != MODE_NOTHING )
				Game_Action_Data_Middle.add( "menu_exit_back_to", int_to_string( m_exit_to_gamemode ) );
#else
			g_android_next_menu = MENU_OPTIONS;
#endif
		}
	}
	else if( tab == 4 )  // Joystick
	{
		// Joystick device selection
		int new_joy = ModernUI::Select_Row( row_x, row_y, row_w, _("Joystick"), m_joy_names_list, m_joy_selected_idx, m_opt_pending_delete );
		if( new_joy != m_joy_selected_idx )
		{
			m_joy_selected_idx = new_joy;
			if( m_joy_selected_idx == 0 )
				Joy_Disable();
			else
				Joy_Default( m_joy_selected_idx - 1 );
		}
		row_y += row_step;

		// Sensitivity slider (axis threshold 0–32767)
		float new_sens = ModernUI::Slider_Row( row_x, row_y, row_w, _("Sensitivity"), m_joy_sensitivity, 0.0f, 32767.0f, m_opt_pending_delete );
		if( new_sens != m_joy_sensitivity )
		{
			m_joy_sensitivity = new_sens;
			pPreferences->m_joy_axis_threshold = static_cast<Sint16>( m_joy_sensitivity );
		}
		row_y += row_step;

		// Analog Jump toggle
		bool new_ajump = ModernUI::Toggle_Row( row_x, row_y, row_w, _("Analog Jump"), pPreferences->m_joy_analog_jump, m_opt_pending_delete );
		if( new_ajump != pPreferences->m_joy_analog_jump )
			pPreferences->m_joy_analog_jump = new_ajump;
		row_y += row_step;

		// Axis indices — use Select_Row with numeric options 0-7
		const std::vector<std::string> axis_opts = { "0","1","2","3","4","5","6","7" };
		int new_axis_hor = ModernUI::Select_Row( row_x, row_y, row_w, _("Axis Horizontal"), axis_opts,
		                                         (m_joy_axis_hor >= 0 && m_joy_axis_hor < 8) ? m_joy_axis_hor : 0,
		                                         m_opt_pending_delete );
		if( new_axis_hor != m_joy_axis_hor )
		{
			m_joy_axis_hor = new_axis_hor;
			pPreferences->m_joy_axis_hor = m_joy_axis_hor;
		}
		row_y += row_step;

		int new_axis_ver = ModernUI::Select_Row( row_x, row_y, row_w, _("Axis Vertical"), axis_opts,
		                                         (m_joy_axis_ver >= 0 && m_joy_axis_ver < 8) ? m_joy_axis_ver : 1,
		                                         m_opt_pending_delete );
		if( new_axis_ver != m_joy_axis_ver )
		{
			m_joy_axis_ver = new_axis_ver;
			pPreferences->m_joy_axis_ver = m_joy_axis_ver;
		}
		row_y += row_step + 4.0f;

		// Button binding list
		float list_h_j = content_h - (row_y - content_y) - row_step - 8.0f;
		int result_j = ModernUI::Scroll_List( row_x, row_y, row_w, list_h_j, m_joy_items, m_joy_btn_selected, m_joy_btn_scroll, m_opt_pending_delete );
		if( result_j >= 0 )
		{
			int idx = result_j & ~ModernUI::SCROLL_LIST_DCLICK_FLAG;
			m_joy_btn_selected = idx;
			if( result_j & ModernUI::SCROLL_LIST_DCLICK_FLAG )
			{
				Uint8 *btn_ptr = Joy_Entry_Ptr( idx );
				if( btn_ptr )
				{
					std::string entry = m_joy_items[idx];
					std::string name = entry.substr( 0, entry.find( "  [" ) );
					Set_Shortcut( name, btn_ptr, true );
					Build_Joy_Items( m_joy_items );
					m_joy_btn_selected = -1;
				}
			}
		}
		row_y += list_h_j + 4.0f;

		float btn_w_j = 100.0f;
		float btn_x_j = content_x + (content_w - btn_w_j) * 0.5f;
		if( ModernUI::Button( btn_x_j, row_y, btn_w_j, row_h, _("Reset"), m_opt_pending_delete ) )
		{
			pPreferences->Reset_Joystick();
			Game_Action = GA_ENTER_MENU;
#ifndef SMC_NO_CEGUI
			Game_Action_Data_Middle.add( "load_menu", int_to_string( MENU_OPTIONS ) );
			if( m_exit_to_gamemode != MODE_NOTHING )
				Game_Action_Data_Middle.add( "menu_exit_back_to", int_to_string( m_exit_to_gamemode ) );
#else
			g_android_next_menu = MENU_OPTIONS;
#endif
		}
	}
	else if( tab == 5 )  // Editor
	{
		bool new_show_imgs = ModernUI::Toggle_Row( row_x, row_y, row_w, _("Show Item Images"), pPreferences->m_editor_show_item_images, m_opt_pending_delete );
		if( new_show_imgs != pPreferences->m_editor_show_item_images )
			pPreferences->m_editor_show_item_images = new_show_imgs;
		row_y += row_step;

		bool new_auto_hide = ModernUI::Toggle_Row( row_x, row_y, row_w, _("Auto-Hide Mouse"), pPreferences->m_editor_mouse_auto_hide, m_opt_pending_delete );
		if( new_auto_hide != pPreferences->m_editor_mouse_auto_hide )
			pPreferences->m_editor_mouse_auto_hide = new_auto_hide;
		row_y += row_step;

		// Item image size: 5–60 step 5, represented as a slider
		float img_size_f = static_cast<float>( m_editor_item_image_size );
		float new_img_size = ModernUI::Slider_Row( row_x, row_y, row_w, _("Item Image Size"), img_size_f, 5.0f, 60.0f, m_opt_pending_delete );
		// snap to nearest 5
		unsigned int snapped = static_cast<unsigned int>( (new_img_size + 2.5f) / 5.0f ) * 5;
		if( snapped < 5 ) snapped = 5;
		if( snapped > 60 ) snapped = 60;
		if( snapped != m_editor_item_image_size )
		{
			m_editor_item_image_size = snapped;
			pPreferences->m_editor_item_image_size = m_editor_item_image_size;
		}
		row_y += row_step + 4.0f;

		float btn_w_e = 100.0f;
		float btn_x_e = content_x + (content_w - btn_w_e) * 0.5f;
		if( ModernUI::Button( btn_x_e, row_y, btn_w_e, row_h, _("Reset"), m_opt_pending_delete ) )
		{
			pPreferences->Reset_Editor();
			Game_Action = GA_ENTER_MENU;
#ifndef SMC_NO_CEGUI
			Game_Action_Data_Middle.add( "load_menu", int_to_string( MENU_OPTIONS ) );
			if( m_exit_to_gamemode != MODE_NOTHING )
				Game_Action_Data_Middle.add( "menu_exit_back_to", int_to_string( m_exit_to_gamemode ) );
#else
			g_android_next_menu = MENU_OPTIONS;
#endif
		}
	}

	// Flush the queued ModernUI geometry to screen
	pRenderer->Render();
}

void cMenu_Options :: Build_Shortcut_List( bool joystick /* = 0 */ )
{
	// No-op: CEGUI MultiColumnList removed in M12.
	// ModernUI keyboard/joystick tabs use Build_Kbd_Items / Build_Joy_Items directly.
}

void cMenu_Options :: Set_Shortcut( std::string name, void *data, bool joystick /* = 0 */ )
{
	std::string info_text;

	if( !joystick )
	{
		info_text += _("Press a key");
	}
	else
	{
		info_text += _("Press a button");
	}

	Draw_Static_Text( info_text + _(" for ") + name + _(". Press ESC to cancel."), &orange, NULL, 0 );

	bool sub_done = 0;

	while( !sub_done )
	{
		// no event
		if( !SDL_PollEvent( &input_event ) )
		{
			continue;
		}

		if( input_event.key.keysym.sym == SDLK_ESCAPE || input_event.key.keysym.sym == SDLK_BACKSPACE )
		{
			sub_done = 1;
			break;
		}

		if( !joystick && input_event.type != SDL_KEYDOWN )
		{
			continue;
		}
		else if( joystick && input_event.type != SDL_JOYBUTTONDOWN )
		{
			continue;
		}

		// Keyboard
		if( !joystick )
		{
			SDLKey *key = static_cast<SDLKey *>(data);
			*key = input_event.key.keysym.sym;
		}
		// Joystick
		else
		{
			Uint8 *button = static_cast<Uint8 *>(data);
			*button = input_event.jbutton.button;
		}

		sub_done = 1;
	}

	Build_Shortcut_List( joystick );
}

void cMenu_Options :: Joy_Default( unsigned int index )
{
	pPreferences->m_joy_enabled = 1;
	pPreferences->m_joy_name = SDL_JoystickNameForIndex( index );

	// initialize and if no joystick available disable
	pJoystick->Init();
}

void cMenu_Options :: Joy_Disable( void )
{
	pPreferences->m_joy_enabled = 0;
	pPreferences->m_joy_name.clear();

	// close the joystick
	pJoystick->Stick_Close();
}

// All CEGUI event handlers for cMenu_Options removed in M12.
// Settings are now handled directly in Post_GUI_Draw() via ModernUI widgets.

/* *** *** *** *** *** *** *** *** cMenu_Savegames *** *** *** *** *** *** *** *** *** */

cMenu_Savegames :: cMenu_Savegames( bool type )
: cMenu_Base()
{
	m_type_save = type;

	for( unsigned int i = 0; i < 9; i++ )
	{
		m_savegame_temp.push_back( new cHudSprite( pMenuCore->m_handler->m_level->m_sprite_manager ) );
	}
}

cMenu_Savegames :: ~cMenu_Savegames( void )
{
	for( HudSpriteList::iterator itr = m_savegame_temp.begin(); itr != m_savegame_temp.end(); ++itr )
	{
		delete *itr;
	}

	m_savegame_temp.clear();
}

void cMenu_Savegames :: Init( void )
{
	cMenu_Base::Init();
	Update_Saved_Games_Text();

	cMenu_Item *temp_item = NULL;

	// savegame descriptions
	for( HudSpriteList::iterator itr = m_savegame_temp.begin(); itr != m_savegame_temp.end(); ++itr )
	{
		temp_item = new cMenu_Item( pMenuCore->m_handler->m_level->m_sprite_manager );
		temp_item->m_image_default->Set_Image( (*itr)->m_image );
		temp_item->Set_Pos( static_cast<float>(game_res_w) / 5, m_menu_pos_y );
		pMenuCore->m_handler->Add_Menu_Item( temp_item, 1.5f, grey );
		
		m_menu_pos_y += temp_item->m_image_default->m_col_rect.m_h;
	}

	// back
	cGL_Surface *back1 = pFont->Render_Text( pFont->m_font_normal, _("Back"), m_text_color );
	temp_item = new cMenu_Item( pMenuCore->m_handler->m_level->m_sprite_manager );
	temp_item->m_image_default->Set_Image( back1 );
	temp_item->Set_Pos( static_cast<float>(game_res_w) / 18, 450 );
	temp_item->m_is_quit = 1;
	pMenuCore->m_handler->Add_Menu_Item( temp_item, 1.5f, grey );

	if( m_type_save )
	{
		cHudSprite *hud_sprite = new cHudSprite( pMenuCore->m_handler->m_level->m_sprite_manager );
		hud_sprite->Set_Image( pVideo->Get_Surface( "menu/save.png" ) );
		hud_sprite->Set_Pos( game_res_w * 0.2f, game_res_h * 0.15f );
		m_draw_list.push_back( hud_sprite );
		hud_sprite = new cHudSprite( pMenuCore->m_handler->m_level->m_sprite_manager );
		hud_sprite->Set_Image( pVideo->Get_Surface( "menu/items/save.png" ) );
		hud_sprite->Set_Pos( game_res_w * 0.07f, game_res_h * 0.24f );
		m_draw_list.push_back( hud_sprite );
	}
	else
	{
		cHudSprite *hud_sprite = new cHudSprite( pMenuCore->m_handler->m_level->m_sprite_manager );
		hud_sprite->Set_Image( pVideo->Get_Surface( "menu/load.png" ) );
		hud_sprite->Set_Pos( game_res_w * 0.2f, game_res_h * 0.15f );
		m_draw_list.push_back( hud_sprite );
		hud_sprite = new cHudSprite( pMenuCore->m_handler->m_level->m_sprite_manager );
		hud_sprite->Set_Image( pVideo->Get_Surface( "menu/items/load.png" ) );
		hud_sprite->Set_Pos( game_res_w * 0.07f, game_res_h * 0.24f );
		m_draw_list.push_back( hud_sprite );
	}

	cHudSprite *hud_sprite = new cHudSprite( pMenuCore->m_handler->m_level->m_sprite_manager );
	hud_sprite->Set_Image( back1, 0, 1 );
	hud_sprite->Set_Pos( -200, 0 );
	m_draw_list.push_back( hud_sprite );

	Init_GUI();
}

void cMenu_Savegames :: Init_GUI( void )
{
	cMenu_Base::Init_GUI();
}

void cMenu_Savegames :: Exit( void )
{
	Game_Action = GA_ENTER_MENU;
#ifndef SMC_NO_CEGUI
	Game_Action_Data_Middle.add( "load_menu", int_to_string( MENU_MAIN ) );
	if( m_exit_to_gamemode != MODE_NOTHING )
	{
		Game_Action_Data_Middle.add( "menu_exit_back_to", int_to_string( m_exit_to_gamemode ) );
	}
#else
	g_android_next_menu = MENU_MAIN;
#endif
}

void cMenu_Savegames :: Update( void )
{
	cMenu_Base::Update();

	if( !m_action )
	{
		return;
	}

	m_action = 0;

	// back
	if( pMenuCore->m_handler->m_active == 9 )
	{
		Exit();
		return;
	}

	if( !m_type_save )
	{
		Update_Load();
	}
	else
	{
		Update_Save();
	}
}

void cMenu_Savegames :: Draw( void )
{
	cMenu_Base::Draw();
	Draw_End();
}

void cMenu_Savegames :: Update_Load( void )
{
	int save_num = pMenuCore->m_handler->m_active + 1;

	// not valid
	if( !pSavegame->Is_Valid( save_num ) )
	{
		return;
	}

	pAudio->Play_Sound( "savegame_load.ogg" );
	// reset before loading the level to keep the level in the manager
	pLevel_Player->Reset_Save();

	cSave *savegame = pSavegame->Load( save_num );
	std::string level_name = savegame->Get_Active_Level();
	delete savegame;

	if( !level_name.empty() )
	{
		Game_Action = GA_ENTER_LEVEL;
		cLevel *level = pLevel_Manager->Load( level_name );
		// only fade-out music if different
#ifndef SMC_NO_CEGUI
		if( pActive_Level->Get_Music_Filename( 1 ).compare( level->Get_Music_Filename( 1 ) ) != 0 )
		{
			Game_Action_Data_Start.add( "music_fadeout", "1000" );
		}
#else
		(void)level;
#endif
	}
	else
	{
		Game_Action = GA_ENTER_WORLD;
#ifndef SMC_NO_CEGUI
		Game_Action_Data_Start.add( "music_fadeout", "1000" );
#endif
	}

#ifndef SMC_NO_CEGUI
	Game_Action_Data_Start.add( "screen_fadeout", int_to_string( EFFECT_OUT_BLACK ) );
	Game_Action_Data_Start.add( "screen_fadeout_speed", "3" );
	Game_Action_Data_Middle.add( "unload_menu", "1" );
	Game_Action_Data_Middle.add( "load_savegame", int_to_string( save_num ) );
	Game_Action_Data_End.add( "screen_fadein", int_to_string( EFFECT_IN_BLACK ) );
	Game_Action_Data_End.add( "screen_fadein_speed", "3" );
#endif
}

void cMenu_Savegames :: Update_Save( void )
{
	// not valid
	if( pOverworld_Player->m_current_waypoint < 0 && !pActive_Level->Is_Loaded() )
	{
		return;
	}

	std::string descripion = Set_Save_Description( pMenuCore->m_handler->m_active + 1 );
	
	pFramerate->Reset();

	if( descripion.compare( "Not enough Points" ) == 0 )
	{
		Game_Action = GA_ENTER_MENU;
#ifndef SMC_NO_CEGUI
		Game_Action_Data_Middle.add( "load_menu", int_to_string( MENU_MAIN ) );
		if( m_exit_to_gamemode != MODE_NOTHING )
		{
			Game_Action_Data_Middle.add( "menu_exit_back_to", int_to_string( m_exit_to_gamemode ) );
		}
#else
		g_android_next_menu = MENU_MAIN;
#endif
		return;
	}

	if( descripion.empty() )
	{
		return;
	}

	pAudio->Play_Sound( "savegame_save.ogg" );

	// no costs in debug builds
#ifndef _DEBUG
	if( pActive_Level->Is_Loaded() )
	{
		pHud_Points->Set_Points( pLevel_Player->m_points - 3000 );
	}
#endif
	// save
	pSavegame->Save_Game( pMenuCore->m_handler->m_active + 1, descripion );

	Game_Action = GA_ENTER_MENU;
#ifndef SMC_NO_CEGUI
	Game_Action_Data_Middle.add( "load_menu", int_to_string( MENU_MAIN ) );
	if( m_exit_to_gamemode != MODE_NOTHING )
	{
		Game_Action_Data_Middle.add( "menu_exit_back_to", int_to_string( m_exit_to_gamemode ) );
	}
#else
	g_android_next_menu = MENU_MAIN;
#endif
}

std::string cMenu_Savegames :: Set_Save_Description( unsigned int save_slot )
{
	if( save_slot == 0 || save_slot > 9 )
	{
		return "";
	}
// save always in debug builds
#ifndef _DEBUG
	if( pActive_Level->Is_Loaded() && pLevel_Player->m_points < 3000 )
	{
		Clear_Input_Events();
		Draw_Static_Text( _("3000 Points needed for saving in a level.\nSaving on the Overworld is free.") );

		return "Not enough Points";
	}
#endif
	std::string save_description;

	bool auto_erase_description = 0;

	// if Savegame exists use old description
	if( pSavegame->Is_Valid( save_slot ) )
	{
		save_description.clear();
		// get only the description
		save_description = pSavegame->Get_Description( save_slot, 1 );
	}
	else
	{
		// use default description
		save_description = _("No Description");
		auto_erase_description = 1;
	}

	return Box_Text_Input( save_description, _("Enter Description"), auto_erase_description );
}

void cMenu_Savegames :: Update_Saved_Games_Text( void )
{
	unsigned int save_slot = 0;

	for( HudSpriteList::iterator itr = m_savegame_temp.begin(); itr != m_savegame_temp.end(); ++itr )
	{
		save_slot++;
		(*itr)->Set_Image( pFont->Render_Text( pFont->m_font_normal, pSavegame->Get_Description( save_slot ), m_text_color_value ), 1, 1 );
	}
}

/* *** *** *** *** *** *** *** *** cMenu_Credits *** *** *** *** *** *** *** *** *** */

cMenu_Credits :: cMenu_Credits( void )
: cMenu_Base()
{

}

cMenu_Credits :: ~cMenu_Credits( void )
{

}

void cMenu_Credits :: Init( void )
{
	cMenu_Base::Init();

	// clear credits
	m_draw_list.clear();

	// add credits text
	Add_Credits_Line( "Florian Richter (FluXy)", 0, 20, black, 1.0f );
	Add_Credits_Line( " - Project Leader", 0, -3 );
	Add_Credits_Line( " - Dedicated Developer", 0, -3 );

	Add_Credits_Line( "Robert W... (BowserJr)", 0, 20, lightgreen, 1.0f );
	Add_Credits_Line( " - Forum and Wiki Moderator", 0, -3 );
	Add_Credits_Line( " - Game Tester", 0, -3 );

	Add_Credits_Line( "Anthony Smith (mrvertigo27)", 0, 20, Color( 0.58f, 0.52f, 1.0f ), 1.0f );
	Add_Credits_Line( " - Graphic Designer", 0, -3 );

	Add_Credits_Line( "Fabian ... (Fabianius)", 0, 20, Color( 0.5f, 0.9f, 0.0f ), 1.0f );
	Add_Credits_Line( " - Graphic Designer", 0, -3 );

	// Most Valued Persons (MVP)
	Add_Credits_Line( "-- Most Valued Persons (MVP) --", 0, 20, lightgrey, 1.0f );

	Add_Credits_Line( "... (Crabmaster)", 0, 20, Color( 0.8f, 0.35f, 0.25f ), 1.0f );
	Add_Credits_Line( " - Graphic Designer", 0, -3 );

	Add_Credits_Line( "Norbu Tsering (Naerbu)", 0, 20, Color( 0.8f, 0.0f, 0.0f ), 1.0f );
	Add_Credits_Line( " - Music Artist", 0, -3 );

	Add_Credits_Line( "Tristan Heaven (nyhm)", 0, 20, lightblue, 1.0f );
	Add_Credits_Line( " - Gentoo eBuild Maintainer", 0, -3 );

	Add_Credits_Line( "Muammar El Khatib (muammar)", 0, 20, lightred, 1.0f );
	Add_Credits_Line( " - Debian Package Maintainer", 0, -3 );

	Add_Credits_Line( "... (Yoshis_Fan)", 0, 20, Color( 0.8f, 1.0f, 0.4f ), 1.0f );
	Add_Credits_Line( " - Graphic Designer", 0, -3 );

	Add_Credits_Line( "Holger Fey (Nemo)", 0, 20, lila, 1.0f );
	Add_Credits_Line( " - Game Tester", 0, -3 );
	Add_Credits_Line( " - German Publicity", 0, -3 );
	Add_Credits_Line( " - Torrent Packager", 0, -3 );

	// Retired
	Add_Credits_Line( "-- Retired --", 0, 20, lightgrey, 1.0f );

	Add_Credits_Line( "Grant ... (youngheart80)", 0, 20, green, 1.0f );
	Add_Credits_Line( " - Graphic Designer", 0, -3 );

	Add_Credits_Line( "... (Sauer2)", 0, 20, Color( 0.1f, 0.6f, 0.1f ), 1.0f );
	Add_Credits_Line( " - Level Contributor", 0, -3 );

	Add_Credits_Line( "... (Simpletoon)", 0, 20, Color( 0.2f, 0.2f, 0.8f ), 1.0f );
	Add_Credits_Line( " - Developer", 0, -3 );

	Add_Credits_Line( "David Hernandez (vencabot_teppoo)", 0, 20, Color( 0.8f, 0.6f, 0.2f ), 1.0f );
	Add_Credits_Line( " - Music Artist", 0, -3 );

	Add_Credits_Line( "Markus Hiebl (Frostbringer)", 0, 20, Color( 0.9f, 0.1f, 0.8f ), 1.0f );
	Add_Credits_Line( " - Graphic Designer", 0, -3 );
	Add_Credits_Line( " - Level Contributor", 0, -3 );

	Add_Credits_Line( "... (Helios)", 0, 20, Color( 0.8f, 0.8f, 0.1f ), 1.0f );
	Add_Credits_Line( " - Graphic Designer", 0, -3 );

	Add_Credits_Line( "Mark Richards (dteck)", 0, 20, blue, 1.0f );
	Add_Credits_Line( " - Graphic Designer", 0, -3 );

	Add_Credits_Line( "Mario Fink (maYO)", 0, 20, blue, 1.0f );
	Add_Credits_Line( " - Graphic Designer", 0, -3 );
	Add_Credits_Line( " - Website Graphic Designer", 0, -3 );
	Add_Credits_Line( " - Other Support", 0, -3 );

	Add_Credits_Line( "... (Polilla86)", 0, 20, Color( 0.7f, 0.1f, 0.2f ), 1.0f );
	Add_Credits_Line( " - Graphic Designer", 0, -3 );

	Add_Credits_Line( "Ursula ... (Pipgirl)", 0, 20, Color( 0.2f, 0.9f, 0.2f ), 1.0f );
	Add_Credits_Line( " - Graphic Designer", 0, -3 );

	Add_Credits_Line( "Tobias Maasland (Weirdnose)", 0, 20, Color( 0.9f, 0.7f, 0.2f ), 1.0f );
	Add_Credits_Line( " - Level and World Contributor", 0, -3 );
	Add_Credits_Line( " - Assistant Developer", 0, -3 );

	Add_Credits_Line( "Robert ... (Consonance)", 0, 20, lightred, 1.0f );
	Add_Credits_Line( " - Sound and Music Artist", 0, -3 );

	Add_Credits_Line( "Justin ... (LoXodonte)", 0, 20, lightblue, 1.0f );
	Add_Credits_Line( " - Music Artist", 0, -3 );

	Add_Credits_Line( "Matt J... (mattwj)", 0, 20, red, 1.0f );
	Add_Credits_Line( " - eDonkey Packager", 0, -3 );
	Add_Credits_Line( " - Quality Assurance", 0, -3 );

	Add_Credits_Line( "Bodhi Crandall-Rus (Boder)", 0, 20, green, 1.0f );
	Add_Credits_Line( " - All Hands Person", 0, -3 );
	Add_Credits_Line( " - Game Tester", 0, -3 );
	Add_Credits_Line( " - Assistant Graphic Designer", 0, -3 );

	Add_Credits_Line( "John Daly (Johnlein)", 0, 20, yellow, 1.0f );
	Add_Credits_Line( " - Graphic Designer", 0, -3 );

	Add_Credits_Line( "Gustavo Gutierrez (Enzakun)", 0, 20, lightred, 1.0f );
	Add_Credits_Line( " - Maryo Graphic Designer", 0, -3 );

	Add_Credits_Line( "Thomas Huth (Thothy)", 0, 20, greenyellow, 1.0f );
	Add_Credits_Line( " - Linux Maintainer", 0, -3 );

	// Thanks
	Add_Credits_Line( "-- Thanks --", 0, 20, lightblue, 1.0f );

	Add_Credits_Line( "Jason Cox (XOC)", 0, 0 );
	Add_Credits_Line( "Ricardo Cruz", 0, 0 );
	Add_Credits_Line( "Devendra (Yuki),", 0, 0 );
	Add_Credits_Line( "Hans de Goede (Hans)", 0, 0 );
	Add_Credits_Line( "... (xPatrickx)", 0, 0 );
	Add_Credits_Line( "Rolando Gonzalez (rolosworld)", 0, 0 );

	m_menu_pos_y = game_res_h * 1.1f;

	// set credits position
	for( HudSpriteList::iterator itr = m_draw_list.begin(); itr != m_draw_list.end(); ++itr )
	{
		// get object
		cHudSprite *obj = (*itr);

		// set shadow if not set
		if( obj->m_shadow_pos == 0 )
		{
			obj->Set_Shadow( grey, 1 );
		}
		// set position
		obj->m_pos_x += static_cast<float>(game_res_w) / 5;
		obj->m_pos_y += m_menu_pos_y;
		// set posz behind front passive
		obj->m_pos_z = 0.096f;
		// set color combine
		obj->Set_Color_Combine( 0, 0, 0, GL_MODULATE );
		obj->m_color.alpha = 0;
		obj->m_shadow_color.alpha = 0;

		m_menu_pos_y = obj->m_pos_y + obj->m_col_rect.m_h;
	}

	cMenu_Item *temp_item = NULL;

	// back
	cGL_Surface *back1 = pFont->Render_Text( pFont->m_font_normal, _("Back"), m_text_color );
	temp_item = new cMenu_Item( pMenuCore->m_handler->m_level->m_sprite_manager );
	temp_item->m_image_default->Set_Image( back1 );
	temp_item->Set_Pos( static_cast<float>(game_res_w) / 18, 250 );
	temp_item->m_is_quit = 1;
	pMenuCore->m_handler->Add_Menu_Item( temp_item, 2, grey );
	
	cHudSprite *hud_sprite = new cHudSprite( pMenuCore->m_handler->m_level->m_sprite_manager );
	hud_sprite->Set_Image( back1, 0, 1 );
	hud_sprite->Set_Pos( -200, 0 );
	m_draw_list.push_back( hud_sprite );

	Init_GUI();
}

void cMenu_Credits :: Init_GUI( void )
{
	cMenu_Base::Init_GUI();
}

void cMenu_Credits :: Enter( const GameMode old_mode /* = MODE_NOTHING */ )
{
	// black background because of fade alpha
	glClearColor( 0, 0, 0, 1 );

	if( old_mode == MODE_MENU )
	{
		// fade in
		Menu_Fade();
	}
}

void cMenu_Credits :: Leave( const GameMode next_mode /* = MODE_NOTHING */ )
{
	if( m_exit_to_gamemode == MODE_NOTHING || m_exit_to_gamemode == MODE_MENU )
	{
		// fade out
		Menu_Fade( 0 );

		// white background
		glClearColor( 1, 1, 1, 1 );
	}

	// set menu gradient colors back
	pMenuCore->m_handler->m_level->m_background_manager->Get_Pointer( 0 )->m_color_1.alpha = 255;
	pMenuCore->m_handler->m_level->m_background_manager->Get_Pointer( 0 )->m_color_2.alpha = 255;
}

void cMenu_Credits :: Exit( void )
{
	Game_Action = GA_ENTER_MENU;
#ifndef SMC_NO_CEGUI
	Game_Action_Data_Start.add( "music_fadeout", "500" );
	Game_Action_Data_Middle.add( "load_menu", int_to_string( MENU_MAIN ) );
	if( m_exit_to_gamemode != MODE_NOTHING )
	{
		Game_Action_Data_Middle.add( "menu_exit_back_to", int_to_string( m_exit_to_gamemode ) );
	}
#else
	g_android_next_menu = MENU_MAIN;
#endif
}

void cMenu_Credits :: Update( void )
{
	cMenu_Base::Update();

	for( HudSpriteList::iterator itr = m_draw_list.begin(); itr != m_draw_list.end(); ++itr )
	{
		cHudSprite *obj = (*itr);

		// long inactive reset
		if( obj->m_pos_y < -2700 )
		{
			obj->Set_Pos_Y( static_cast<float>(game_res_h) * 1.1f );
		}
		// fading out
		else if( obj->m_pos_y < game_res_h * 0.3f )
		{
			float new_value = obj->m_combine_color[0] - ( pFramerate->m_speed_factor * 0.01f );

			if( new_value < 0 )
			{
				new_value = 0;
			}

			obj->Set_Color_Combine( new_value, new_value, new_value, GL_MODULATE );
			obj->m_color.alpha = static_cast<Uint8>( new_value * 255 );
			obj->m_shadow_color.alpha = obj->m_color.alpha;
		}
		// fading in
		else if( obj->m_pos_y < game_res_h * 0.9f )
		{
			float new_value = obj->m_combine_color[0] + ( pFramerate->m_speed_factor * 0.01f );

			if( new_value > 1.0f )
			{
				new_value = 1.0f;

				// add particles
				if( obj->m_combine_color[0] < 1.0f )
				{
					cParticle_Emitter *anim = new cParticle_Emitter( pMenuCore->m_handler->m_level->m_sprite_manager );
					anim->Set_Emitter_Rect( Get_Random_Float( game_res_w * 0.1f, game_res_w * 0.8f ), -Get_Random_Float( game_res_h * 0.8f, game_res_h * 0.9f ), Get_Random_Float( 0.0f, 5.0f ), Get_Random_Float( 0.0f, 5.0f ) );
					unsigned int quota = 4;
					
					// multi-explosion
					if( rand() % 2 )
					{
						anim->Set_Image_Filename( "animation/particles/fire_2.png" );
						anim->Set_Emitter_Time_to_Live( 0.4f );
						anim->Set_Emitter_Iteration_Interval( 0.05f );
						anim->Set_Direction_Range( 0, 360 );
						anim->Set_Scale( 0.3f, 0.2f );
						anim->Set_Blending( BLEND_ADD );
						anim->Set_Time_to_Live( 1.8f, 1.2f );
						anim->Set_Speed( 2.1f, 0.5f );
					}
					// star explosion
					else
					{
						quota += rand() % 25;
						anim->Set_Image_Filename( "animation/particles/fire_3.png" );
						anim->Set_Direction_Range( 0, 360 );
						anim->Set_Scale( 0.2f, 0.1f );

						if( quota < 10 )
						{
							anim->Set_Time_to_Live( 2.8f, 0.5f );
							anim->Set_Speed( 0.8f, 0.3f );
						}
						else
						{
							anim->Set_Time_to_Live( 1.4f, 0.5f );
							anim->Set_Fading_Size( 1 );
							anim->Set_Speed( 1.6f, 0.5f );
						}
					}
					
					anim->Set_Quota( quota );
					anim->Set_Color( Color( static_cast<Uint8>( 100 + ( rand() % 155 ) ), 100 + ( rand() % 155 ), 100 + ( rand() % 155 ) ) );
					anim->Set_Const_Rotation_Z( -5, 10 );
					anim->Set_Vertical_Gravity( 0.02f );
					anim->Set_Pos_Z( 0.16f );
					anim->Emit();
					pMenuCore->m_animation_manager->Add( anim );
				}
			}

			obj->Set_Color_Combine( new_value, new_value, new_value, GL_MODULATE );
			obj->m_color.alpha = static_cast<Uint8>( new_value * 255 );
			obj->m_shadow_color.alpha = obj->m_color.alpha;
		}

		// default upwards scroll
		obj->Move( 0, -1.1f );
	}

	if( !m_action )
	{
		return;
	}

	m_action = 0;

	// back
	if( pMenuCore->m_handler->m_active == 0 )
	{
		Exit();
	}
}

void cMenu_Credits :: Draw( void )
{
	// do not draw if exiting
	if( Game_Action != GA_NONE )
	{
		return;
	}

	cMenu_Base::Draw();

	// darken background
	cRect_Request *request = new cRect_Request();
	pVideo->Draw_Rect( NULL, 0.095f, &pMenuCore->m_handler->m_level->m_background_manager->Get_Pointer( 0 )->m_color_2, request );
	request->m_color.red = static_cast<Uint8>(request->m_color.red * 0.1f);
	request->m_color.green = static_cast<Uint8>(request->m_color.green * 0.1f);
	request->m_color.blue = static_cast<Uint8>(request->m_color.blue * 0.1f);
	request->m_color.alpha = 195;
	pRenderer->Add( request );

	Draw_End();
}

void cMenu_Credits :: Add_Credits_Line( const std::string &text, float posx, float posy, const Color &shadow_color /* = black */, float shadow_pos /* = 0.0f */ )
{
	cHudSprite *temp = new cHudSprite( pMenuCore->m_handler->m_level->m_sprite_manager );
	temp->Set_Image( pFont->Render_Text( pFont->m_font_normal, text, white ), 1, 1 );
	temp->Set_Pos( posx, posy );
	if( !Is_Float_Equal( shadow_pos, 0.0f ) )
	{
		temp->Set_Shadow( shadow_color, shadow_pos );
	}
	m_draw_list.push_back( temp );
}

void cMenu_Credits :: Menu_Fade( bool fade_in /* = 1 */ )
{
	// logo position y
	int logo_pos_y = 0;
	// fade counter
	float counter;
	// move speed
	float move_speed;

	if( fade_in )
	{
		logo_pos_y = 20;
		counter = 255.0f;
		move_speed = -2.0f;
	}
	else
	{
		logo_pos_y = -200;
		counter = 60.0f;
		move_speed = 2.0f;
	}

	// get logo
	cSprite *logo = pMenuCore->m_handler->m_level->m_sprite_manager->Get_from_Position( 180, logo_pos_y, TYPE_FRONT_PASSIVE, 2 );

	// fade out
	while( 1 )
	{
		// # Update

		if( fade_in )
		{
			counter -= 4.5f * pFramerate->m_speed_factor;
			move_speed -= 1.0f * pFramerate->m_speed_factor;

			if( counter < 60.0f )
			{
				break;
			}

			// move logo out
			if( logo && logo->m_pos_y > -200.0f )
			{
				logo->Move( 0.0f, move_speed );

				if( logo->m_pos_y < -200.0f )
				{
					logo->Set_Pos_Y( -200.0f );
				}
			}
		}
		else
		{
			counter += 5.0f * pFramerate->m_speed_factor;
			move_speed += 1.0f * pFramerate->m_speed_factor;

			if( counter > 255.0f )
			{
				break;
			}

			// move logo in
			if( logo && logo->m_pos_y < 20.0f )
			{
				logo->Move( 0.0f, move_speed );

				if( logo->m_pos_y > 20.0f )
				{
					logo->Set_Pos_Y( 20.0f );
				}
			}
		}

		// set menu gradient colors
		pMenuCore->m_handler->m_level->m_background_manager->Get_Pointer( 0 )->m_color_1.alpha = static_cast<Uint8>(counter);
		pMenuCore->m_handler->m_level->m_background_manager->Get_Pointer( 0 )->m_color_2.alpha = static_cast<Uint8>(counter);

		// # Draw

		// clear
		pVideo->Clear_Screen();

		// draw menu
		pMenuCore->m_handler->Draw();
		pMenuCore->m_animation_manager->Draw();

		// create request
		cRect_Request *request = new cRect_Request();
		pVideo->Draw_Rect( NULL, 0.095f, &pMenuCore->m_handler->m_level->m_background_manager->Get_Pointer( 0 )->m_color_2, request );
		request->m_color.red = static_cast<Uint8>(request->m_color.red * 0.1f);
		request->m_color.green = static_cast<Uint8>(request->m_color.green * 0.1f);
		request->m_color.blue = static_cast<Uint8>(request->m_color.blue * 0.1f);
		request->m_color.alpha = 255 - static_cast<Uint8>(counter);
		// add request
		pRenderer->Add( request );

		pVideo->Render();

		// # framerate
		pFramerate->Update();
		// if vsync is disabled then limit the fps to reduce the CPU usage
		if( !pPreferences->m_video_vsync )
		{
			Correct_Frame_Time( 100 );
		}
	}
}

/* *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** */

} // namespace SMC
