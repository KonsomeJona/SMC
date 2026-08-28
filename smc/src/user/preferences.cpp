/***************************************************************************
 * preferences.cpp  -  Game settings handler
 *
 * Copyright (C) 2003 - 2011 Florian Richter
 ***************************************************************************/
/*
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "../user/preferences.h"
#include "../audio/audio.h"
#include "../video/video.h"
#include "../core/game_core.h"
#include "../input/joystick.h"
#include "../gui/hud.h"
#include "../level/level_manager.h"
#include "../core/i18n.h"
#include "../core/filesystem/resource_manager.h"
#include "../core/filesystem/filesystem.h"
#include "../core/debug_log.h"
// TinyXML2 (bundled)
#include "../core/tinyxml2.h"

namespace SMC
{

/* *** *** *** *** *** *** *** cPreferences *** *** *** *** *** *** *** *** *** *** */

// Game
const bool cPreferences::m_always_run_default = 0;
const bool cPreferences::m_touch_vibration_default = 1;
const float cPreferences::m_touch_opacity_default = 0.45f;
const float cPreferences::m_touch_scale_default = 1.0f;
const std::string cPreferences::m_menu_level_default = "menu_green_1";
const float cPreferences::m_camera_hor_speed_default = 0.3f;
const float cPreferences::m_camera_ver_speed_default = 0.2f;
// Video
#ifdef _DEBUG
const bool cPreferences::m_video_fullscreen_default = 0;
#else
const bool cPreferences::m_video_fullscreen_default = 1;
#endif
const Uint16 cPreferences::m_video_screen_w_default = 1024;
const Uint16 cPreferences::m_video_screen_h_default = 768;
const Uint8 cPreferences::m_video_screen_bpp_default = 32;
/* disable by default because of possible bad drivers
 * which can't handle visual sync
*/
const bool cPreferences::m_video_vsync_default = 0;
const Uint16 cPreferences::m_video_fps_limit_default = 240;
// default geometry detail is medium
const float cPreferences::m_geometry_quality_default = 0.5f;
// default texture detail is high
const float cPreferences::m_texture_quality_default = 0.75f;
// Audio
const bool cPreferences::m_audio_music_default = 1;
const bool cPreferences::m_audio_sound_default = 1;
const unsigned int cPreferences::m_audio_hz_default = 44100;
const Uint8 cPreferences::m_sound_volume_default = 100;
const Uint8 cPreferences::m_music_volume_default = 80;
// Keyboard
const SDLKey cPreferences::m_key_up_default = SDLK_UP;
const SDLKey cPreferences::m_key_down_default = SDLK_DOWN;
const SDLKey cPreferences::m_key_left_default = SDLK_LEFT;
const SDLKey cPreferences::m_key_right_default = SDLK_RIGHT;
const SDLKey cPreferences::m_key_jump_default = SDLK_s;
const SDLKey cPreferences::m_key_shoot_default = SDLK_SPACE;
const SDLKey cPreferences::m_key_item_default = SDLK_RETURN;
const SDLKey cPreferences::m_key_action_default = SDLK_a;
const SDLKey cPreferences::m_key_screenshot_default = SDLK_PRINT;
const SDLKey cPreferences::m_key_editor_fast_copy_up_default = SDLK_KP8;
const SDLKey cPreferences::m_key_editor_fast_copy_down_default = SDLK_KP2;
const SDLKey cPreferences::m_key_editor_fast_copy_left_default = SDLK_KP4;
const SDLKey cPreferences::m_key_editor_fast_copy_right_default = SDLK_KP6;
const SDLKey cPreferences::m_key_editor_pixel_move_up_default = SDLK_KP8;
const SDLKey cPreferences::m_key_editor_pixel_move_down_default = SDLK_KP2;
const SDLKey cPreferences::m_key_editor_pixel_move_left_default = SDLK_KP4;
const SDLKey cPreferences::m_key_editor_pixel_move_right_default = SDLK_KP6;
const float cPreferences::m_scroll_speed_default = 1.0f;
// Joystick
const bool cPreferences::m_joy_enabled_default = 1;
const bool cPreferences::m_joy_analog_jump_default = 0;
const int cPreferences::m_joy_axis_hor_default = 0;
const int cPreferences::m_joy_axis_ver_default = 1;
const Sint16 cPreferences::m_joy_axis_threshold_default = 10000;
const Uint8 cPreferences::m_joy_button_jump_default = 0;
const Uint8 cPreferences::m_joy_button_shoot_default = 1;
const Uint8 cPreferences::m_joy_button_item_default = 3;
const Uint8 cPreferences::m_joy_button_action_default = 2;
const Uint8 cPreferences::m_joy_button_exit_default = 4;
// Editor
const bool cPreferences::m_editor_mouse_auto_hide_default = 0;
const bool cPreferences::m_editor_show_item_images_default = 1;
const unsigned int cPreferences::m_editor_item_image_size_default = 50;

