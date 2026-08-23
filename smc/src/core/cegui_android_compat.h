#ifndef SMC_CEGUI_ANDROID_COMPAT_H
#define SMC_CEGUI_ANDROID_COMPAT_H

#ifdef SMC_NO_CEGUI

#include <string>
#include <unordered_map>
#include <cstdio>
#include <vector>
#include <iosfwd>
#include <stdexcept>

namespace CEGUI {

using String = std::string;

// Ordered name/value list. Order matters: savegame.cpp walks the attributes
// by index (getCount / getName / getValue), which an unordered_map cannot do.
class XMLAttributes {
    std::vector< std::pair<std::string, std::string> > m_attrs;

    int Find( const std::string &key ) const {
        for (size_t i = 0; i < m_attrs.size(); ++i)
            if (m_attrs[i].first == key) return (int)i;
        return -1;
    }

public:
    void add( const std::string &key, const std::string &value ) {
        int pos = Find(key);
        if (pos >= 0) m_attrs[pos].second = value;   // CEGUI's add() overwrites
        else m_attrs.push_back(std::make_pair(key, value));
    }
    void remove( const std::string &key ) {
        int pos = Find(key);
        if (pos >= 0) m_attrs.erase(m_attrs.begin() + pos);
    }
    bool exists( const std::string &key ) const { return Find(key) >= 0; }
    void clear() { m_attrs.clear(); }

    size_t getCount() const { return m_attrs.size(); }
    const std::string &getName( size_t index ) const {
        static const std::string empty;
        return index < m_attrs.size() ? m_attrs[index].first : empty;
    }
    const std::string &getValue( size_t index ) const {
        static const std::string empty;
        return index < m_attrs.size() ? m_attrs[index].second : empty;
    }

    std::string getValueAsString( const std::string &key, const std::string &def = "" ) const {
        int pos = Find(key);
        return pos >= 0 ? m_attrs[pos].second : def;
    }
    std::string getValue( const std::string &key ) const { return getValueAsString(key); }

    int getValueAsInteger( const std::string &key, int def = 0 ) const {
        int pos = Find(key);
        if (pos < 0 || m_attrs[pos].second.empty()) return def;
        try { return std::stoi(m_attrs[pos].second); } catch(...) { return def; }
    }
    float getValueAsFloat( const std::string &key, float def = 0.0f ) const {
        int pos = Find(key);
        if (pos < 0 || m_attrs[pos].second.empty()) return def;
        try { return std::stof(m_attrs[pos].second); } catch(...) { return def; }
    }
    bool getValueAsBool( const std::string &key, bool def = false ) const {
        int pos = Find(key);
        if (pos < 0 || m_attrs[pos].second.empty()) return def;
        const std::string &v = m_attrs[pos].second;
        if (v == "true" || v == "True" || v == "1" || v == "yes") return true;
        if (v == "false" || v == "False" || v == "0" || v == "no") return false;
        return def;
    }
};


typedef unsigned char utf8;

// Loaders catch CEGUI::Exception around parsing and print getMessage().
class Exception : public std::runtime_error
{
public:
    Exception( const String &msg = "" ) : std::runtime_error( msg ), m_message( msg ) {}
    const String &getMessage() const { return m_message; }
private:
    String m_message;
};


// --- XML reading -----------------------------------------------------------
//
// Levels, worlds, savegames and campaigns are all parsed through CEGUI's XML
// handler interface. The Android build keeps that interface and drives it from
// TinyXML2 (already vendored in core/), so the parsing code in the game is
// untouched. Implementation lives in cegui_android_compat.cpp.

class XMLHandler
{
public:
    virtual ~XMLHandler() {}
    virtual void elementStart( const String &element, const XMLAttributes &attributes ) {}
    virtual void elementEnd( const String &element ) {}
};

class XMLParser
{
public:
    // schemaName / resourceGroup are accepted for source compatibility and
    // ignored: there is no validating parser here.
    void parseXMLFile( XMLHandler &handler, const String &filename,
                       const String &schemaName = "", const String &resourceGroup = "" );
};

class System
{
public:
    static System &getSingleton();
    XMLParser *getXMLParser() { return &m_parser; }
private:
    XMLParser m_parser;
};

// --- XML writing -----------------------------------------------------------

// Fluent writer: stream.openTag( "x" ).attribute( "a", "b" ).closeTag()
class XMLSerializer
{
public:
    explicit XMLSerializer( std::ostream &out, size_t indent = 1 );
    ~XMLSerializer();

    XMLSerializer &openTag( const String &name );
    XMLSerializer &closeTag();
    XMLSerializer &attribute( const String &name, const String &value );
    XMLSerializer &text( const String &value );

private:
    std::ostream &m_out;
    size_t m_indent;
    std::vector< String > m_stack;
    bool m_tag_open;

    void Close_Opening_Tag();
};

template<typename T> struct PropertyHelper {};
template<> struct PropertyHelper<float> {
    static std::string toString(float v) {
        char buf[64]; snprintf(buf, sizeof(buf), "%g", v); return buf;
    }
};
template<> struct PropertyHelper<int> {
    static std::string toString(int v) { return std::to_string(v); }
};
template<> struct PropertyHelper<unsigned int> {
    static std::string toString(unsigned int v) { return std::to_string(v); }
};

} // namespace CEGUI

#endif // SMC_NO_CEGUI
#endif // SMC_CEGUI_ANDROID_COMPAT_H
