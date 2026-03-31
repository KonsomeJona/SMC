/***************************************************************************
 * main.cpp  -  main routines and initialization
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

#include "../core/global_basic.h"
#include "../core/game_core.h"
#include "../core/main.h"
#include "../core/filesystem/resource_manager.h"
#include "../core/filesystem/filesystem.h"
#include "../level/level.h"
#include "../gui/menu.h"
#include "../core/framerate.h"
#include "../video/font.h"
#include "../user/preferences.h"
#include "../audio/sound_manager.h"
#include "../audio/audio.h"
#include "../level/level_editor.h"
#include "../overworld/world_editor.h"
#include "../input/joystick.h"
#include "../overworld/world_manager.h"
#include "../overworld/overworld.h"
#include "../core/campaign_manager.h"
#include "../input/mouse.h"
#include "../user/savegame.h"
#include "../input/keyboard.h"
#include "../input/touch_controls.h"
#include "../video/renderer.h"
#include "../video/gles2_renderer.h"
#include "../core/i18n.h"
#include "../gui/generic.h"
#include "../gui/modern_ui.h"
#include "core/sdl2_compat.h"
#include "../core/debug_log.h"
#include <signal.h>
#include <execinfo.h>

static void crash_handler(int sig)
{
    fprintf(stderr, "[CRASH] Signal %d (%s) received\n", sig, strsignal(sig));
    void *buffer[64];
    int nptrs = backtrace(buffer, 64);
    fprintf(stderr, "[CRASH] Backtrace (%d frames):\n", nptrs);
    backtrace_symbols_fd(buffer, nptrs, STDERR_FILENO);
    fflush(stderr);
    signal(sig, SIG_DFL);
    raise(sig);
}

#ifdef __APPLE__
// needed for datapath detection
#include <CoreFoundation/CoreFoundation.h>
#include <unistd.h>
#endif

// CEGUI include removed in M12 (Init_CEGUI stubbed; remaining uses guarded with null checks)

// SMC namespace is set later to exclude main() from it
using namespace SMC;

// SDLmain defines this for Win32 applications but under debug we use the console
#if defined( __WIN32__ ) && defined( _DEBUG )
	#undef main
#endif

/* *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** */

