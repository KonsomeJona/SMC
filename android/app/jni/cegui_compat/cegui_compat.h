/***************************************************************************
 * cegui_compat.h  -  Minimal CEGUI-compatible surface for the Android build
 *
 * SMC was written against CEGUI 0.7/0.8 and uses it for two very different
 * things:
 *
 *   1. XML: every level, world and savegame is parsed and written through
 *      CEGUI::XMLAttributes / XMLSerializer / XMLHandler. This part has to
 *      actually work, so it is reimplemented here on top of TinyXML2.
 *
 *   2. The in-game editor UI (Combobox, Editbox, PushButton, ...). The
 *      Android build has no editor, so those types exist only so the
 *      Editor_* methods scattered across the object classes still compile.
 *      They do nothing.
 *
 * Keeping the CEGUI names means the ~50 game source files that include
 * "CEGUIXMLAttributes.h" & co. are left untouched: the shim headers next to
 * this file map every historical CEGUI include path onto this one.
 ***************************************************************************/
#ifndef SMC_CEGUI_COMPAT_H
#define SMC_CEGUI_COMPAT_H

#include <string>
#include <map>
#include <vector>
#include <sstream>
#include <stdexcept>
#include <cstdio>

namespace CEGUI
{

typedef unsigned char utf8;

/* *** String *** */

// Derives from std::string so the thousands of implicit conversions in the
// game code keep working. CEGUI::String is never deleted polymorphically.
class String : public std::string
{
public:
	String( void ) {}
	String( const char *s ) : std::string( s ? s : "" ) {}
	String( const std::string &s ) : std::string( s ) {}
	String( const utf8 *s ) : std::string( reinterpret_cast<const char *>( s ) ) {}
	String( size_type n, char c ) : std::string( n, c ) {}

	// UTF8_( "x" ) expands to a CEGUI::utf8 * (see i18n.h), and the game
	// compares against it directly. std::string has no such overload.
	using std::string::compare;
	int compare( const utf8 *s ) const
	{
		return std::string::compare( reinterpret_cast<const char *>( s ) );
	}
};

/* *** colour *** */

class colour
{
public:
	colour( float r = 0.0f, float g = 0.0f, float b = 0.0f, float a = 1.0f )
		: m_red( r ), m_green( g ), m_blue( b ), m_alpha( a ) {}

	float getRed( void ) const   { return m_red; }
	float getGreen( void ) const { return m_green; }
	float getBlue( void ) const  { return m_blue; }
	float getAlpha( void ) const { return m_alpha; }

	float m_red, m_green, m_blue, m_alpha;
};

/* *** Exceptions *** */

class Exception : public std::runtime_error
{
public:
	Exception( const String &msg = "" ) : std::runtime_error( msg.c_str() ), m_message( msg ) {}
	const String &getMessage( void ) const { return m_message; }
	const char *what( void ) const throw() { return m_message.c_str(); }
private:
	String m_message;
};

class UnknownObjectException  : public Exception { public: UnknownObjectException( const String &m = "" ) : Exception( m ) {} };
class InvalidRequestException : public Exception { public: InvalidRequestException( const String &m = "" ) : Exception( m ) {} };
class FileIOException         : public Exception { public: FileIOException( const String &m = "" ) : Exception( m ) {} };
class GenericException        : public Exception { public: GenericException( const String &m = "" ) : Exception( m ) {} };

/* *** XMLAttributes *** */

// A flat name -> value map, exactly what the game uses it for: it is filled
// by the parser on the way in, and by the game itself on the way out.
class XMLAttributes
{
public:
	XMLAttributes( void ) {}

	void add( const String &name, const String &value );
	void remove( const String &name );
	bool exists( const String &name ) const;

	size_t getCount( void ) const;
	const String &getName( size_t index ) const;
	const String &getValue( size_t index ) const;
	const String &getValue( const String &name ) const;

