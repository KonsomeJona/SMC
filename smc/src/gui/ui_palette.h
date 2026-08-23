/***************************************************************************
 * ui_palette.h  -  Shared color palette for ModernUI and cHelpCard
 *
 * Include this file in the .cpp of any custom OpenGL UI that needs to
 * match the golden-header / cream style.  Each TU gets its own copy of
 * the static constants, which is fine for a handful of Color objects.
 ***************************************************************************/
#ifndef SMC_UI_PALETTE_H
#define SMC_UI_PALETTE_H

#include "../video/color.h"

// cast first arg to Uint8 to disambiguate the Color constructor overloads
#define UI_RGBA(r,g,b,a) Color( static_cast<Uint8>(r), static_cast<Uint8>(g), static_cast<Uint8>(b), static_cast<Uint8>(a) )

static const Color COL_OVERLAY    = UI_RGBA(   0,   0,   0, 160 );
static const Color COL_CARD_BG    = UI_RGBA( 255, 248, 220, 245 );
static const Color COL_HEADER_BG  = UI_RGBA( 218, 165,  32, 255 );
static const Color COL_BORDER     = UI_RGBA( 139,  90,  43, 200 );
static const Color COL_BTN_BG     = UI_RGBA( 218, 165,  32, 255 );
static const Color COL_BTN_BORDER = UI_RGBA( 139,  90,  43, 255 );
static const Color COL_TITLE      = UI_RGBA( 255, 255, 255, 255 );
static const Color COL_BODY       = UI_RGBA(  60,  30,   0, 255 );
static const Color COL_BTN_TEXT   = UI_RGBA(  60,  30,   0, 255 );
static const Color COL_SELECTED   = UI_RGBA( 218, 165,  32, 255 );
static const Color COL_HOVER      = UI_RGBA( 255, 248, 220, 200 );

#undef UI_RGBA

#endif