int main( int argc, char **argv )
{
    signal(SIGSEGV, crash_handler);
    signal(SIGABRT, crash_handler);
    signal(SIGFPE, crash_handler);
// todo : remove this apple hack
#ifdef __APPLE__
	// dynamic datapath detection for OS X
	// change CWD to point inside bundle so it finds its data (if necessary)
	char path[1024];
	CFBundleRef mainBundle = CFBundleGetMainBundle();
	assert(mainBundle);
	CFURLRef mainBundleURL = CFBundleCopyBundleURL(mainBundle);
	assert(mainBundleURL);
	CFStringRef cfStringRef = CFURLCopyFileSystemPath( mainBundleURL, kCFURLPOSIXPathStyle);
	assert(cfStringRef);
	CFStringGetCString(cfStringRef, path, 1024, kCFStringEncodingASCII);
	CFRelease(mainBundleURL);
	CFRelease(cfStringRef);

	std::string contents = std::string(path) + std::string("/Contents");
	std::string datapath;

	if( contents.find(".app") != std::string::npos )
	{
		// executable is inside an app bundle, use app bundle-relative paths
		datapath = contents + std::string("/Resources/data/");
	}
	else if( contents.find("/bin") != std::string::npos )
	{
		// executable is installed Unix-way
		datapath = contents.substr( 0, contents.find("/bin") ) + "/share/smc";
	}
	else
	{
		std::cerr << "Warning: Could not determine installation type\n";
	}

	if( !datapath.empty() )
	{
		std::cout << "setting CWD to " << datapath.c_str() << std::endl;
		if( chdir( datapath.c_str() ) != 0 )
		{
			std::cerr << "Warning: Failed changing CWD\n";
		}
	}
#endif

	// convert arguments to a vector string
	vector<std::string> arguments( argv, argv + argc );

	if( argc >= 2 )
	{
		for( unsigned int i = 1; i < arguments.size(); i++ )
		{
			// help
			if( arguments[i] == "--help" || arguments[i] == "-h" )
			{
				printf( "Usage: %s [OPTIONS]\n", arguments[0].c_str() );
				printf( "Where OPTIONS is one of the following:\n" );
				printf( "-h, --help\tDisplay this message\n" );
				printf( "-v, --version\tShow the version of %s\n", CAPTION );
				printf( "-d, --debug\tEnable debug modes with the options : game performance\n" );
				printf( "-l, --level\tLoad the given level\n" );
				printf( "-w, --world\tLoad the given world\n" );
				return EXIT_SUCCESS;
			}
			// version
			else if( arguments[i] == "--version" || arguments[i] == "-v" )
			{
				printf( "%s %d.%d.%d\n", CAPTION, SMC_VERSION_MAJOR, SMC_VERSION_MINOR, SMC_VERSION_PATCH );
				return EXIT_SUCCESS;
			}
			// debug
			else if( arguments[i] == "--debug" || arguments[i] == "-d" )
			{
				// no value
				if( i + 1 >= arguments.size() )
				{
					printf( "%s requires a value\n", arguments[i].c_str() );
					return EXIT_FAILURE;
				}
				// with value
				else
				{
					for( unsigned int option = i + i; i < arguments.size(); i++ )
					{
						std::string option_str = arguments[option];

						if( option_str == "game" )
						{
							game_debug = 1;
						}
						else if( option_str == "performance" )
						{
							game_debug_performance = 1;
						}
						else
						{
							printf( "Unknown debug option %s\n", option_str.c_str() );
							return EXIT_FAILURE;
						}
					}
				}
			}
			// level loading is handled later
			else if( arguments[i] == "--level" || arguments[i] == "-l" )
			{
				// skip
			}
			// world loading is handled later
			else if( arguments[1] == "--world" || arguments[1] == "-w" )
			{
				// skip
			}
			// unknown argument
			else if( arguments[i].substr( 0, 1 ) == "-" )
			{
				printf( "Unknown argument %s\nUse -h to list all possible arguments\n", arguments[i].c_str() );
				return EXIT_FAILURE;
			}
		}
	}

	try
	{
		// initialize everything
		Init_Game();
	}
	catch( const std::exception &e )
	{
		printf( "Initialization: Exception raised: %s\n", e.what() );
		return EXIT_FAILURE;
	}

	// command line level entering
	if( argc > 2 && ( arguments[1] == "--level" || arguments[1] == "-l" ) && !arguments[2].empty() )
	{
		Game_Action = GA_ENTER_LEVEL;
		Game_Mode_Type = MODE_TYPE_LEVEL_CUSTOM;
		Game_Action_Data_Middle.add( "load_level", arguments[2] );
	}
	// command line world entering
	else if( argc > 2 && ( arguments[1] == "--world" || arguments[1] == "-w" ) && !arguments[2].empty() )
	{
		Game_Action = GA_ENTER_WORLD;
		Game_Action_Data_Middle.add( "enter_world", arguments[2] );
	}
	// enter main menu
	else
	{
		Game_Action = GA_ENTER_MENU;
		Game_Action_Data_Middle.add( "load_menu", int_to_string( MENU_MAIN ) );
	}

	Game_Action_Data_Start.add( "screen_fadeout", int_to_string( EFFECT_OUT_BLACK ) );
	Game_Action_Data_Start.add( "screen_fadeout_speed", "3" );
	Game_Action_Data_End.add( "screen_fadein", int_to_string( EFFECT_IN_BLACK ) );
	Game_Action_Data_End.add( "screen_fadein_speed", "3" );

	// game loop
	while( !game_exit )
	{
		// update
		Update_Game();
		// draw
		Draw_Game();

		// render
#ifdef SMC_RENDER_THREAD_TEST
		pVideo->Render( 1 );
#else
		pVideo->Render();
#endif

		// update speedfactor
		pFramerate->Update();
	}

	Exit_Game();
	return EXIT_SUCCESS;
}