cPreferences :: cPreferences( void )
{
	Reset_All();
}

cPreferences :: ~cPreferences( void )
{
	//
}

bool cPreferences :: Load( const std::string &filename /* = "" */ )
{
	Reset_All();
	
	// if config file is given
	if( filename.length() )
	{
		m_config_filename = filename;
	}

	// prefer local config file
	if( File_Exists( m_config_filename ) )
	{
		printf( "Using local preferences file : %s\n", m_config_filename.c_str() );
	}
	// user dir
	else
	{
		m_config_filename.insert( 0, pResource_Manager->user_data_dir );

		// does not exist in user dir
		if( !File_Exists( m_config_filename ) )
		{
			// only print warning if file is given
			if( !filename.empty() )
			{
				printf( "Couldn't open preferences file : %s\n", m_config_filename.c_str() );
			}
			return 0;
		}
	}

	tinyxml2::XMLDocument doc;
	tinyxml2::XMLError err = doc.LoadFile( m_config_filename.c_str() );
	if( err != tinyxml2::XML_SUCCESS )
	{
		printf( "Preferences Loading failed: TinyXML2 error %d (%s)\n", (int)err, doc.ErrorStr() );
	}
	else
	{
		tinyxml2::XMLElement *root = doc.FirstChildElement( "config" );
		if( root )
		{
			for( tinyxml2::XMLElement *el = root->FirstChildElement(); el; el = el->NextSiblingElement() )
			{
				const char *ename = el->Name();
				if( ename && (strcmp(ename, "property") == 0 || strcmp(ename, "Item") == 0) )
				{
					const char *name  = el->Attribute( "name"  );
					const char *value = el->Attribute( "value" );
					if( !name )
					{
						// V.1.9 and lower used "Name"/"Value"
						name  = el->Attribute( "Name"  );
						value = el->Attribute( "Value" );
					}
					if( name && value )
					{
						handle_item( name, value );
					}
				}
			}
		}
	}

	// if user data dir is set
	if( !m_force_user_data_dir.empty() )
	{
		pResource_Manager->Set_User_Directory( m_force_user_data_dir );
	}

	LOG_DEBUG(GAME, "Preferences loaded: resolution=%dx%dx%d fullscreen=%d vsync=%d fps_limit=%d",
		m_video_screen_w, m_video_screen_h, m_video_screen_bpp, m_video_fullscreen, m_video_vsync, m_video_fps_limit);
	LOG_DEBUG(GAME, "Preferences audio: music=%d sound=%d hz=%d",
		m_audio_music, m_audio_sound, m_audio_hz);
	LOG_DEBUG(GAME, "Preferences keys: up=%d down=%d left=%d right=%d jump=%d shoot=%d action=%d",
		m_key_up, m_key_down, m_key_left, m_key_right, m_key_jump, m_key_shoot, m_key_action);
	LOG_DEBUG(GAME, "Preferences misc: language=%s always_run=%d camera_hor=%.1f camera_ver=%.1f",
		m_language.c_str(), m_always_run, m_camera_hor_speed, m_camera_ver_speed);

	return 1;
}

