/***************************************************************************
 * generic.cpp  -  generic gui stuff
 *
 * Copyright (C) 2005 - 2011 Florian Richter
 ***************************************************************************/
/*
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "../gui/generic.h"
#include "../core/game_core.h"
#include "../core/main.h"
#include "../core/framerate.h"
#include "../input/mouse.h"
#include "../input/keyboard.h"
#include "../video/renderer.h"
#include "../video/font.h"
#include "../video/gl_surface.h"
#include "../user/preferences.h"
#include "../gui/modern_ui.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <algorithm>
#include <cstring>

#ifdef __unix__
	// needed for the clipboard access
#  include <SDL_syswm.h>
#elif __APPLE__
	// needed for the clipboard access
#include <Carbon/Carbon.h>
#endif

// CEGUI — still needed to compile the header-declared CEGUI member types
#include <CEGUI/WindowManager.h>
#include <CEGUI/FontManager.h>
#include <CEGUI/widgets/Editbox.h>
#include <CEGUI/widgets/FrameWindow.h>
#include <CEGUI/widgets/PushButton.h>
#include <CEGUI/widgets/MultiLineEditbox.h>

namespace SMC
{

#include "../gui/ui_palette.h"

// ---------------------------------------------------------------------------
// Shared dialog layout constants (match cHelpCard / ModernUI visual style)
// ---------------------------------------------------------------------------
static const float DLG_HEADER_H  = 40.0f;
static const float DLG_PAD       = 14.0f;
static const float DLG_BTN_H     = 32.0f;
static const float DLG_BTN_W     = 90.0f;
static const float DLG_BORDER_T  = 3.0f;
static const float DLG_INPUT_H   = 30.0f;

// ---------------------------------------------------------------------------
// Internal helper: draw the overlay dimming + a titled card panel.
// Returns the top-left (cx, cy) of the card body area (below header).
// ---------------------------------------------------------------------------
static void Draw_Dialog_Frame( float cx, float cy, float cw, float ch,
                               const std::string &title,
                               std::vector<cGL_Surface*> &pd )
{
    // Dim overlay
    pVideo->Draw_Rect( 0, 0,
                       static_cast<float>(game_res_w),
                       static_cast<float>(game_res_h),
                       0.899f, &COL_OVERLAY );

    // Outer border
    pVideo->Draw_Rect( cx - DLG_BORDER_T, cy - DLG_BORDER_T,
                       cw + DLG_BORDER_T * 2.0f, ch + DLG_BORDER_T * 2.0f,
                       0.900f, &COL_BORDER );

    // Card background
    pVideo->Draw_Rect( cx, cy, cw, ch, 0.901f, &COL_CARD_BG );

    // Header bar
    pVideo->Draw_Rect( cx, cy, cw, DLG_HEADER_H, 0.902f, &COL_HEADER_BG );

    // Title text
    if( !title.empty() )
    {
        cGL_Surface *ts = pFont->Render_Text( pFont->m_font_normal, title, COL_TITLE );
        if( ts )
        {
            ts->Blit( cx + DLG_PAD,
                      cy + ( DLG_HEADER_H - static_cast<float>(ts->m_h) ) * 0.5f,
                      0.903f );
            pd.push_back( ts );
        }
    }
}

// ---------------------------------------------------------------------------
// Internal helper: draw a single-line text input field.
// ---------------------------------------------------------------------------
static void Draw_Input_Field( float fx, float fy, float fw, float fh,
                               const std::string &text, bool focused,
                               std::vector<cGL_Surface*> &pd )
{
    // Border
    Color border = focused ? COL_HEADER_BG : COL_BORDER;
    pVideo->Draw_Rect( fx - 2.0f, fy - 2.0f, fw + 4.0f, fh + 4.0f, 0.902f, &border );

    // Background
    Color bg = Color( static_cast<Uint8>(255), static_cast<Uint8>(255), static_cast<Uint8>(255), static_cast<Uint8>(240) );
    pVideo->Draw_Rect( fx, fy, fw, fh, 0.903f, &bg );

    // Text
    if( !text.empty() )
    {
        cGL_Surface *ts = pFont->Render_Text( pFont->m_font_small, text, COL_BODY );
        if( ts )
        {
            ts->Blit( fx + 6.0f,
                      fy + ( fh - static_cast<float>(ts->m_h) ) * 0.5f,
                      0.904f );
            pd.push_back( ts );
        }
    }

    // Cursor bar (blinking based on time)
    if( focused && ( ( SDL_GetTicks() / 500 ) % 2 == 0 ) )
    {
        // measure text width for cursor placement
        int tw = 0;
        if( !text.empty() )
            TTF_SizeUTF8( pFont->m_font_small, text.c_str(), &tw, NULL );

        float cur_x = fx + 6.0f + static_cast<float>(tw) + 1.0f;
        Color cursor_col = COL_BODY;
        pVideo->Draw_Rect( cur_x, fy + 4.0f, 2.0f, fh - 8.0f, 0.905f, &cursor_col );
    }
}

/* *** *** *** *** *** *** *** cDialogBox *** *** *** *** *** *** *** *** *** *** */

