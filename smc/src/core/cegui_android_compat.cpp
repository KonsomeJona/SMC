/***************************************************************************
 * cegui_android_compat.cpp  -  CEGUI XML layer driven by TinyXML2
 *
 * The Android build has no CEGUI, but every level, world, savegame and
 * campaign is read and written through CEGUI's XML interfaces. The handler
 * and serializer contracts are kept exactly as the game expects them; only
 * the engine underneath changes.
 ***************************************************************************/
#ifdef SMC_NO_CEGUI

#include "cegui_android_compat.h"
#include "tinyxml2.h"

#include <ostream>
#include <stdexcept>

namespace CEGUI
{

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
		// The callers catch CEGUI::Exception / std::exception around every
		// parseXMLFile call and fall back to reporting a broken file.
		throw Exception( std::string( "Could not parse " ) + filename + " : " +
			( doc.ErrorStr() ? doc.ErrorStr() : "unknown error" ) );
	}

	Walk_Element( doc.RootElement(), handler );
}

System &System :: getSingleton( void )
{
	static System instance;
	return instance;
}

/* *** XMLSerializer *** */

XMLSerializer :: XMLSerializer( std::ostream &out, size_t indent )
: m_out( out ), m_indent( indent ), m_tag_open( false )
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
		m_tag_open = false;
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
	m_tag_open = true;

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
		switch( value[i] )
		{
			case '&':  m_out << "&amp;";  break;
			case '<':  m_out << "&lt;";   break;
			case '>':  m_out << "&gt;";   break;
			case '"':  m_out << "&quot;"; break;
			case '\'': m_out << "&apos;"; break;
			default:   m_out << value[i]; break;
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
		m_tag_open = false;
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

} // namespace CEGUI

#endif // SMC_NO_CEGUI