	String getValueAsString( const String &name, const String &def = "" ) const;
	int    getValueAsInteger( const String &name, int def = 0 ) const;
	float  getValueAsFloat( const String &name, float def = 0.0f ) const;
	bool   getValueAsBool( const String &name, bool def = false ) const;

private:
	std::vector< std::pair< String, String > > m_attrs;
	int Find( const String &name ) const;
};

/* *** XMLSerializer *** */

// Fluent writer: stream.openTag("x").attribute("a","b").closeTag()
class XMLSerializer
{
public:
	explicit XMLSerializer( std::ostream &out, size_t indent = 1 );
	~XMLSerializer( void );

	XMLSerializer &openTag( const String &name );
	XMLSerializer &closeTag( void );
	XMLSerializer &attribute( const String &name, const String &value );
	XMLSerializer &text( const String &value );

private:
	std::ostream &m_out;
	size_t m_indent;
	std::vector< String > m_stack;
	bool m_tag_open;   // still inside "<name ", attributes may follow
	bool m_has_child;

	void Close_Opening_Tag( void );
};

/* *** XMLHandler *** */

class XMLHandler
{
public:
	virtual ~XMLHandler( void ) {}
	virtual void elementStart( const String &element, const XMLAttributes &attributes ) {}
	virtual void elementEnd( const String &element ) {}
	virtual void text( const String &text ) {}
};

/* *** XMLParser — TinyXML2 driver *** */

class XMLParser
{
public:
	virtual ~XMLParser( void ) {}