cDialogBox :: cDialogBox( void )
{
	finished = 0;
	window = NULL;
	mouse_hide = 0;
}

cDialogBox :: ~cDialogBox( void )
{
}

void cDialogBox :: Init( void )
{
	// ModernUI dialogs don't use CEGUI layouts.
	// Show mouse cursor if it was hidden.
	if( !pMouseCursor->m_active )
	{
		mouse_hide = 1;
		pMouseCursor->Set_Active( 1 );
	}
}

void cDialogBox :: Exit( void )
{
	// Restore cursor state
	if( mouse_hide )
	{
		pMouseCursor->Set_Active( 0 );
	}
}

void cDialogBox :: Draw( void )
{
	Draw_Game();
	pVideo->Render();
	pMouseCursor->Update_Position();
}

void cDialogBox :: Update( void )
{
	pFramerate->Update();
}

/* *** *** *** *** *** *** *** cDialogBox_Text *** *** *** *** *** *** *** *** *** *** */

cDialogBox_Text :: cDialogBox_Text( void )
: cDialogBox()
{
	layout_file = "box_text.layout";
	box_editbox = NULL;
}

cDialogBox_Text :: ~cDialogBox_Text( void )
{
}

void cDialogBox_Text :: Init( std::string title_text )
{
	cDialogBox::Init();
}

