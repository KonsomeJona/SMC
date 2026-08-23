/***************************************************************************
 * cegui_compat.cpp  -  CEGUI XML layer reimplemented on TinyXML2
 *
 * See cegui_compat.h. Only the XML half has behaviour; the widget stubs are
 * header-only, apart from the event-name constants defined at the bottom.
 ***************************************************************************/
#include "cegui_compat.h"
#include "tinyxml2.h"

#include <cstdlib>
#include <cstring>
#include <ostream>

namespace CEGUI
{

/* *** XMLAttributes *** */

int XMLAttributes :: Find( const String &name ) const
{
	for( size_t i = 0; i < m_attrs.size(); ++i )
	{
		if( m_attrs[i].first == name )
		{
			return static_cast<int>( i );
		}
	}

	return -1;
}

void XMLAttributes :: add( const String &name, const String &value )
{
	int pos = Find( name );

	// CEGUI's add() overwrites an existing attribute of the same name.
	if( pos >= 0 )
	{
		m_attrs[pos].second = value;
		return;
	}

	m_attrs.push_back( std::make_pair( name, value ) );
}

void XMLAttributes :: remove( const String &name )
{
	int pos = Find( name );

	if( pos >= 0 )
	{
		m_attrs.erase( m_attrs.begin() + pos );
	}
}

bool XMLAttributes :: exists( const String &name ) const
{
	return Find( name ) >= 0;
}

size_t XMLAttributes :: getCount( void ) const
{
	return m_attrs.size();
}

const String &XMLAttributes :: getName( size_t index ) const
{
	static const String empty;
	return index < m_attrs.size() ? m_attrs[index].first : empty;
}

const String &XMLAttributes :: getValue( size_t index ) const
{
	static const String empty;
	return index < m_attrs.size() ? m_attrs[index].second : empty;
}

const String &XMLAttributes :: getValue( const String &name ) const
{
	static const String empty;
	int pos = Find( name );
	return pos >= 0 ? m_attrs[pos].second : empty;
}

String XMLAttributes :: getValueAsString( const String &name, const String &def ) const
{
	int pos = Find( name );
	return pos >= 0 ? m_attrs[pos].second : def;
}

int XMLAttributes :: getValueAsInteger( const String &name, int def ) const
{
	int pos = Find( name );

	if( pos < 0 )
	{
		return def;
	}

	// strtol, not atoi: a malformed value must fall back to the default
	// instead of silently becoming 0 (level files do contain junk).
	const char *str = m_attrs[pos].second.c_str();
	char *end = 0;
	long val = std::strtol( str, &end, 10 );

	return end == str ? def : static_cast<int>( val );
}

float XMLAttributes :: getValueAsFloat( const String &name, float def ) const
{
	int pos = Find( name );

	if( pos < 0 )
	{
		return def;
	}

	const char *str = m_attrs[pos].second.c_str();
	char *end = 0;
	float val = std::strtof( str, &end );

	return end == str ? def : val;
}

bool XMLAttributes :: getValueAsBool( const String &name, bool def ) const
{
	int pos = Find( name );

	if( pos < 0 )
	{
		return def;
	}

	const String &val = m_attrs[pos].second;

	// CEGUI accepts both spellings; SMC level files use "1"/"0".
	if( val == "true" || val == "True" || val == "1" || val == "yes" )
	{
		return true;
	}

	if( val == "false" || val == "False" || val == "0" || val == "no" )
	{
		return false;
	}

	return def;
}

/* *** XMLSerializer *** */

XMLSerializer :: XMLSerializer( std::ostream &out, size_t indent )
: m_out( out ), m_indent( indent ), m_tag_open( 0 ), m_has_child( 0 )
{
	m_out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
}

XMLSerializer :: ~XMLSerializer( void )
{
	// An unbalanced document would be silently truncated otherwise.
	while( !m_stack.empty() )
	{
		closeTag();
	}
}

void XMLSerializer :: Close_Opening_Tag( void )
{
	if( m_tag_open )
	{
		m_out << ">\n";
		m_tag_open = 0;
	}
}

XMLSerializer &XMLSerializer :: openTag( const String &name )
{
	Close_Opening_Tag();

	for( size_t i = 0; i < m_stack.size() * m_indent; ++i )
	{
		m_out << "    ";
	}

	m_out << "<" << name;
	m_stack.push_back( name );
	m_tag_open = 1;

	return *this;
}

XMLSerializer &XMLSerializer :: attribute( const String &name, const String &value )
{
	if( !m_tag_open )
	{
		return *this;
	}

	m_out << " " << name << "=\"";

	// Escape the five XML entities — level names and messages are free text.
	for( size_t i = 0; i < value.size(); ++i )
	{
		char c = value[i];

		switch( c )
		{
			case '&':  m_out << "&amp;";  break;
			case '<':  m_out << "&lt;";   break;
			case '>':  m_out << "&gt;";   break;
			case '"':  m_out << "&quot;"; break;
			case '\'': m_out << "&apos;"; break;
			default:   m_out << c;        break;
		}
	}

	m_out << "\"";

	return *this;
}

XMLSerializer &XMLSerializer :: text( const String &value )
{
	Close_Opening_Tag();
	m_out << value;

	return *this;
}

XMLSerializer &XMLSerializer :: closeTag( void )
{
	if( m_stack.empty() )
	{
		return *this;
	}

	String name = m_stack.back();
	m_stack.pop_back();

	if( m_tag_open )
	{
		// Nothing was written inside: self-closing tag.
		m_out << " />\n";
		m_tag_open = 0;
	}
	else
	{
		for( size_t i = 0; i < m_stack.size() * m_indent; ++i )
		{
			m_out << "    ";
		}

		m_out << "</" << name << ">\n";
	}

	return *this;
}

/* *** XMLParser *** */

namespace
{

void Walk_Element( tinyxml2::XMLElement *element, XMLHandler &handler )
{
	for( ; element; element = element->NextSiblingElement() )
	{
		XMLAttributes attributes;

		for( const tinyxml2::XMLAttribute *attr = element->FirstAttribute(); attr; attr = attr->Next() )
		{
			attributes.add( attr->Name(), attr->Value() ? attr->Value() : "" );
		}

		handler.elementStart( element->Name(), attributes );

		const char *text = element->GetText();

		if( text )
		{
			handler.text( text );
		}

		Walk_Element( element->FirstChildElement(), handler );

		handler.elementEnd( element->Name() );
	}
}

} // anonymous namespace

void XMLParser :: parseXMLFile( XMLHandler &handler, const String &filename,
	const String &/* schemaName */, const String &/* resourceGroup */ )
{
	tinyxml2::XMLDocument doc;

	if( doc.LoadFile( filename.c_str() ) != tinyxml2::XML_SUCCESS )
	{
		// The game catches CEGUI::Exception around every parseXMLFile call.
		throw FileIOException( String( "Could not parse " ) + filename +
			" : " + ( doc.ErrorStr() ? doc.ErrorStr() : "unknown error" ) );
	}

	Walk_Element( doc.RootElement(), handler );
}

/* *** System *** */

System &System :: getSingleton( void )
{
	static System instance;
	return instance;
}

/* *** Managers *** */

WindowManager &WindowManager :: getSingleton( void )
{
	static WindowManager instance;
	return instance;
}

FontManager &FontManager :: getSingleton( void )
{
	static FontManager instance;
	return instance;
}

/* *** Widget event names *** */

const String Editbox::EventTextChanged                = "TextChanged";
const String MultiLineEditbox::EventTextChanged       = "TextChanged";
const String Combobox::EventListSelectionAccepted     = "ListSelectionAccepted";
const String Listbox::EventSelectionChanged           = "ItemSelectionChanged";
const String PushButton::EventClicked                 = "Clicked";
const String Checkbox::EventCheckStateChanged         = "CheckStateChanged";
const String Slider::EventValueChanged                = "ValueChanged";
const String Spinner::EventValueChanged               = "ValueChanged";

} // namespace CEGUI