void cPreferences :: Save( void )
{
	Update();

	tinyxml2::XMLDocument doc;
	doc.InsertFirstChild( doc.NewDeclaration() );

	tinyxml2::XMLElement *root = doc.NewElement( "config" );
	doc.InsertEndChild( root );

	// Helper lambdas to append <property name="..." value="..."> children
	auto add_str = [&]( const char *name, const std::string &val )
	{
		tinyxml2::XMLElement *el = doc.NewElement( "property" );
		el->SetAttribute( "name", name );
		el->SetAttribute( "value", val.c_str() );
		root->InsertEndChild( el );
	};
	auto add_int = [&]( const char *name, int val )
	{
		tinyxml2::XMLElement *el = doc.NewElement( "property" );
		el->SetAttribute( "name", name );
		el->SetAttribute( "value", val );
		root->InsertEndChild( el );
	};
	auto add_float = [&]( const char *name, float val )
	{
		tinyxml2::XMLElement *el = doc.NewElement( "property" );
		el->SetAttribute( "name", name );
		el->SetAttribute( "value", val );
		root->InsertEndChild( el );
	};
	auto add_bool = [&]( const char *name, bool val )
	{
		add_int( name, val ? 1 : 0 );
	};

	// Game
	add_str  ( "game_version",          int_to_string(SMC_VERSION_MAJOR) + "." + int_to_string(SMC_VERSION_MINOR) + "." + int_to_string(SMC_VERSION_PATCH) );
	add_str  ( "game_language",         m_language );
	add_bool ( "game_always_run",       m_always_run );
	add_str  ( "game_menu_level",       m_menu_level );
	add_str  ( "game_user_data_dir",    m_force_user_data_dir );
	add_float( "game_camera_hor_speed", m_camera_hor_speed );
	add_float( "game_camera_ver_speed", m_camera_ver_speed );
	// Video
	add_bool ( "video_fullscreen",       m_video_fullscreen );
	add_int  ( "video_screen_w",         m_video_screen_w );
	add_int  ( "video_screen_h",         m_video_screen_h );
	add_int  ( "video_screen_bpp",       static_cast<int>(m_video_screen_bpp) );
	add_bool ( "video_vsync",            m_video_vsync );
	add_int  ( "video_fps_limit",        m_video_fps_limit );
	add_float( "video_geometry_quality", pVideo->m_geometry_quality );
	add_float( "video_texture_quality",  pVideo->m_texture_quality );
	// Audio
	add_bool ( "audio_music",        m_audio_music );
	add_bool ( "audio_sound",        m_audio_sound );
	add_int  ( "audio_sound_volume", static_cast<int>(pAudio->m_sound_volume) );
	add_int  ( "audio_music_volume", static_cast<int>(pAudio->m_music_volume) );
	add_int  ( "audio_hz",           m_audio_hz );
	// Keyboard
	add_int  ( "keyboard_key_up",                    m_key_up );
	add_int  ( "keyboard_key_down",                  m_key_down );
	add_int  ( "keyboard_key_left",                  m_key_left );
	add_int  ( "keyboard_key_right",                 m_key_right );
	add_int  ( "keyboard_key_jump",                  m_key_jump );
	add_int  ( "keyboard_key_shoot",                 m_key_shoot );
	add_int  ( "keyboard_key_item",                  m_key_item );
	add_int  ( "keyboard_key_action",                m_key_action );
	add_float( "keyboard_scroll_speed",              m_scroll_speed );
	add_int  ( "keyboard_key_screenshot",            m_key_screenshot );
	add_int  ( "keyboard_key_editor_fast_copy_up",   m_key_editor_fast_copy_up );
	add_int  ( "keyboard_key_editor_fast_copy_down", m_key_editor_fast_copy_down );
	add_int  ( "keyboard_key_editor_fast_copy_left", m_key_editor_fast_copy_left );
	add_int  ( "keyboard_key_editor_fast_copy_right",m_key_editor_fast_copy_right );
	add_int  ( "keyboard_key_editor_pixel_move_up",  m_key_editor_pixel_move_up );
	add_int  ( "keyboard_key_editor_pixel_move_down",m_key_editor_pixel_move_down );
	add_int  ( "keyboard_key_editor_pixel_move_left",m_key_editor_pixel_move_left );
	add_int  ( "keyboard_key_editor_pixel_move_right",m_key_editor_pixel_move_right );
	// Joystick/Gamepad
	add_bool ( "joy_enabled",        m_joy_enabled );
	add_str  ( "joy_name",           m_joy_name );
	add_bool ( "joy_analog_jump",    m_joy_analog_jump );
	add_int  ( "joy_axis_hor",       m_joy_axis_hor );
	add_int  ( "joy_axis_ver",       m_joy_axis_ver );
	add_int  ( "joy_axis_threshold", m_joy_axis_threshold );
	add_int  ( "joy_button_jump",    static_cast<int>(m_joy_button_jump) );
	add_int  ( "joy_button_item",    static_cast<int>(m_joy_button_item) );
	add_int  ( "joy_button_shoot",   static_cast<int>(m_joy_button_shoot) );
	add_int  ( "joy_button_action",  static_cast<int>(m_joy_button_action) );
	add_int  ( "joy_button_exit",    static_cast<int>(m_joy_button_exit) );
	// Special
	add_bool( "level_background_images", m_level_background_images );
	add_bool( "image_cache_enabled",     m_image_cache_enabled );
	// Editor
	add_bool( "editor_mouse_auto_hide",    m_editor_mouse_auto_hide );
	add_bool( "editor_show_item_images",   m_editor_show_item_images );
	add_int ( "editor_item_image_size",    m_editor_item_image_size );

	tinyxml2::XMLError err = doc.SaveFile( m_config_filename.c_str() );
	if( err != tinyxml2::XML_SUCCESS )
	{
		printf( "Error : couldn't save config %s (TinyXML2 error %d)\n", m_config_filename.c_str(), (int)err );
	}
}