std::string cDialogBox_Text :: Enter( std::string default_text,
                                      std::string title_text,
                                      bool auto_no_text /* = 1 */ )
{
	pVideo->Render_Finish();
	Init( title_text );

	std::string text = default_text;
	finished = 0;

	// Show soft keyboard on Android / mobile
	SDL_StartTextInput();

	while( !finished )
	{
		// --- Event handling ---
		ModernUI::Begin_Frame();

		SDL_Event e;
		while( SDL_PollEvent( &e ) )
		{
			ModernUI::Feed_Event( e );

			if( e.type == SDL_QUIT )
			{
				game_exit = 1;
				text = "";
				finished = 1;
			}
			else if( e.type == SDL_KEYDOWN )
			{
				SDL_Keycode sym = e.key.keysym.sym;

				if( sym == SDLK_ESCAPE )
				{
					text = "";
					finished = 1;
				}
				else if( sym == SDLK_RETURN || sym == SDLK_KP_ENTER )
				{
					finished = 1;
				}
				else if( sym == SDLK_BACKSPACE )
				{
					// UTF-8 aware backspace: remove last multi-byte sequence
					if( !text.empty() )
					{
						// Walk backwards past continuation bytes (10xxxxxx)
						size_t pos = text.size() - 1;
						while( pos > 0 && ( text[pos] & 0xC0 ) == 0x80 )
							--pos;
						text.erase( pos );
					}
				}
				else if( sym == SDLK_v && ( e.key.keysym.mod & KMOD_CTRL ) )
				{
					// Ctrl+V paste
					char *clip = SDL_GetClipboardText();
					if( clip )
					{
						text += clip;
						SDL_free( clip );
					}
				}
				else if( auto_no_text && text == default_text )
				{
					// First keystroke clears the default text (non-printable keys also
					// trigger the clear so the user doesn't have to delete manually)
					text = "";
					auto_no_text = 0;
				}
			}
			else if( e.type == SDL_TEXTINPUT )
			{
				// Clear default text on first real input if auto_no_text is set
				if( auto_no_text && text == default_text )
				{
					text = "";
					auto_no_text = 0;
				}
				text += e.text.text;
			}
		}

		// --- Layout ---
		const float DLG_W   = std::min( 380.0f, static_cast<float>(game_res_w) * 0.85f );
		const float DLG_H   = DLG_HEADER_H + DLG_PAD * 3.0f + DLG_INPUT_H + DLG_BTN_H;
		const float cx      = ( static_cast<float>(game_res_w) - DLG_W ) * 0.5f;
		const float cy      = ( static_cast<float>(game_res_h) - DLG_H ) * 0.5f;

		const float field_x = cx + DLG_PAD;
		const float field_y = cy + DLG_HEADER_H + DLG_PAD;
		const float field_w = DLG_W - DLG_PAD * 2.0f;

		const float btn_x   = cx + ( DLG_W - DLG_BTN_W ) * 0.5f;
		const float btn_y   = cy + DLG_HEADER_H + DLG_PAD * 2.0f + DLG_INPUT_H;

		// --- Draw ---
		std::vector<cGL_Surface*> pd;
		Draw_Game();
		Draw_Dialog_Frame( cx, cy, DLG_W, DLG_H, title_text, pd );
		Draw_Input_Field( field_x, field_y, field_w, DLG_INPUT_H, text, true, pd );

		if( ModernUI::Button( btn_x, btn_y, DLG_BTN_W, DLG_BTN_H, "OK", pd ) )
		{
			finished = 1;
		}

		pVideo->Render();
		for( unsigned int i = 0; i < pd.size(); ++i )
			delete pd[i];

		pFramerate->Update();

		// Frame rate cap when vsync is off
		if( !pPreferences->m_video_vsync )
			Correct_Frame_Time( 60 );
	}

	SDL_StopTextInput();
	Exit();
	return text;
}

bool cDialogBox_Text :: Button_window_quit_clicked( const CEGUI::EventArgs &event )
{
	// Legacy CEGUI callback — no longer wired up, but must exist for the header.
	finished = 1;
	return 1;
}

/* *** *** *** *** *** *** *** cDialogBox_Question *** *** *** *** *** *** *** *** *** *** */

cDialogBox_Question :: cDialogBox_Question( void )
: cDialogBox()
{
	layout_file = "box_question.layout";
	box_window = NULL;
	return_value = -1;
}

cDialogBox_Question :: ~cDialogBox_Question( void )
{
}

void cDialogBox_Question :: Init( bool with_cancel )
{
	pVideo->Render_Finish();
	cDialogBox::Init();
}

