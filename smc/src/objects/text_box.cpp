/***************************************************************************
 * text_box.cpp  -  box speaking to you
 *
 * Copyright (C) 2007 - 2011 Florian Richter
 ***************************************************************************/
/*
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "../objects/text_box.h"
#include "../core/framerate.h"
#include "../core/game_core.h"
#include "../user/preferences.h"
#include "../input/joystick.h"
#include "../core/main.h"
#include "../input/keyboard.h"
#include "../core/i18n.h"
#include "../audio/audio.h"
#include "../level/level.h"
// CEGUI — only the base XML/editor infrastructure; MultiLineEditbox removed (M9)
#include <CEGUI/WindowManager.h>
#include <CEGUI/widgets/Editbox.h>

namespace SMC
{

/* *** *** *** *** *** *** *** *** cText_Box *** *** *** *** *** *** *** *** *** */

static unsigned int text_box_window_width = 300;
static unsigned int text_box_window_height = 28;

cText_Box :: cText_Box( cSprite_Manager *sprite_manager )
: cBaseBox( sprite_manager )
{
	cText_Box::Init();
}

cText_Box :: cText_Box( CEGUI::XMLAttributes &attributes, cSprite_Manager *sprite_manager )
: cBaseBox( sprite_manager )
{
	cText_Box::Init();
	cText_Box::Load_From_XML( attributes );
}

cText_Box :: ~cText_Box( void )
{

}

void cText_Box :: Init( void )
{
	m_type = TYPE_TEXT_BOX;
	box_type = m_type;
	m_can_be_on_ground = 0;

	// default is infinite times activate-able
	Set_Useable_Count( -1, 1 );
	// Spinbox Animation
	Set_Animation_Type( "Default" );

	// todo : editor image needed
	//item_image = NULL;

	Create_Name();
}

cText_Box *cText_Box :: Copy( void ) const
{
	cText_Box *text_box = new cText_Box( m_sprite_manager );
	text_box->Set_Pos( m_start_pos_x, m_start_pos_y );
	text_box->Set_Text( m_text );
	text_box->Set_Invisible( m_box_invisible );
	return text_box;
}

void cText_Box :: Load_From_XML( CEGUI::XMLAttributes &attributes )
{
	cBaseBox::Load_From_XML( attributes );

	// text
	Set_Text( xml_string_to_string( attributes.getValueAsString( "text" ).c_str() ) );
}

void cText_Box :: Save_To_XML( CEGUI::XMLSerializer &stream )
{
	// begin
	stream.openTag( m_type_name );

	cBaseBox::Save_To_XML( stream );

	// text
	Write_Property( stream, "text", m_text );

	// end
	stream.closeTag();
}

/* Activate: display the text using the ModernUI cHelpCard overlay.
 * No CEGUI widgets are created at runtime — pure SDL2/OpenGL rendering.
 * The player dismisses the card with Enter, Space, Escape, action key,
 * joystick button, a tap/click on the "Got it" button, or a tap/click
 * outside the card area.
 */
void cText_Box :: Activate( void )
{
	const std::string &text = m_text.empty() ? "(No text set. Use the editor to add text.)" : m_text;
	cHelpCard card( "Hint!", text, ICON_HINT );
	card.Run();
}

void cText_Box :: Update( void )
{
	if( !m_valid_update || !Is_In_Range() )
	{
		return;
	}

	cBaseBox::Update();
}

void cText_Box :: Set_Text( const std::string &str_text )
{
	m_text = str_text;
}

/* Editor_Activate: use a single-line CEGUI Editbox for the text property.
 * MultiLineEditbox (M9: removed) has been replaced with the same Editbox
 * widget used by cLevel_Exit and other objects — lighter, no separate
 * CEGUI module dependency, and consistent with the rest of the editor UI.
 * The window height constant above is updated accordingly (200→28 px).
 */
void cText_Box :: Editor_Activate( void )
{
	// BaseBox Settings first
	cBaseBox::Editor_Activate();

	// get window manager
	CEGUI::WindowManager &wmgr = CEGUI::WindowManager::getSingleton();

	// text — single-line Editbox replaces the old MultiLineEditbox
	CEGUI::Editbox *editbox = static_cast<CEGUI::Editbox *>(wmgr.createWindow( "TaharezLook/Editbox", "text_box_text" ));
	Editor_Add( UTF8_("Text"), UTF8_("Text to display when activated"), editbox, static_cast<float>(text_box_window_width), static_cast<float>(text_box_window_height) );

	editbox->setText( reinterpret_cast<const CEGUI::utf8*>(m_text.c_str()) );
	editbox->subscribeEvent( CEGUI::Editbox::EventTextChanged, CEGUI::Event::Subscriber( &cText_Box::Editor_Text_Text_Changed, this ) );

	// init
	Editor_Init();
}

bool cText_Box :: Editor_Text_Text_Changed( const CEGUI::EventArgs &event )
{
	const CEGUI::WindowEventArgs &windowEventArgs = static_cast<const CEGUI::WindowEventArgs&>( event );
	std::string str_text = static_cast<CEGUI::Editbox *>( windowEventArgs.window )->getText().c_str();

	Set_Text( str_text );

	return 1;
}

/* *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** */

} // namespace SMC
