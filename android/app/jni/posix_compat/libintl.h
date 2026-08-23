/***************************************************************************
 * libintl.h  -  gettext no-op shim for Android
 *
 * Bionic has no gettext, and shipping libintl for a game whose UI text is
 * already English would buy nothing. Every entry point returns the source
 * string unchanged, so _( "Options" ) is simply "Options".
 *
 * Consequence, on purpose: the Android build is English-only. data/translations
 * is still packaged but never read.
 ***************************************************************************/
#ifndef SMC_LIBINTL_SHIM_H
#define SMC_LIBINTL_SHIM_H

static inline char *gettext( const char *msgid )
{
	// gettext() returns a non-const pointer into its own catalog; with no
	// catalog the caller gets the literal back. Callers never write to it.
	return const_cast<char *>( msgid );
}

static inline char *dgettext( const char *, const char *msgid )
{
	return const_cast<char *>( msgid );
}

static inline char *ngettext( const char *msgid, const char *msgid_plural, unsigned long n )
{
	return const_cast<char *>( n == 1 ? msgid : msgid_plural );
}

// The three setup calls in i18n.cpp check for NULL and only print a warning,
// so returning the domain/dirname keeps that path quiet.
static inline char *bindtextdomain( const char *, const char *dirname )
{
	return const_cast<char *>( dirname );
}

static inline char *bind_textdomain_codeset( const char *, const char *codeset )
{
	return const_cast<char *>( codeset );
}

static inline char *textdomain( const char *domainname )
{
	return const_cast<char *>( domainname );
}

#endif // SMC_LIBINTL_SHIM_H