int cDialogBox_Question :: Enter( std::string text, bool with_cancel /* = 0 */ )
{
	Init( with_cancel );
	return_value = -1;
	finished = 0;

	// Measure text width to size dialog appropriately
	int text_w = 0, text_h = 0;
	if( !text.empty() && pFont->m_font_small )
		TTF_SizeUTF8( pFont->m_font_small, text.c_str(), &text_w, &text_h );

	// Dialog width: fit text with padding, clamped to screen
	const float min_w    = with_cancel ? 280.0f : 240.0f;
	const float DLG_W    = std::min(
		static_cast<float>(game_res_w) * 0.90f,
		std::max( min_w, static_cast<float>(text_w) + DLG_PAD * 4.0f )
	);

	// Number of buttons: Yes + No, optionally + Cancel
	const int   btn_count = with_cancel ? 3 : 2;
	const float total_btn = static_cast<float>(btn_count) * DLG_BTN_W
	                      + static_cast<float>(btn_count - 1) * DLG_PAD;
	const float DLG_H    = DLG_HEADER_H + DLG_PAD * 3.0f
	                      + static_cast<float>(text_h > 0 ? text_h : 20)
	                      + DLG_BTN_H;

	while( !finished )
	{
		// --- Event handling ---
		ModernUI::Begin_Frame();

		SDL_Event e;
		while( SDL_PollEvent( &e ) )
		{
			ModernUI::Feed_Event( e );

			if( e.type == SDL_QUIT )
			{
				game_exit = 1;
				return_value = with_cancel ? -1 : 0;
				finished = 1;
			}
			else if( e.type == SDL_KEYDOWN )
			{
				SDL_Keycode sym = e.key.keysym.sym;
				if( sym == SDLK_ESCAPE )
				{
					return_value = with_cancel ? -1 : 0;
					finished = 1;
				}
				else if( sym == SDLK_RETURN || sym == SDLK_KP_ENTER )
				{
					return_value = 1;
					finished = 1;
				}
			}
		}

		// --- Layout ---
		const float cx = ( static_cast<float>(game_res_w) - DLG_W ) * 0.5f;
		const float cy = ( static_cast<float>(game_res_h) - DLG_H ) * 0.5f;

		// Question text position
		const float txt_x = cx + DLG_PAD;
		const float txt_y = cy + DLG_HEADER_H + DLG_PAD;

		// Button row: centred
		const float btn_row_x = cx + ( DLG_W - total_btn ) * 0.5f;
		const float btn_row_y = cy + DLG_H - DLG_BTN_H - DLG_PAD;

		// --- Draw ---
		std::vector<cGL_Surface*> pd;
		Draw_Game();
		Draw_Dialog_Frame( cx, cy, DLG_W, DLG_H, "", pd );

		// Question text
		if( !text.empty() )
		{
			cGL_Surface *ts = pFont->Render_Text( pFont->m_font_small, text, COL_BODY );
			if( ts )
			{
				// Centre horizontally
				float tx = cx + ( DLG_W - static_cast<float>(ts->m_w) ) * 0.5f;
				ts->Blit( tx, txt_y, 0.903f );
				pd.push_back( ts );
			}
		}

		// Button: Yes
		float bx = btn_row_x;
		if( ModernUI::Button( bx, btn_row_y, DLG_BTN_W, DLG_BTN_H, "Yes", pd ) )
		{
			return_value = 1;
			finished = 1;
		}

		// Button: No
		bx += DLG_BTN_W + DLG_PAD;
		if( ModernUI::Button( bx, btn_row_y, DLG_BTN_W, DLG_BTN_H, "No", pd ) )
		{
			return_value = 0;
			finished = 1;
		}

		// Button: Cancel (optional)
		if( with_cancel )
		{
			bx += DLG_BTN_W + DLG_PAD;
			if( ModernUI::Button( bx, btn_row_y, DLG_BTN_W, DLG_BTN_H, "Cancel", pd ) )
			{
				return_value = -1;
				finished = 1;
			}
		}

		pVideo->Render();
		for( unsigned int i = 0; i < pd.size(); ++i )
			delete pd[i];

		pFramerate->Update();

		// Frame rate cap when vsync is off
		if( !pPreferences->m_video_vsync )
			Correct_Frame_Time( 60 );
	}

	Exit();
	return return_value;
}

bool cDialogBox_Question :: Button_yes_clicked( const CEGUI::EventArgs &event )
{
	// Legacy CEGUI callback — no longer wired up.
	return_value = 1;
	finished = 1;
	return 1;
}

bool cDialogBox_Question :: Button_no_clicked( const CEGUI::EventArgs &event )
{
	return_value = 0;
	finished = 1;
	return 1;
}

