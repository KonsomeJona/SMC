/***************************************************************************
 * help_card.cpp  -  Modern help card overlay
 ***************************************************************************/
#include "../gui/help_card.h"
#include "../core/game_core.h"
#include "../core/main.h"
#include "../core/framerate.h"
#include "../video/video.h"
#include "../video/font.h"
#include "../audio/audio.h"
#include "../user/preferences.h"
#include "../input/keyboard.h"
#include "../input/joystick.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <algorithm>
#include <cmath>

namespace SMC
{

// Card palette (RGBA)
static const Color COL_OVERLAY    = Color( 0,   0,   0,   160 );
static const Color COL_CARD_BG    = Color( 255, 248, 220, 245 );
static const Color COL_HEADER_BG  = Color( 218, 165,  32, 255 );
static const Color COL_BORDER     = Color( 139,  90,  43, 200 );
static const Color COL_BTN_BG     = Color( 218, 165,  32, 255 );
static const Color COL_BTN_BORDER = Color( 139,  90,  43, 255 );
static const Color COL_TITLE      = Color( 255, 255, 255, 255 );
static const Color COL_BODY       = Color(  60,  30,   0, 255 );
static const Color COL_BTN_TEXT   = Color(  60,  30,   0, 255 );

static const float HEADER_H      = 44.0f;
static const float BODY_PAD      = 14.0f;
static const float BTN_H         = 34.0f;
static const float BTN_W         = 110.0f;
static const float BORDER_T      = 3.0f;
static const float ANIM_IN_TIME  = 0.15f;
static const float ANIM_OUT_TIME = 0.10f;
static const float BODY_H        = 120.0f;

cHelpCard :: cHelpCard( const std::string &title, const std::string &body, HelpCardIcon icon )
: m_title(title), m_body(body), m_icon(icon), m_scroll_offset(0.0f), m_closing(false)
{
}

void cHelpCard :: Run( void )
{
    m_scroll_offset = 0.0f;
    m_closing = false;

    float anim_t = 0.0f;
    bool closing = false;
    float close_t = 0.0f;

    while( true )
    {
        SDL_Event e;
        while( SDL_PollEvent( &e ) )
        {
            if( Handle_Event( e ) )
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
            anim_t = std::min( 1.0f, anim_t + pFramerate->m_speed_factor / ( ANIM_IN_TIME * pFramerate->m_fps_max ) );

        if( closing )
        {
            close_t += pFramerate->m_speed_factor / ( ANIM_OUT_TIME * pFramerate->m_fps_max );
            anim_t = std::max( 0.0f, 1.0f - close_t );
            if( anim_t <= 0.0f )
                break;
        }

        Draw_Game();
        Render( anim_t );
        pAudio->Update();
        pVideo->Render();
        pFramerate->Update();
    }
}

bool cHelpCard :: Handle_Event( const SDL_Event &e )
{
    const float CARD_W = game_res_w * 0.75f;
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
        float mx = static_cast<float>( e.button.x ) / global_upscalex;
        float my = static_cast<float>( e.button.y ) / global_upscaley;
        if( mx < cx || mx > cx + CARD_W || my < cy || my > cy + CARD_H )
            return true;
        float btn_x = cx + ( CARD_W - BTN_W ) * 0.5f;
        float btn_y = cy + HEADER_H + BODY_H + BODY_PAD * 2.0f;
        if( mx >= btn_x && mx <= btn_x + BTN_W && my >= btn_y && my <= btn_y + BTN_H )
            return true;
    }
    else if( e.type == SDL_FINGERDOWN )
    {
        float mx = e.tfinger.x * game_res_w;
        float my = e.tfinger.y * game_res_h;
        // dismiss if outside card
        if( mx < cx || mx > cx + CARD_W || my < cy || my > cy + CARD_H )
            return true;
        // dismiss if on "Got it" button
        float btn_x = cx + ( CARD_W - BTN_W ) * 0.5f;
        float btn_y = cy + HEADER_H + BODY_H + BODY_PAD * 2.0f;
        if( mx >= btn_x && mx <= btn_x + BTN_W && my >= btn_y && my <= btn_y + BTN_H )
            return true;
    }
    return false;
}

void cHelpCard :: Render( float anim_t )
{
    const float CARD_W = game_res_w * 0.75f;
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
            delete title_surf;
        }
    }

    if( t > 0.4f )
    {
        float body_y = off_y + header_h + pad;
        Render_Text_Wrapped( m_body, off_x + pad, body_y, sw - pad * 2.0f, 18.0f * scale, body_h );
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
        cGL_Surface *btn_surf = pFont->Render_Text( pFont->m_font_small, "Got it", btn_text_col );
        if( btn_surf )
        {
            btn_surf->Blit( btn_x + ( BTN_W * scale - btn_surf->m_w ) * 0.5f,
                            btn_y + ( btn_h - btn_surf->m_h ) * 0.5f, 0.904f );
            delete btn_surf;
        }
    }
}

void cHelpCard :: Render_Text_Wrapped( const std::string &text, float x, float y, float max_w, float line_h, float clip_height )
{
    float cur_y = y - m_scroll_offset;
    float clip_top = y;
    float clip_bot = y + clip_height;

    std::string remaining = text;
    while( !remaining.empty() )
    {
        size_t nl = remaining.find( '\n' );
        std::string line = ( nl == std::string::npos ) ? remaining : remaining.substr( 0, nl );
        remaining = ( nl == std::string::npos ) ? "" : remaining.substr( nl + 1 );

        while( !line.empty() )
        {
            std::string fit = line;
            while( !fit.empty() )
            {
                int tw = 0, th = 0;
                TTF_SizeUTF8( pFont->m_font_small, fit.c_str(), &tw, &th );
                if( static_cast<float>(tw) <= max_w ) break;
                size_t sp = fit.rfind( ' ' );
                if( sp == std::string::npos ) { break; }  // keep full word, let it overflow
                fit = fit.substr( 0, sp );
            }

            if( cur_y + line_h > clip_top && cur_y < clip_bot )
            {
                Color col = COL_BODY;
                cGL_Surface *surf = pFont->Render_Text( pFont->m_font_small, fit, col );
                if( surf )
                {
                    surf->Blit( x, cur_y, 0.903f );
                    delete surf;
                }
            }

            cur_y += line_h;
            size_t skip = fit.size();
            if( skip < line.size() && line[skip] == ' ' ) skip++;
            line = line.substr( skip );
        }
    }
}

} // namespace SMC
