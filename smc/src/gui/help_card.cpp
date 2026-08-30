/***************************************************************************
 * help_card.cpp  -  Modern help card overlay
 ***************************************************************************/
#include "../gui/help_card.h"
#include "../core/game_core.h"
#include "../core/main.h"
#include "../core/framerate.h"
#include "../video/video.h"
#include "../video/font.h"
#include "../video/gl_surface.h"
#include "../audio/audio.h"
#include "../user/preferences.h"
#include "../input/keyboard.h"
#include "../input/joystick.h"
#include "../input/touch_controls.h"
#include "../input/autoplay.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <algorithm>
#include <cmath>

namespace SMC
{

#include "../gui/ui_palette.h"

static const float HEADER_H      = 44.0f;
static const float BODY_PAD      = 14.0f;
#ifdef __ANDROID__
// A thumb needs a real target: 34x110 is a mouse button.
static const float BTN_H         = 52.0f;
static const float BTN_W         = 200.0f;
#else
static const float BTN_H         = 34.0f;
static const float BTN_W         = 110.0f;
#endif
static const float BORDER_T      = 3.0f;
static const float ANIM_IN_TIME  = 0.15f;
static const float ANIM_OUT_TIME = 0.10f;
#ifdef __ANDROID__
// Le corps etait rendu dans la petite police : lisible sur un ecran de bureau
// a 60 cm, pas sur un telephone tenu a bout de bras. La carte grandit avec le
// texte, sinon les lignes deborderaient de la zone de clip.
#define BODY_FONT ( pFont->m_font_normal )
static const float BODY_H_MAX    = 200.0f;
static const float BODY_LINE_H   = 26.0f;
#else
#define BODY_FONT ( pFont->m_font_small )
static const float BODY_H_MAX    = 120.0f;
static const float BODY_LINE_H   = 18.0f;
#endif

/* Wrap on the font the body is actually drawn in.
 *
 * The measuring and the drawing used to disagree: lines were fitted against
 * BODY_FONT while Render_Text was still handed m_font_small, so enlarging the
 * body font on Android widened the wrap without touching a single glyph. */
std::vector<std::string> cHelpCard :: Wrap_Body( float max_w ) const
{
    std::vector<std::string> out;
    std::string remaining = m_body;

    while( !remaining.empty() )
    {
        size_t nl = remaining.find( '\n' );
        std::string line = ( nl == std::string::npos ) ? remaining : remaining.substr( 0, nl );
        remaining = ( nl == std::string::npos ) ? "" : remaining.substr( nl + 1 );

        if( line.empty() )
        {
            out.push_back( "" );
            continue;
        }

        while( !line.empty() )
        {
            std::string fit = line;
            while( !fit.empty() )
            {
                int tw = 0, th = 0;
                TTF_SizeUTF8( BODY_FONT, fit.c_str(), &tw, &th );
                if( static_cast<float>(tw) <= max_w ) break;
                size_t sp = fit.rfind( ' ' );
                if( sp == std::string::npos ) break;   // keep full word, let it overflow
                fit = fit.substr( 0, sp );
            }

            out.push_back( fit );

            size_t skip = fit.size();
            if( skip < line.size() && line[skip] == ' ' ) skip++;
            line = line.substr( skip );
        }
    }

    return out;
}

/* A three-line hint used to sit in a frame built for eight. */
float cHelpCard :: Body_Height( float max_w ) const
{
    float needed = Wrap_Body( max_w ).size() * BODY_LINE_H;
    if( needed < BODY_LINE_H ) needed = BODY_LINE_H;
    return std::min( needed, BODY_H_MAX );
}

cHelpCard :: cHelpCard( const std::string &title, const std::string &body, HelpCardIcon icon )
: m_title(title), m_body(body), m_icon(icon), m_scroll_offset(0.0f), m_closing(false), m_open_tick(0)
{
}

void cHelpCard :: Run( void )
{
    m_scroll_offset = 0.0f;
    m_closing = false;

    float anim_t = 0.0f;
    bool closing = false;
    float close_t = 0.0f;

    // The card opens while the player is walking, so a thumb is already down
    // on the d-pad. Without a grace period that press closes the card in the
    // same frame it appeared and the hint is never read.
    m_open_tick = SDL_GetTicks();

    // The pad is drawn after the card and was covering a line of the hint. It
    // is dead weight here anyway: this loop polls SDL itself and never hands
    // events to the touch controls.
    bool pad_was_visible = false;
    if( pTouchControls )
    {
        pad_was_visible = pTouchControls->m_visible;
        pTouchControls->m_visible = false;
    }

    while( true )
    {
        // The scripted pilot has no way to press "Got it": the card owns the
        // event loop and never hands anything to the touch controls. Without
        // this it parks on the first hint box for the rest of the run.
        if( Autoplay_Enabled() && SDL_GetTicks() - m_open_tick > 1500 )
        {
            closing = true;
            m_closing = true;
        }

        SDL_Event e;
        while( SDL_PollEvent( &e ) )
        {
            if( e.type == SDL_QUIT )
            {
                game_exit = 1;
                closing = true;
                m_closing = true;
            }
            else if( Handle_Event( e ) )
            {
                closing = true;
                m_closing = true;
            }
        }

        const Uint8 *keys = SDL_GetKeyboardState( NULL );
        Sint16 joy_ver = 0;
        if( pPreferences->m_joy_enabled && pJoystick->m_joystick )
            joy_ver = SDL_JoystickGetAxis( pJoystick->m_joystick, pPreferences->m_joy_axis_ver );

        float scroll_step = 2.5f * pFramerate->m_speed_factor;
        if( keys[SDL_GetScancodeFromKey( pPreferences->m_key_down )] || joy_ver > pPreferences->m_joy_axis_threshold )
            m_scroll_offset += scroll_step;
        if( keys[SDL_GetScancodeFromKey( pPreferences->m_key_up )] || joy_ver < -pPreferences->m_joy_axis_threshold )
            m_scroll_offset = std::max( 0.0f, m_scroll_offset - scroll_step );

        if( !closing && anim_t < 1.0f )
            anim_t = std::min( 1.0f, anim_t + pFramerate->m_speed_factor / ( ANIM_IN_TIME * pFramerate->m_fps_target ) );

        if( closing )
        {
            close_t += pFramerate->m_speed_factor / ( ANIM_OUT_TIME * pFramerate->m_fps_target );
            anim_t = std::max( 0.0f, 1.0f - close_t );
            if( anim_t <= 0.0f )
                break;
        }

        Draw_Game();
        std::vector<cGL_Surface*> pending_delete;
        Render( anim_t, pending_delete );
        pAudio->Update();
        pVideo->Render();
        // Delete font surfaces AFTER Render() — their GL textures must stay alive
        // until pVideo->Render() flushes the deferred render queue.
        for( unsigned int i = 0; i < pending_delete.size(); ++i )
            delete pending_delete[i];
        pFramerate->Update();
    }

    if( pTouchControls )
        pTouchControls->m_visible = pad_was_visible;
}


/* Ignore everything for a moment after the card appears. */
bool cHelpCard :: Input_Allowed( void ) const
{
    return SDL_GetTicks() - m_open_tick > 400;
}

/* Where a press closes the card.
 *
 * On a phone only the button does: the player is holding the d-pad when the
 * card opens, and a press anywhere else is far more likely to be that thumb
 * than an intent to dismiss. With a mouse, clicking away is the expected
 * gesture and stays. */
bool cHelpCard :: Hit_Dismiss( float mx, float my, float cx, float cy,
                               float card_w, float card_h, float body_h ) const
{
    const float btn_x = cx + ( card_w - BTN_W ) * 0.5f;
    const float btn_y = cy + HEADER_H + body_h + BODY_PAD * 2.0f;

    if( mx >= btn_x && mx <= btn_x + BTN_W && my >= btn_y && my <= btn_y + BTN_H )
        return true;

#ifndef __ANDROID__
    if( mx < cx || mx > cx + card_w || my < cy || my > cy + card_h )
        return true;
#else
    (void)card_h;
#endif

    return false;
}

bool cHelpCard :: Handle_Event( const SDL_Event &e )
{
    const float CARD_W = game_res_w * 0.75f;
    const float BODY_H = Body_Height( CARD_W - BODY_PAD * 2.0f );
    const float CARD_H = HEADER_H + BODY_H + BTN_H + BODY_PAD * 3.0f;
    const float cx = ( game_res_w - CARD_W ) * 0.5f;
    const float cy = ( game_res_h - CARD_H ) * 0.5f;

    if( e.type == SDL_KEYDOWN )
    {
        SDL_Keycode sym = e.key.keysym.sym;
        if( sym == SDLK_ESCAPE || sym == SDLK_RETURN || sym == SDLK_SPACE
            || sym == pPreferences->m_key_action )
            return true;
    }
    else if( e.type == SDL_JOYBUTTONDOWN )
    {
        if( e.jbutton.button == static_cast<Uint8>( pPreferences->m_joy_button_action )
            || e.jbutton.button == static_cast<Uint8>( pPreferences->m_joy_button_exit ) )
            return true;
    }
    else if( e.type == SDL_MOUSEBUTTONDOWN )
    {
        // SDL raises a synthetic mouse event for every touch as well; letting
        // both through means one finger counts twice.
        if( e.button.which == SDL_TOUCH_MOUSEID ) return false;
        if( !Input_Allowed() ) return false;

        float mx = static_cast<float>( e.button.x ) / global_upscalex;
        float my = static_cast<float>( e.button.y ) / global_upscaley;
        if( Hit_Dismiss( mx, my, cx, cy, CARD_W, CARD_H, BODY_H ) )
            return true;
    }
    else if( e.type == SDL_FINGERDOWN )
    {
        if( !Input_Allowed() ) return false;

        float mx = e.tfinger.x * game_res_w;
        float my = e.tfinger.y * game_res_h;
        if( Hit_Dismiss( mx, my, cx, cy, CARD_W, CARD_H, BODY_H ) )
            return true;
    }
    return false;
}

void cHelpCard :: Render( float anim_t, std::vector<cGL_Surface*> &pending_delete )
{
    const float CARD_W = game_res_w * 0.75f;
    const float BODY_H = Body_Height( CARD_W - BODY_PAD * 2.0f );
    const float CARD_H = HEADER_H + BODY_H + BTN_H + BODY_PAD * 3.0f;

    float t = 1.0f - (1.0f - anim_t) * (1.0f - anim_t) * (1.0f - anim_t);

    float cx = ( game_res_w - CARD_W ) * 0.5f;
    float cy = ( game_res_h - CARD_H ) * 0.5f;

    float scale;
    if( !m_closing )
        scale = 0.8f + 0.2f * t;    // entry: 0.8→1.0
    else
        scale = 0.9f + 0.1f * t;    // exit:  collapses from 1.0 to 0.9
    float off_x    = cx + CARD_W * 0.5f * ( 1.0f - scale );
    float off_y    = cy + CARD_H * 0.5f * ( 1.0f - scale );
    float sw       = CARD_W * scale;
    float sh       = CARD_H * scale;
    float header_h = HEADER_H * scale;
    float body_h   = BODY_H * scale;
    float btn_h    = BTN_H * scale;
    float pad      = BODY_PAD * scale;

    Uint8 alpha = static_cast<Uint8>( 255 * t );

    Color overlay = COL_OVERLAY;
    overlay.alpha = static_cast<Uint8>( COL_OVERLAY.alpha * t );
    pVideo->Draw_Rect( 0, 0, static_cast<float>(game_res_w), static_cast<float>(game_res_h), 0.899f, &overlay );

    Color border = COL_BORDER;
    border.alpha = alpha;
    pVideo->Draw_Rect( off_x - BORDER_T, off_y - BORDER_T, sw + BORDER_T*2, sh + BORDER_T*2, 0.9f, &border );

    Color card_bg = COL_CARD_BG;
    card_bg.alpha = alpha;
    pVideo->Draw_Rect( off_x, off_y, sw, sh, 0.901f, &card_bg );

    Color header_bg = COL_HEADER_BG;
    header_bg.alpha = alpha;
    pVideo->Draw_Rect( off_x, off_y, sw, header_h, 0.902f, &header_bg );

    if( t > 0.3f )
    {
        Color title_col = COL_TITLE;
        title_col.alpha = alpha;
        cGL_Surface *title_surf = pFont->Render_Text( pFont->m_font_normal, m_title, title_col );
        if( title_surf )
        {
            title_surf->Blit( off_x + pad, off_y + ( header_h - title_surf->m_h * scale ) * 0.5f, 0.903f );
            pending_delete.push_back( title_surf );
        }
    }

    if( t > 0.4f )
    {
        float body_y = off_y + header_h + pad;
        Render_Text_Wrapped( m_body, off_x + pad, body_y, CARD_W - BODY_PAD * 2.0f, BODY_LINE_H * scale, body_h, pending_delete );
    }

    float btn_x = off_x + ( sw - BTN_W * scale ) * 0.5f;
    float btn_y = off_y + header_h + body_h + pad * 2.0f;

    Color btn_border = COL_BTN_BORDER;
    btn_border.alpha = alpha;
    pVideo->Draw_Rect( btn_x - 2, btn_y - 2, BTN_W * scale + 4, btn_h + 4, 0.902f, &btn_border );

    Color btn_bg = COL_BTN_BG;
    btn_bg.alpha = alpha;
    pVideo->Draw_Rect( btn_x, btn_y, BTN_W * scale, btn_h, 0.903f, &btn_bg );

    if( t > 0.5f )
    {
        Color btn_text_col = COL_BTN_TEXT;
        btn_text_col.alpha = alpha;
        cGL_Surface *btn_surf = pFont->Render_Text( BODY_FONT, "Got it", btn_text_col );
        if( btn_surf )
        {
            btn_surf->Blit( btn_x + ( BTN_W * scale - btn_surf->m_w ) * 0.5f,
                            btn_y + ( btn_h - btn_surf->m_h ) * 0.5f, 0.904f );
            pending_delete.push_back( btn_surf );
        }
    }
}

void cHelpCard :: Render_Text_Wrapped( const std::string &text, float x, float y, float max_w, float line_h, float clip_height, std::vector<cGL_Surface*> &pending_delete )
{
    (void)text;   // always m_body; the wrap is shared with the height measure

    float cur_y = y - m_scroll_offset;
    float clip_top = y;
    float clip_bot = y + clip_height;

    std::vector<std::string> lines = Wrap_Body( max_w );
    for( unsigned int i = 0; i < lines.size(); ++i )
    {
        if( !lines[i].empty() && cur_y + line_h > clip_top && cur_y < clip_bot )
        {
            Color col = COL_BODY;
            cGL_Surface *surf = pFont->Render_Text( BODY_FONT, lines[i], col );
            if( surf )
            {
                surf->Blit( x, cur_y, 0.903f );
                pending_delete.push_back( surf );
            }
        }
        cur_y += line_h;
    }
}

} // namespace SMC