bool cDialogBox_Question :: Button_cancel_clicked( const CEGUI::EventArgs &event )
{
	return_value = -1;
	finished = 1;
	return 1;
}

/* *** *** *** *** *** *** *** Functions *** *** *** *** *** *** *** *** *** *** */

void Gui_Handle_Time( void )
{
	static float last_time_pulse = 0;

	// get current "run-time" in seconds
	float t = 0.001f * SDL_GetTicks();

	// inject the time that passed since the last call
	CEGUI::System::getSingleton().injectTimePulse( t - last_time_pulse );

	// store the new time as the last time
	last_time_pulse = t;
}

void Draw_Static_Text( const std::string &text, const Color *color_text /* = &white */, const Color *color_bg /* = NULL */, bool wait_for_input /* = 1 */ )
{
	// fixme : Can't handle multiple lines of text. Change to MultiLineEditbox or use HorzFormatting=WordWrapLeftAligned property.
	bool draw = 1;

	// Statictext window
	CEGUI::Window *window_statictext = CEGUI::WindowManager::getSingleton().loadLayoutFromFile( "statictext.layout" );
	pGuiSystem->getDefaultGUIContext().getRootWindow()->addChild( window_statictext );
	// get default text
	CEGUI::Window *text_default = static_cast<CEGUI::Window *>(CEGUI_GetChild( pGuiSystem->getDefaultGUIContext().getRootWindow(), "text_default" ));

	// set text
	text_default->setProperty( "TextColours", CEGUI::PropertyHelper<CEGUI::Colour>::toString( CEGUI::Colour( static_cast<float>(color_text->red) / 255, static_cast<float>(color_text->green) / 255, static_cast<float>(color_text->blue) / 255, 1 ) ) );
	CEGUI::String gui_text = reinterpret_cast<const CEGUI::utf8*>(text.c_str());
	text_default->setText( gui_text );

	// align text
	CEGUI::Font *font = &CEGUI::FontManager::getSingleton().get( "bluebold_medium" );
	float text_width = font->getTextExtent( gui_text ) * global_downscalex;

	text_default->setWidth( CEGUI::UDim( 0, ( text_width + 15 ) * global_upscalex ) );
	text_default->setXPosition( CEGUI::UDim( 0, ( game_res_w * 0.5f - text_width * 0.5f ) * global_upscalex ) );
	text_default->moveToFront();

	float text_height = font->getLineSpacing();
	text_height *= 1 + std::count(text.begin(), text.end(), '\n');
	// set window height
	text_default->setHeight( CEGUI::UDim( 0, text_height + ( 12 * global_upscaley ) ) );

	while( draw )
	{
		Draw_Game();

		// draw background
		if( color_bg )
		{
			// create request
			cRect_Request *request = new cRect_Request();

			pVideo->Draw_Rect( NULL, 0.9f, color_bg, request );
			request->m_render_count = wait_for_input ? 4 : 1;

			// add request
			pRenderer->Add( request );
		}

		pVideo->Render();

		if( wait_for_input )
		{
			while( SDL_PollEvent( &input_event ) )
			{
				if( input_event.type == SDL_KEYDOWN || input_event.type == SDL_JOYBUTTONDOWN || input_event.type == SDL_MOUSEBUTTONDOWN )
				{
					draw = 0;
				}
			}

			// if vsync is disabled then limit the fps to reduce the CPU usage
			if( !pPreferences->m_video_vsync )
			{
				Correct_Frame_Time( 100 );
			}
		}
		else
		{
			draw = 0;
		}

		pFramerate->Update();
	}

	// Clear possible active input
	if( wait_for_input )
	{
		Clear_Input_Events();
	}

	pGuiSystem->getDefaultGUIContext().getRootWindow()->removeChild( window_statictext );
	CEGUI::WindowManager::getSingleton().destroyWindow( window_statictext );
}