// namespace is set here to exclude main() from it
namespace SMC
{

void Init_Game( void )
{
	// init random number generator
	srand( static_cast<unsigned int>(time( NULL )) );

	// Init Stage 1 - core classes
	pResource_Manager = new cResource_Manager();
	pVideo = new cVideo();
	pAudio = new cAudio();
	pFont = new cFont_Manager();
	pFramerate = new cFramerate();
	pRenderer = new cRenderQueue( 200 );
	pRenderer_current = new cRenderQueue( 200 );
	pPreferences = new cPreferences();
	pImage_Manager = new cImage_Manager();
	pSound_Manager = new cSound_Manager();
	pSettingsParser = new cImage_Settings_Parser();

	// Init Stage 2 - set preferences and init audio and the video screen
	/* Set default user directory
	 * can get overridden later from the preferences
	*/
	pResource_Manager->Set_User_Directory( Get_User_Directory() );
	// load user data (preferences now parsed with TinyXML2, no CEGUI needed)
	pPreferences->Load();
	// set game language
	I18N_Set_Language( pPreferences->m_language );
	// init translation support
	I18N_Init();

	// init user dir directory
	pResource_Manager->Init_User_Directory();
	// video init
	pVideo->Init_SDL();
	pVideo->Init_Video();
	pVideo->Init_CEGUI();
	pVideo->Init_CEGUI_Data();
	pFont->Init();
	// framerate init ( must be after SDL init because of SDL_GetTicks() )
	pFramerate->Init();
	// audio init
	pAudio->Init();

	pCampaign_Manager = new cCampaign_Manager();
	pLevel_Player = new cLevel_Player( NULL );
	pLevel_Player->m_disallow_managed_delete = 1;
	// set the first active player available
	pActive_Player = pLevel_Player;
	pLevel_Manager = new cLevel_Manager();
	// set the first animation manager available
	pActive_Animation_Manager = pActive_Level->m_animation_manager;
	// set the first active sprite manager available
	pLevel_Player->Set_Sprite_Manager( pActive_Level->m_sprite_manager );

	// apply preferences
	pPreferences->Apply();

	// draw generic loading screen
	Loading_Screen_Init();
	// initialize image cache
	pVideo->Init_Image_Cache( 0, 1 );

	// Init Stage 3 - game classes
	// note : set any sprite manager as it is set again on game mode switch
	pHud_Manager = new cHud_Manager( pActive_Level->m_sprite_manager );
	pLevel_Player->Init();
	pLevel_Editor = new cEditor_Level( pActive_Level->m_sprite_manager, pActive_Level );
	/* note : set any sprite manager as cOverworld_Manager::Load sets it again 
	 * parent overworld is also set from there again
	*/
	pWorld_Editor = new cEditor_World( pActive_Level->m_sprite_manager, NULL );
	pMouseCursor = new cMouseCursor( pActive_Level->m_sprite_manager );
	pKeyboard = new cKeyboard();
	pJoystick = new cJoystick();
	pTouchControls = new cTouchControls();
	pTouchControls->Init();
	pLevel_Manager->Init();
	// note : set any sprite manager as cOverworld_Manager::Load sets it again
	pOverworld_Player = new cOverworld_Player( pActive_Level->m_sprite_manager, NULL );
	pOverworld_Manager = new cOverworld_Manager( pActive_Level->m_sprite_manager );
	// set default overworld active
	pOverworld_Player->Set_Overworld( pOverworld_Manager->Get( "World 1" ) );
	pOverworld_Manager->Set_Active( "World 1" );
	pHud_Manager->Load();
	pMenuCore = new cMenuCore();
	pSavegame = new cSavegame();

	// cache
	Preload_Images( 1 );
	Preload_Sounds( 1 );
	Loading_Screen_Exit();
}

void Exit_Game( void )
{
	if( pPreferences )
	{
		pPreferences->Save();
	}

	pLevel_Manager->Unload();
	pMenuCore->m_handler->m_level->Unload();

	if( pAudio )
	{
		delete pAudio;
		pAudio = NULL;
	}

	if( pLevel_Player )
	{
		delete pLevel_Player;
		pLevel_Player = NULL;
	}

	if( pHud_Manager )
	{
		delete pHud_Manager;
		pHud_Manager = NULL;
	}

	if( pSound_Manager )
	{
		delete pSound_Manager;
		pSound_Manager = NULL;
	}

	if( pLevel_Editor )
	{
		delete pLevel_Editor;
		pLevel_Editor = NULL;
	}

	if( pWorld_Editor )
	{
		delete pWorld_Editor;
		pWorld_Editor = NULL;
	}

	if( pPreferences )
	{
		delete pPreferences;
		pPreferences = NULL;
	}

	if( pSavegame )
	{
		delete pSavegame;
		pSavegame = NULL;
	}

	if( pMouseCursor )
	{
		delete pMouseCursor;
		pMouseCursor = NULL;
	}

	if( pTouchControls )
	{
		delete pTouchControls;
		pTouchControls = NULL;
	}

	if( pJoystick )
	{
		delete pJoystick;
		pJoystick = NULL;
	}

	if( pKeyboard )
	{
		delete pKeyboard;
		pKeyboard = NULL;
	}

	if( pCampaign_Manager )
	{
		delete pCampaign_Manager;
		pCampaign_Manager = NULL;
	}
	
	if( pOverworld_Manager )
	{
		delete pOverworld_Manager;
		pOverworld_Manager = NULL;
	}

	if( pOverworld_Player )
	{
		delete pOverworld_Player;
		pOverworld_Player = NULL;
	}

	if( pLevel_Manager )
	{
		delete pLevel_Manager;
		pLevel_Manager = NULL;
	}

	if( pMenuCore )
	{
		delete pMenuCore;
		pMenuCore = NULL;
	}

	if( pRenderer )
	{
		delete pRenderer;
		pRenderer = NULL;
	}

	if( pRenderer_current )
	{
		delete pRenderer_current;
		pRenderer_current = NULL;
	}

	// pGuiSystem / pGuiRenderer are NULL (Init_CEGUI is a stub since M12).
	// No cleanup needed; guards remain in case CEGUI is re-enabled later.
	if( pGuiSystem )
	{
		// pGuiSystem->destroy() would need CEGUI headers — keep guard but skip if NULL.
		pGuiSystem = NULL;
	}
	if( pGuiRenderer )
	{
		pGuiRenderer = NULL;
	}

	if( pVideo )
	{
		delete pVideo;
		pVideo = NULL;
	}

	if( pImage_Manager )
	{
		delete pImage_Manager;
		pImage_Manager = NULL;
	}

	if( pSettingsParser )
	{
		delete pSettingsParser;
		pSettingsParser = NULL;
	}

	if( pFont )
	{
		delete pFont;
		pFont = NULL;
	}

	if( pResource_Manager )
	{
		delete pResource_Manager;
		pResource_Manager = NULL;
	}

	const char *last_sdl_error = SDL_GetError();
	if( strlen( last_sdl_error ) > 0 )
	{
		printf( "Last known SDL Error : %s\n", last_sdl_error );
	}

	// unload the sdl_image preloaded libraries
	IMG_Quit();

	SDL_Quit();
}

bool Handle_Input_Global( SDL_Event *ev )
{
	LOG_DEBUG(INPUT, "Handle_Input_Global: event type=%d", ev->type);

	switch( ev->type )
	{
		case SDL_QUIT:
		{
			LOG_DEBUG(INPUT, "SDL_QUIT received");
			game_exit = 1;
			Clear_Input_Events();

			// handle on all handlers ?
			return 0;
		}
		case SDL_WINDOWEVENT:
		{
			LOG_DEBUG(INPUT, "SDL_WINDOWEVENT sub-event=%d", ev->window.event);
			if( ev->window.event == SDL_WINDOWEVENT_RESIZED )
			{
				// Update GL viewport to match new drawable size
				int draw_w, draw_h;
				SDL_GL_GetDrawableSize( g_sdl_window, &draw_w, &draw_h );
				glViewport( 0, 0, draw_w, draw_h );
				// CEGUI resize notification — no-op when CEGUI is not initialised (M12)
#ifdef __ANDROID__
				GLES2::Set_Projection( static_cast<float>(draw_w),
				                       static_cast<float>(draw_h) );
#endif
			}
			else if( ev->window.event == SDL_WINDOWEVENT_FOCUS_LOST )
			{
				bool music_paused = false;
				if( pAudio->Is_Music_Playing() )
				{
					pAudio->Pause_Music();
					music_paused = true;
				}
				SDL_Event wait_ev;
				while( SDL_WaitEvent( &wait_ev ) )
				{
					if( wait_ev.type == SDL_QUIT )
					{
						game_exit = 1;
						break;
					}
					if( wait_ev.type == SDL_FINGERDOWN )
						break;
					if( wait_ev.type == SDL_WINDOWEVENT &&
						wait_ev.window.event == SDL_WINDOWEVENT_FOCUS_GAINED )
						break;
				}
				if( music_paused )
				{
					pAudio->Resume_Music();
				}
			}
			break;
		}
		case SDL_KEYDOWN:
		{
			LOG_DEBUG(INPUT, "SDL_KEYDOWN key=%s (sym=%d, scancode=%d)", SDL_GetKeyName(ev->key.keysym.sym), ev->key.keysym.sym, ev->key.keysym.scancode);
			if( pKeyboard->Key_Down( ev->key.keysym.sym ) )
			{
				return 1;
			}
			break;
		}
		case SDL_KEYUP:
		{
			LOG_DEBUG(INPUT, "SDL_KEYUP key=%s (sym=%d)", SDL_GetKeyName(ev->key.keysym.sym), ev->key.keysym.sym);
			if( pKeyboard->Key_Up( ev->key.keysym.sym ) )
			{
				return 1;
			}
			break;
		}
		case SDL_TEXTINPUT:
		{
			// Text input was forwarded to CEGUI for editbox input.
			// CEGUI is not initialised in M12; the editor (which uses CEGUI editboxes)
			// handles its own text input internally via pGuiSystem when it is active.
			if( pGuiSystem )
			{
				const char *text = ev->text.text;
				for( int i = 0; text[i]; )
				{
					Uint32 cp = 0;
					unsigned char c = text[i];
					if( c < 0x80 ) { cp = c; i += 1; }
					else if( c < 0xE0 ) { cp = (c & 0x1F) << 6 | (text[i+1] & 0x3F); i += 2; }
					else if( c < 0xF0 ) { cp = (c & 0x0F) << 12 | (text[i+1] & 0x3F) << 6 | (text[i+2] & 0x3F); i += 3; }
					else { cp = (c & 0x07) << 18 | (text[i+1] & 0x3F) << 12 | (text[i+2] & 0x3F) << 6 | (text[i+3] & 0x3F); i += 4; }
					pGuiSystem->getDefaultGUIContext().injectChar( cp );
				}
			}
			break;
		}
		case SDL_MOUSEWHEEL:
		{
			// SDL2 mouse wheel events — forward to CEGUI only when it is initialised
			if( pGuiSystem )
			{
				if( ev->wheel.y > 0 )
					pGuiSystem->getDefaultGUIContext().injectMouseWheelChange( 1.0f );
				else if( ev->wheel.y < 0 )
					pGuiSystem->getDefaultGUIContext().injectMouseWheelChange( -1.0f );
			}
			break;
		}
		case SDL_JOYBUTTONDOWN:
		{
			LOG_DEBUG(INPUT, "SDL_JOYBUTTONDOWN button=%d", ev->jbutton.button);
			if( pJoystick->Handle_Button_Down_Event( ev ) )
			{
				return 1;
			}
			break;
		}
		case SDL_JOYBUTTONUP:
		{
			if( pJoystick->Handle_Button_Up_Event( ev ) )
			{
				return 1;
			}
			break;
		}
		case SDL_JOYHATMOTION:
		{
			pJoystick->Handle_Hat( ev );
			break;
		}
		case SDL_JOYAXISMOTION:
		{
			pJoystick->Handle_Motion( ev );
			break;
		}
		case SDL_FINGERDOWN:
		{
			if( pTouchControls && pTouchControls->m_enabled )
			{
				if( pTouchControls->Handle_Finger_Down( ev ) )
					return 1;
			}
			break;
		}
		case SDL_FINGERUP:
		{
			if( pTouchControls && pTouchControls->m_enabled )
			{
				if( pTouchControls->Handle_Finger_Up( ev ) )
					return 1;
			}
			break;
		}
		case SDL_FINGERMOTION:
		{
			if( pTouchControls && pTouchControls->m_enabled )
			{
				if( pTouchControls->Handle_Finger_Motion( ev ) )
					return 1;
			}
			break;
		}
		default: // other events
		{
			// Check touch controls for mouse clicks (touch pad / Surface)
			if( pTouchControls && pTouchControls->m_visible )
			{
				if( ev->type == SDL_MOUSEBUTTONDOWN && pTouchControls->Handle_Mouse_Down( ev ) )
				{
					return 1;
				}
				if( ev->type == SDL_MOUSEBUTTONUP && pTouchControls->Handle_Mouse_Up( ev ) )
				{
					return 1;
				}
				if( ev->type == SDL_MOUSEMOTION && pTouchControls->Handle_Mouse_Motion( ev ) )
				{
					// Don't consume motion — let game cursor update too
				}
			}

			// mouse
			if( pMouseCursor->Handle_Event( ev ) )
			{
				return 1;
			}

			// send events
			if( Game_Mode == MODE_LEVEL )
			{
				// editor events
				if( pLevel_Editor->m_enabled )
				{
					if( pLevel_Editor->Handle_Event( ev ) )
					{
						return 1;
					}
				}
			}
			else if( Game_Mode == MODE_OVERWORLD )
			{
				// editor events
				if( pWorld_Editor->m_enabled )
				{
					if( pWorld_Editor->Handle_Event( ev ) )
					{
						return 1;
					}
				}
			}
			else if( Game_Mode == MODE_MENU )
			{
				if( pMenuCore->Handle_Event( ev ) )
				{
					return 1;
				}
			}
			break;
		}
	}

	return 0;
}

void Update_Game( void )
{
	// do not update if exiting
	if( game_exit )
	{
		return;
	}

	// if in menu and vsync is disabled then limit the fps to reduce the load for CPU/GPU
	if( Game_Mode == MODE_MENU && !pPreferences->m_video_vsync )
	{
		Correct_Frame_Time( 100 );
	}
	// if fps limit is set
	else if( pPreferences->m_video_fps_limit )
	{
		Correct_Frame_Time( pPreferences->m_video_fps_limit );
	}
	
	if( Game_Action != GA_NONE )
	{
		pVideo->Render_Finish();
	}
	
	// ## game events
	Handle_Game_Events();

	// ## input
	ModernUI::Begin_Frame();
	while( SDL_PollEvent( &input_event ) )
	{
		// handle
		Handle_Input_Global( &input_event );
	}

	pMouseCursor->Update();

	// ## touch controls
	if( pTouchControls )
	{
		pTouchControls->Update();
	}

	// ## audio
	pAudio->Resume_Music();
	pAudio->Update();

	// performance measuring
	pFramerate->m_perf_last_ticks = SDL_GetTicks();

	// ## update
	if( Game_Mode == MODE_LEVEL )
	{
		LOG_DEBUG(GAME, "Update_Game: MODE_LEVEL");
		pLevel_Manager->Update();
	}
	else if( Game_Mode == MODE_OVERWORLD )
	{
		LOG_DEBUG(GAME, "Update_Game: MODE_OVERWORLD");
		pActive_Overworld->Update();
	}
	else if( Game_Mode == MODE_MENU )
	{
		LOG_DEBUG(GAME, "Update_Game: MODE_MENU");
		pMenuCore->Update();
	}
	else if( Game_Mode == MODE_LEVEL_SETTINGS )
	{
		LOG_DEBUG(GAME, "Update_Game: MODE_LEVEL_SETTINGS");
		pLevel_Editor->m_settings_screen->Update();
	}

	// gui
	Gui_Handle_Time();
}

void Draw_Game( void )
{
	// don't draw if exiting
	if( game_exit )
	{
		return;
	}

	// performance measuring
	pFramerate->m_perf_last_ticks = SDL_GetTicks();

	if( Game_Mode == MODE_LEVEL )
	{
		pLevel_Manager->Draw();
	}
	else if( Game_Mode == MODE_OVERWORLD )
	{
		pActive_Overworld->Draw();
	}
	else if( Game_Mode == MODE_MENU )
	{
		pMenuCore->Draw();
	}
	else if( Game_Mode == MODE_LEVEL_SETTINGS )
	{
		pLevel_Editor->m_settings_screen->Draw();
	}

	// Mouse
	pMouseCursor->Draw();

	// Touch controls overlay (drawn on top of everything, direct GL)
	if( pTouchControls && pTouchControls->m_visible )
	{
		pTouchControls->Draw();
	}

	// update performance timer
	pFramerate->m_perf_timer[PERF_DRAW_MOUSE]->Update();
}

/* *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** */

} // namespace SMC