void cPreferences :: Reset_All( void )
{
	// Game
	m_game_version = smc_version;
	m_force_user_data_dir.clear();

	Reset_Game();
	Reset_Video();
	Reset_Audio();
	Reset_Keyboard();
	Reset_Joystick();
	Reset_Editor();

	// Special
	m_level_background_images = 1;
	m_image_cache_enabled = 1;

	// filename
	m_config_filename = "config.xml";
}

void cPreferences :: Reset_Game( void )
{
	m_language = "";
	m_always_run = m_always_run_default;
	m_touch_vibration = m_touch_vibration_default;
	m_touch_opacity = m_touch_opacity_default;
	m_touch_scale = m_touch_scale_default;
	m_menu_level = m_menu_level_default;
	m_camera_hor_speed = m_camera_hor_speed_default;
	m_camera_ver_speed = m_camera_ver_speed_default;
}

void cPreferences :: Reset_Video( void )
{
	// Video
	m_video_screen_w = m_video_screen_w_default;
	m_video_screen_h = m_video_screen_h_default;
	m_video_screen_bpp = m_video_screen_bpp_default;
	m_video_vsync = m_video_vsync_default;
	m_video_fps_limit = m_video_fps_limit_default;
	m_video_fullscreen = m_video_fullscreen_default;
	pVideo->m_geometry_quality = m_geometry_quality_default;
	pVideo->m_texture_quality = m_texture_quality_default;
}

void cPreferences :: Reset_Audio( void )
{
	// Audio
	m_audio_music = m_audio_music_default;
	m_audio_sound = m_audio_sound_default;
	m_audio_hz = m_audio_hz_default;
	pAudio->m_sound_volume = m_sound_volume_default;
	pAudio->m_music_volume = m_music_volume_default;
}