std::string Box_Text_Input( const std::string &default_text, const std::string &title_text, bool auto_no_text /* = 1 */ )
{
	cDialogBox_Text *box = new cDialogBox_Text();
	std::string return_value = box->Enter( default_text, title_text, auto_no_text );
	delete box;

	return return_value;
}

int Box_Question( const std::string &text, bool with_cancel /* = 0 */ )
{
	cDialogBox_Question *box = new cDialogBox_Question();
	int return_value = box->Enter( text, with_cancel );
	delete box;

	return return_value;
}

std::string Get_Clipboard_Content( void )
{
	std::string content;
#ifdef _WIN32
	if( OpenClipboard( NULL ) )
	{
		bool ucs2 = 0;
		if( IsClipboardFormatAvailable( CF_UNICODETEXT ) )
		{
			ucs2 = 1;
		}

		HANDLE h;

		// Both have line ends as CR-LF and a null character at the end of the data.
		if( ucs2 )
		{
			// CF_UNICODETEXT is UCS-2
			h = GetClipboardData( CF_UNICODETEXT );
		}
		else
		{
			// CF_TEXT is Windows-1252
			h = GetClipboardData( CF_TEXT );
		}

		// no handle
		if( !h )
		{
			printf( "Could not get clipboard data\n" );
			CloseClipboard();
			return content;
		}

		// get content
		if( ucs2 )
		{
			content = ucs2_to_utf8(static_cast<wchar_t *>(GlobalLock( h )));
		}
		else
		{
			content = static_cast<char *>(GlobalLock( h ));
		}

		// clean up
		GlobalUnlock( h );
		CloseClipboard();
	}
#elif __APPLE__
	// not tested
	ScrapRef scrap;
	if( ::GetCurrentScrap( &scrap ) != noErr )
	{
		return false;
	}

	Size bytecount = 0;
	OSStatus status = ::GetScrapFlavorSize( scrap, kScrapFlavorTypeText, &bytecount );
	if( status != noErr )
	{
		return false;
	}

	char *buffer = new char[bytecount];
	if( ::GetScrapFlavorData( scrap, kScrapFlavorTypeText, &bytecount, buffer ) == noErr )
	{
		content = static_cast<char *>(buffer);
	}

	delete[] buffer;
#elif __unix__
	char *clipboard = SDL_GetClipboardText();
	if( clipboard )
	{
		content = clipboard;
		SDL_free( clipboard );
	}
#endif
	return content;
}

void Set_Clipboard_Content( std::string str )
{
#ifdef _WIN32
	if( OpenClipboard( NULL ) )
	{
		if( !EmptyClipboard() )
		{
			printf( "Failed to empty clipboard\n" );
			return;
		}

		unsigned int length = ( str.length() + 1 ) * sizeof(std::string::allocator_type);
		HANDLE h = GlobalAlloc( (GMEM_MOVEABLE|GMEM_DDESHARE|GMEM_ZEROINIT), length );

		if( !h )
		{
			printf( "Could not allocate clipboard memory\n" );
			return;
		}

		void *data = GlobalLock( h );

		if( !data )
		{
			GlobalFree( h );
			CloseClipboard();
			printf( "Could not lock clipboard memory\n" );
			return;
		}

		memcpy( data, str.c_str(), length );
		GlobalUnlock( h );

		HANDLE data_result = SetClipboardData( CF_TEXT, h );

		if( !data_result )
		{
			GlobalFree( h );
			CloseClipboard();
			printf( "Could not set clipboard data\n" );
		}

		CloseClipboard();
	}
#elif __APPLE__
	// not implemented
#elif __unix__
	SDL_SetClipboardText( str.c_str() );
#endif
}