	// schema and resource group are accepted for source compatibility and
	// ignored: there is no validating parser on Android.
	void parseXMLFile( XMLHandler &handler, const String &filename,
		const String &schemaName = "", const String &resourceGroup = "" );
};

/* *** System — only getXMLParser() is ever called on Android *** */

class Window;

// main.cpp forwards SDL text and wheel events to the GUI context. On Android
// pGuiSystem stays NULL and those calls are already guarded, so this only has
// to compile.
class GUIContext
{
public:
	void injectChar( unsigned int ) {}
	void injectMouseWheelChange( float ) {}
	Window *getRootWindow( void ) const { return 0; }
};

class System
{
public:
	static System &getSingleton( void );
	XMLParser *getXMLParser( void ) { return &m_parser; }
	GUIContext &getDefaultGUIContext( void ) { return m_gui_context; }
	void notifyDisplaySizeChanged( const class Size & ) {}

private:
	XMLParser m_parser;
	GUIContext m_gui_context;
};

/* *** Editor widgets — inert stubs *** */
//
// Everything below exists so the editor code paths compile. No widget is
// ever created on Android: WindowManager::createWindow() returns NULL and
// the Editor_* methods are never called (the editor is disabled).

class EventArgs { public: virtual ~EventArgs( void ) {} };
class Window;
class WindowEventArgs : public EventArgs { public: WindowEventArgs( Window *w = 0 ) : window( w ) {} Window *window; };

class Event
{
public:
	// The game writes Event::Subscriber( &cEato::Editor_Direction_Select, this ).
	// The pair is accepted and dropped — nothing ever fires on Android.
	class Subscriber
	{
	public:
		Subscriber( void ) {}
		template< typename TMethod, typename TObject >
		Subscriber( TMethod, TObject ) {}
	};
};

class ListboxItem
{
public:
	ListboxItem( const String &text = "" ) : m_text( text ) {}
	virtual ~ListboxItem( void ) {}
	const String &getText( void ) const { return m_text; }
	void setText( const String &t ) { m_text = t; }
	void setSelectionColours( const colour & ) {}
	void setSelectionBrushImage( const String &, const String & ) {}
private:
	String m_text;
};

class ListboxTextItem : public ListboxItem
{
public:
	ListboxTextItem( const String &text = "", unsigned int id = 0 ) : ListboxItem( text ), m_id( id ) {}
	unsigned int getID( void ) const { return m_id; }
private:
	unsigned int m_id;
};

class Window
{
public:
	virtual ~Window( void ) {}
	void setText( const String & ) {}
	String getText( void ) const { return String(); }
	void setPosition( float, float ) {}
	void setSize( float, float ) {}
	void setVisible( bool ) {}
	void setEnabled( bool ) {}
	void setAlpha( float ) {}
	void addChildWindow( Window * ) {}
	void removeChildWindow( Window * ) {}
	Window *getChild( const String & ) { return 0; }
	String getName( void ) const { return String(); }
	void subscribeEvent( const String &, const Event::Subscriber & ) {}
	void setProperty( const String &, const String & ) {}
	String getProperty( const String & ) const { return String(); }
};

class Editbox : public Window
{
public:
	static const String EventTextChanged;
	void setValidationString( const String & ) {}
	void setMaxTextLength( size_t ) {}
};

class MultiLineEditbox : public Window { public: static const String EventTextChanged; };

class Combobox : public Window
{
public:
	static const String EventListSelectionAccepted;
	void addItem( ListboxItem * ) {}
	ListboxItem *getSelectedItem( void ) const { return 0; }
	void setReadOnly( bool ) {}
	void setSortingEnabled( bool ) {}
};

class Listbox : public Window
{
public:
	static const String EventSelectionChanged;
	void addItem( ListboxItem * ) {}
	ListboxItem *getFirstSelectedItem( void ) const { return 0; }
};

class PushButton : public Window { public: static const String EventClicked; };
class Checkbox   : public Window { public: static const String EventCheckStateChanged; void setSelected( bool ) {} bool isSelected( void ) const { return false; } };
class ProgressBar: public Window { public: void setProgress( float ) {} };
class Slider     : public Window { public: static const String EventValueChanged; void setCurrentValue( float ) {} float getCurrentValue( void ) const { return 0.0f; } };
class Spinner    : public Window { public: static const String EventValueChanged; void setCurrentValue( float ) {} float getCurrentValue( void ) const { return 0.0f; } };
class TabControl : public Window {};
class FrameWindow: public Window {};
class Scrollbar  : public Window {};

/* *** Geometry + property helpers *** */

class Size
{
public:
	Size( float w = 0.0f, float h = 0.0f ) : d_width( w ), d_height( h ) {}
	float d_width, d_height;
};
typedef Size Sizef;

class Rect
{
public:
	Rect( float l = 0.0f, float t = 0.0f, float r = 0.0f, float b = 0.0f )
		: d_left( l ), d_top( t ), d_right( r ), d_bottom( b ) {}
	float d_left, d_top, d_right, d_bottom;
};
typedef Rect Rectf;

class Imageset {};
class GeometryBuffer {};
class OpenGLRenderer {};

// editor.h derives cEditor_CEGUI_Texture from this one.
class Texture {};
class OpenGLTexture : public Texture {};

// Only the four converters the game actually calls.
class PropertyHelper
{
public:
	static String intToString( int val )           { return To_String( val ); }
	static String uintToString( unsigned int val ) { return To_String( val ); }
	static String floatToString( float val )       { return To_String( val ); }
	static colour stringToColour( const String & ) { return colour(); }

private:
	template< typename T >
	static String To_String( T val )
	{
		std::ostringstream os;
		os << val;
		return String( os.str() );
	}
};

class WindowManager
{
public:
	static WindowManager &getSingleton( void );
	Window *createWindow( const String &, const String & = "" ) { return 0; }
	Window *getWindow( const String & ) { return 0; }
	void destroyWindow( Window * ) {}
	bool isWindowPresent( const String & ) const { return false; }
};

class FontManager
{
public:
	static FontManager &getSingleton( void );
	bool isDefined( const String & ) const { return false; }
};

} // namespace CEGUI

#endif // SMC_CEGUI_COMPAT_H