void cPreferences :: Reset_Keyboard( void )
{
	m_key_up = m_key_up_default;
	m_key_down = m_key_down_default;
	m_key_left = m_key_left_default;
	m_key_right = m_key_right_default;
	m_key_jump = m_key_jump_default;
	m_key_shoot = m_key_shoot_default;
	m_key_item = m_key_item_default;
	m_key_action = m_key_action_default;
	m_scroll_speed = m_scroll_speed_default;
	m_key_screenshot = m_key_screenshot_default;
	m_key_editor_fast_copy_up = m_key_editor_fast_copy_up_default;
	m_key_editor_fast_copy_down = m_key_editor_fast_copy_down_default;
	m_key_editor_fast_copy_left = m_key_editor_fast_copy_left_default;
	m_key_editor_fast_copy_right = m_key_editor_fast_copy_right_default;
	m_key_editor_pixel_move_up = m_key_editor_pixel_move_up_default;
	m_key_editor_pixel_move_down = m_key_editor_pixel_move_down_default;
	m_key_editor_pixel_move_left = m_key_editor_pixel_move_left_default;
	m_key_editor_pixel_move_right = m_key_editor_pixel_move_right_default;
}

void cPreferences :: Reset_Joystick( void )
{
	m_joy_enabled = m_joy_enabled_default;
	m_joy_name.clear();
	m_joy_analog_jump = m_joy_analog_jump_default;
	// axes
	m_joy_axis_hor = m_joy_axis_hor_default;
	m_joy_axis_ver = m_joy_axis_ver_default;
	// axis threshold
	m_joy_axis_threshold = m_joy_axis_threshold_default;
	// buttons
	m_joy_button_jump = m_joy_button_jump_default;
	m_joy_button_shoot = m_joy_button_shoot_default;
	m_joy_button_item = m_joy_button_item_default;
	m_joy_button_action = m_joy_button_action_default;
	m_joy_button_exit = m_joy_button_exit_default;
}

void cPreferences :: Reset_Editor( void )
{
	m_editor_mouse_auto_hide = m_editor_mouse_auto_hide_default;
	m_editor_show_item_images = m_editor_show_item_images_default;
	m_editor_item_image_size = m_editor_item_image_size_default;
}

void cPreferences :: Update( void )
{
	m_camera_hor_speed = pLevel_Manager->m_camera->m_hor_offset_speed;
	m_camera_ver_speed = pLevel_Manager->m_camera->m_ver_offset_speed;

	m_audio_music = pAudio->m_music_enabled;
	m_audio_sound = pAudio->m_sound_enabled;

	// if not default joy used
	if( pJoystick->m_current_joystick > 0 )
	{
		m_joy_name = pJoystick->Get_Name();
	}
	// using default joy
	else
	{
		m_joy_name.clear();
	}
}

void cPreferences :: Apply( void )
{
	pLevel_Manager->m_camera->m_hor_offset_speed = m_camera_hor_speed;
	pLevel_Manager->m_camera->m_ver_offset_speed = m_camera_ver_speed;
	
	// disable joystick if the joystick initialization failed
	if( pVideo->m_joy_init_failed )
	{
		m_joy_enabled = 0;
	}
}

void cPreferences :: Apply_Video( Uint16 screen_w, Uint16 screen_h, Uint8 screen_bpp, bool fullscreen, bool vsync, float geometry_detail, float texture_detail )
{
	/* if resolution, bpp, vsync or texture detail changed
	 * a texture reload is necessary
	*/
	if( m_video_screen_w != screen_w || m_video_screen_h != screen_h || m_video_screen_bpp != screen_bpp || m_video_vsync != vsync || !Is_Float_Equal( pVideo->m_texture_quality, texture_detail ) )
	{
		// new settings
		m_video_screen_w = screen_w;
		m_video_screen_h = screen_h;
		m_video_screen_bpp = screen_bpp;
		m_video_vsync = vsync;
		m_video_fullscreen = fullscreen;
		pVideo->m_texture_quality = texture_detail;
		pVideo->m_geometry_quality = geometry_detail;

		// reinitialize video and reload textures from file
		pVideo->Init_Video( 1 );
	}
	// no texture reload necessary
	else
	{
		// geometry detail changed
		if( !Is_Float_Equal( pVideo->m_geometry_quality, geometry_detail ) )
		{
			pVideo->m_geometry_quality = geometry_detail;
			pVideo->Init_Geometry();
		}

		// fullscreen changed
		if( m_video_fullscreen != fullscreen )
		{
			// toggle fullscreen and switches video_fullscreen itself
			pVideo->Toggle_Fullscreen();
		}
	}
}