bool GUI_Copy_To_Clipboard( bool cut )
{
	CEGUI::Window *sheet = pGuiSystem->getDefaultGUIContext().getRootWindow();

	// no sheet
	if( !sheet )
	{
		return 0;
	}

	CEGUI::Window *window_active = sheet->getActiveChild();

	// no active window
	if( !window_active )
	{
		return 0;
	}

	CEGUI::String sel_text;
	const CEGUI::String &type = window_active->getType();

	// MultiLineEditbox
	if( type.find( "/MultiLineEditbox" ) != CEGUI::String::npos )
	{
		CEGUI::MultiLineEditbox *editbox = static_cast<CEGUI::MultiLineEditbox*>(window_active);
		CEGUI::String::size_type beg = editbox->getSelectionStartIndex();
		CEGUI::String::size_type len = editbox->getSelectionLength();
		sel_text = editbox->getText().substr( beg, len ).c_str();

		// if cutting
		if( cut )
		{
			if( editbox->isReadOnly() )
			{
				return 0;
			}

			CEGUI::String new_text = editbox->getText();
			editbox->setText( new_text.erase( beg, len ) );
		}
	}
	// Editbox
	else if( type.find( "/Editbox" ) != CEGUI::String::npos )
	{
		CEGUI::Editbox *editbox = static_cast<CEGUI::Editbox*>(window_active);
		CEGUI::String::size_type beg = editbox->getSelectionStartIndex();
		CEGUI::String::size_type len = editbox->getSelectionLength();
		sel_text = editbox->getText().substr( beg, len ).c_str();

		// if cutting
		if( cut )
		{
			if( editbox->isReadOnly() )
			{
				return 0;
			}

			CEGUI::String new_text = editbox->getText();
			editbox->setText( new_text.erase( beg, len ) );
		}
	}
	else
	{
		return 0;
	}

	Set_Clipboard_Content( sel_text.c_str() );
	return 1;
}

bool GUI_Paste_From_Clipboard( void )
{
	CEGUI::Window *sheet = pGuiSystem->getDefaultGUIContext().getRootWindow();

	// no sheet
	if( !sheet )
	{
		return 0;
	}

	CEGUI::Window *window_active = sheet->getActiveChild();

	// no active window
	if( !window_active )
	{
		return 0;
	}

	const CEGUI::String &type = window_active->getType();

	// MultiLineEditbox
	if( type.find( "/MultiLineEditbox" ) != CEGUI::String::npos )
	{
		CEGUI::MultiLineEditbox *editbox = static_cast<CEGUI::MultiLineEditbox*>(window_active);

		if( editbox->isReadOnly() )
		{
			return 0;
		}

		CEGUI::String::size_type beg = editbox->getSelectionStartIndex();
		CEGUI::String::size_type len = editbox->getSelectionLength();

		CEGUI::String new_text = editbox->getText();
		// erase selected text
		new_text.erase( beg, len );

		// get clipboard text
		CEGUI::String clipboard_text = reinterpret_cast<const CEGUI::utf8*>(Get_Clipboard_Content().c_str());
		// set new text
		editbox->setText( new_text.insert( beg, clipboard_text ) );
		// set new carat index
		editbox->setCaretIndex( editbox->getCaretIndex() + clipboard_text.length() );
	}
	// Editbox
	else if( type.find( "/Editbox" ) != CEGUI::String::npos )
	{
		CEGUI::Editbox *editbox = static_cast<CEGUI::Editbox*>(window_active);

		if( editbox->isReadOnly() )
		{
			return 0;
		}

		CEGUI::String::size_type beg = editbox->getSelectionStartIndex();
		CEGUI::String::size_type len = editbox->getSelectionLength();

		CEGUI::String new_text = editbox->getText();
		// erase selected text
		new_text.erase( beg, len );

		// get clipboard text
		CEGUI::String clipboard_text = reinterpret_cast<const CEGUI::utf8*>(Get_Clipboard_Content().c_str());
		// set new text
		editbox->setText( new_text.insert( beg, clipboard_text ) );
		// set new carat index
		editbox->setCaretIndex( editbox->getCaretIndex() + clipboard_text.length() );
	}
	else
	{
		return 0;
	}

	return 1;
}

/* *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** */

} // namespace SMC