void cPreferences :: Apply_Audio( bool sound, bool music )
{
	// disable sound and music if the audio initialization failed
	if( pVideo->m_audio_init_failed )
	{
		m_audio_sound = 0;
		m_audio_music = 0;
		return;
	}

	m_audio_sound = sound;
	m_audio_music = music;

	// init audio settings
	pAudio->Init();
}

void cPreferences :: handle_item( const char *name, const char *value )
{
	// Game
	if( strcmp( name, "game_version" ) == 0 )
	{
		m_game_version = string_to_version_number( value );
	}
	else if( strcmp( name, "game_language" ) == 0 )
	{
		m_language = value;
	}
	else if( strcmp( name, "game_always_run" ) == 0 || strcmp( name, "always_run" ) == 0 )
	{
		m_always_run = atoi( value ) != 0;
	}
	else if( strcmp( name, "touch_vibration" ) == 0 )
	{
		m_touch_vibration = atoi( value ) != 0;
	}
	else if( strcmp( name, "touch_opacity" ) == 0 )
	{
		m_touch_opacity = static_cast<float>( atof( value ) );
	}
	else if( strcmp( name, "touch_scale" ) == 0 )
	{
		m_touch_scale = static_cast<float>( atof( value ) );
	}
	else if( strcmp( name, "game_menu_level" ) == 0 )
	{
		m_menu_level = value;
	}
	else if( strcmp( name, "game_user_data_dir" ) == 0 || strcmp( name, "user_data_dir" ) == 0 )
	{
		m_force_user_data_dir = value;

		// if user data dir is set
		if( !m_force_user_data_dir.empty() ) 
		{
			Convert_Path_Separators( m_force_user_data_dir );

			// add trailing slash if missing
			if( *(m_force_user_data_dir.end() - 1) != '/' )
			{
				m_force_user_data_dir.insert( m_force_user_data_dir.length(), "/" );
			}
		}
	}
	else if( strcmp( name, "game_camera_hor_speed" ) == 0 || strcmp( name, "camera_hor_speed" ) == 0 )
	{
		m_camera_hor_speed = (float)atof( value );
	}
	else if( strcmp( name, "game_camera_ver_speed" ) == 0 || strcmp( name, "camera_ver_speed" ) == 0 )
	{
		m_camera_ver_speed = (float)atof( value );
	}
	// Video
	else if( strcmp( name, "video_screen_h" ) == 0 )
	{
		int val = atoi( value );
		if( val < 200 ) val = 200;
		else if( val > 2560 ) val = 2560;
		m_video_screen_h = val;
	}
	else if( strcmp( name, "video_screen_w" ) == 0 )
	{
		int val = atoi( value );
		if( val < 200 ) val = 200;
		else if( val > 2560 ) val = 2560;
		m_video_screen_w = val;
	}
	else if( strcmp( name, "video_screen_bpp" ) == 0 )
	{
		int val = atoi( value );
		if( val < 8 ) val = 8;
		else if( val > 32 ) val = 32;
		m_video_screen_bpp = val;
	}
	else if( strcmp( name, "video_vsync" ) == 0 )
	{
		m_video_vsync = atoi( value ) != 0;
	}
	else if( strcmp( name, "video_fps_limit" ) == 0 )
	{
		m_video_fps_limit = atoi( value );
	}
	else if( strcmp( name, "video_fullscreen" ) == 0 )
	{
		m_video_fullscreen = atoi( value ) != 0;
	}
	else if( strcmp( name, "video_geometry_detail" ) == 0 || strcmp( name, "video_geometry_quality" ) == 0 )
	{
		pVideo->m_geometry_quality = (float)atof( value );
	}
	else if( strcmp( name, "video_texture_detail" ) == 0 || strcmp( name, "video_texture_quality" ) == 0 )
	{
		pVideo->m_texture_quality = (float)atof( value );
	}
	// Audio
	else if( strcmp( name, "audio_music" ) == 0 )
	{
		m_audio_music = atoi( value ) != 0;
	}
	else if( strcmp( name, "audio_sound" ) == 0 )
	{
		m_audio_sound = atoi( value ) != 0;
	}
	else if( strcmp( name, "audio_music_volume" ) == 0 )
	{
		int val = atoi( value );
		if( val >= 0 && val <= MIX_MAX_VOLUME )
			pAudio->m_music_volume = val;
	}
	else if( strcmp( name, "audio_sound_volume" ) == 0 )
	{
		int val = atoi( value );
		if( val >= 0 && val <= MIX_MAX_VOLUME )
			pAudio->m_sound_volume = val;
	}
	else if( strcmp( name, "audio_hz" ) == 0 )
	{
		int val = atoi( value );
		if( val >= 0 && val <= 96000 )
			m_audio_hz = val;
	}
	// Keyboard
	else if( strcmp( name, "keyboard_key_up" ) == 0 )
	{
		int val = atoi( value );
		if( val >= 0 && val <= SDLK_LAST ) m_key_up = static_cast<SDLKey>(val);
	}
	else if( strcmp( name, "keyboard_key_down" ) == 0 )
	{
		int val = atoi( value );
		if( val >= 0 && val <= SDLK_LAST ) m_key_down = static_cast<SDLKey>(val);
	}
	else if( strcmp( name, "keyboard_key_left" ) == 0 )
	{
		int val = atoi( value );
		if( val >= 0 && val <= SDLK_LAST ) m_key_left = static_cast<SDLKey>(val);
	}
	else if( strcmp( name, "keyboard_key_right" ) == 0 )
	{
		int val = atoi( value );
		if( val >= 0 && val <= SDLK_LAST ) m_key_right = static_cast<SDLKey>(val);
	}
	else if( strcmp( name, "keyboard_key_jump" ) == 0 )
	{
		int val = atoi( value );
		if( val >= 0 && val <= SDLK_LAST ) m_key_jump = static_cast<SDLKey>(val);
	}
	else if( strcmp( name, "keyboard_key_shoot" ) == 0 )
	{
		int val = atoi( value );
		if( val >= 0 && val <= SDLK_LAST ) m_key_shoot = static_cast<SDLKey>(val);
	}
	else if( strcmp( name, "keyboard_key_item" ) == 0 )
	{
		int val = atoi( value );
		if( val >= 0 && val <= SDLK_LAST ) m_key_item = static_cast<SDLKey>(val);
	}
	else if( strcmp( name, "keyboard_key_action" ) == 0 )
	{
		int val = atoi( value );
		if( val >= 0 && val <= SDLK_LAST ) m_key_action = static_cast<SDLKey>(val);
	}
	else if( strcmp( name, "keyboard_scroll_speed" ) == 0 )
	{
		m_scroll_speed = (float)atof( value );
	}
	else if( strcmp( name, "keyboard_key_screenshot" ) == 0 )
	{
		int val = atoi( value );
		if( val >= 0 && val <= SDLK_LAST ) m_key_screenshot = static_cast<SDLKey>(val);
	}
	else if( strcmp( name, "keyboard_key_editor_fast_copy_up" ) == 0 )
	{
		int val = atoi( value );
		if( val >= 0 && val <= SDLK_LAST ) m_key_editor_fast_copy_up = static_cast<SDLKey>(val);
	}
	else if( strcmp( name, "keyboard_key_editor_fast_copy_down" ) == 0 )
	{
		int val = atoi( value );
		if( val >= 0 && val <= SDLK_LAST ) m_key_editor_fast_copy_down = static_cast<SDLKey>(val);
	}
	else if( strcmp( name, "keyboard_key_editor_fast_copy_left" ) == 0 )
	{
		int val = atoi( value );
		if( val >= 0 && val <= SDLK_LAST ) m_key_editor_fast_copy_left = static_cast<SDLKey>(val);
	}
	else if( strcmp( name, "keyboard_key_editor_fast_copy_right" ) == 0 )
	{
		int val = atoi( value );
		if( val >= 0 && val <= SDLK_LAST ) m_key_editor_fast_copy_right = static_cast<SDLKey>(val);
	}
	else if( strcmp( name, "keyboard_key_editor_pixel_move_up" ) == 0 )
	{
		int val = atoi( value );
		if( val >= 0 && val <= SDLK_LAST ) m_key_editor_pixel_move_up = static_cast<SDLKey>(val);
	}
	else if( strcmp( name, "keyboard_key_editor_pixel_move_down" ) == 0 )
	{
		int val = atoi( value );
		if( val >= 0 && val <= SDLK_LAST ) m_key_editor_pixel_move_down = static_cast<SDLKey>(val);
	}
	else if( strcmp( name, "keyboard_key_editor_pixel_move_left" ) == 0 )
	{
		int val = atoi( value );
		if( val >= 0 && val <= SDLK_LAST ) m_key_editor_pixel_move_left = static_cast<SDLKey>(val);
	}
	else if( strcmp( name, "keyboard_key_editor_pixel_move_right" ) == 0 )
	{
		int val = atoi( value );
		if( val >= 0 && val <= SDLK_LAST ) m_key_editor_pixel_move_right = static_cast<SDLKey>(val);
	}
	// Joypad
	else if( strcmp( name, "joy_enabled" ) == 0 )
	{
		m_joy_enabled = atoi( value ) != 0;
	}
	else if( strcmp( name, "joy_name" ) == 0 )
	{
		m_joy_name = value;
	}
	else if( strcmp( name, "joy_analog_jump" ) == 0 )
	{
		m_joy_analog_jump = atoi( value ) != 0;
	}
	else if( strcmp( name, "joy_axis_hor" ) == 0 )
	{
		int val = atoi( value );
		if( val >= 0 && val <= 256 ) m_joy_axis_hor = val;
	}
	else if( strcmp( name, "joy_axis_ver" ) == 0 )
	{
		int val = atoi( value );
		if( val >= 0 && val <= 256 ) m_joy_axis_ver = val;
	}
	else if( strcmp( name, "joy_axis_threshold" ) == 0 )
	{
		int val = atoi( value );
		if( val >= 0 && val <= 32767 ) m_joy_axis_threshold = val;
	}
	else if( strcmp( name, "joy_button_jump" ) == 0 )
	{
		int val = atoi( value );
		if( val >= 0 && val <= 256 ) m_joy_button_jump = val;
	}
	else if( strcmp( name, "joy_button_item" ) == 0 )
	{
		int val = atoi( value );
		if( val >= 0 && val <= 256 ) m_joy_button_item = val;
	}
	else if( strcmp( name, "joy_button_shoot" ) == 0 )
	{
		int val = atoi( value );
		if( val >= 0 && val <= 256 ) m_joy_button_shoot = val;
	}
	else if( strcmp( name, "joy_button_action" ) == 0 )
	{
		int val = atoi( value );
		if( val >= 0 && val <= 256 ) m_joy_button_action = val;
	}
	else if( strcmp( name, "joy_button_exit" ) == 0 )
	{
		int val = atoi( value );
		if( val >= 0 && val <= 256 ) m_joy_button_exit = val;
	}
	// Special
	else if( strcmp( name, "level_background_images" ) == 0 )
	{
		m_level_background_images = atoi( value ) != 0;
	}
	else if( strcmp( name, "image_cache_enabled" ) == 0 )
	{
		m_image_cache_enabled = atoi( value ) != 0;
	}
	// Editor
	else if( strcmp( name, "editor_mouse_auto_hide" ) == 0 )
	{
		m_editor_mouse_auto_hide = atoi( value ) != 0;
	}
	else if( strcmp( name, "editor_show_item_images" ) == 0 )
	{
		m_editor_show_item_images = atoi( value ) != 0;
	}
	else if( strcmp( name, "editor_item_image_size" ) == 0 )
	{
		m_editor_item_image_size = atoi( value );
	}
}

/* *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** */

cPreferences *pPreferences = NULL;

/* *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** */

} // namespace SMC
