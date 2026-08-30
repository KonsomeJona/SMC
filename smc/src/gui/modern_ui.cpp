/***************************************************************************
 * modern_ui.cpp  -  Lightweight stateless widget toolkit
 ***************************************************************************/
#include "../gui/modern_ui.h"
#include "../core/game_core.h"
#include "../core/main.h"
#include "../video/video.h"
#include "../video/font.h"
#include "../video/gl_surface.h"
#include "../user/preferences.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <algorithm>
#include <cmath>

namespace SMC { /* pulling in SMC globals */ }
using namespace SMC;

#include "../gui/ui_palette.h"

// ---------------------------------------------------------------------------
// Timing constants
// ---------------------------------------------------------------------------
static const Uint32 DCLICK_MS = 300;  // double-click window in milliseconds

// ---------------------------------------------------------------------------
// Z-depth constants
// ---------------------------------------------------------------------------
static const float Z_BG   = 0.900f;
static const float Z_CONT = 0.901f;
static const float Z_TEXT = 0.902f;

// ---------------------------------------------------------------------------
// Layout constants
// ---------------------------------------------------------------------------
static const float ROW_H     = 30.0f;
static const float BORDER_T  = 2.0f;
static const float ITEM_H    = 24.0f;
static const float ARROW_W   = 20.0f;

// ---------------------------------------------------------------------------
// Internal mouse / scroll state
// ---------------------------------------------------------------------------
namespace
{
    float  s_mouse_x       = 0.0f;
    float  s_mouse_y       = 0.0f;
    bool   s_mouse_clicked = false;   // true for one frame when LMB released
    bool   s_mouse_held    = false;   // true while LMB is down

    // double-click detection
    int    s_last_clicked_item = -1;
    Uint32 s_last_click_time   = 0;

    // slider drag state
    bool  s_slider_dragging = false;
    float s_slider_drag_min = 0.0f;
    float s_slider_drag_max = 0.0f;
    float s_slider_drag_x   = 0.0f;
    float s_slider_drag_w   = 0.0f;

    // mouse-wheel scroll accumulated per frame
    int   s_scroll_delta = 0;
} // anonymous namespace

// ---------------------------------------------------------------------------
// Hit test helper
// ---------------------------------------------------------------------------
static bool Hit( float x, float y, float w, float h )
{
    return s_mouse_x >= x && s_mouse_x <= x + w
        && s_mouse_y >= y && s_mouse_y <= y + h;
}

// ---------------------------------------------------------------------------
// Text rendering helper: render, blit, push to pending_delete
// ---------------------------------------------------------------------------

// The small font was sized for a desktop screen at arm's length. A phone
// holds the same pixels much further from the eye, relatively speaking.
#ifdef __ANDROID__
#define UI_FONT ( pFont->m_font_normal )
#else
#define UI_FONT ( pFont->m_font_small )
#endif

static void Draw_Text( const std::string &text, float x, float y, float z,
                        const Color &col, std::vector<cGL_Surface*> &pd )
{
    if( text.empty() ) return;
    cGL_Surface *surf = pFont->Render_Text( UI_FONT, text, col );
    if( surf )
    {
        surf->Blit( x, y, z );
        pd.push_back( surf );
    }
}

// Render text centred inside a box (x,y,w,h)
static void Draw_Text_Centered( const std::string &text,
                                 float bx, float by, float bw, float bh,
                                 float z, const Color &col,
                                 std::vector<cGL_Surface*> &pd )
{
    if( text.empty() ) return;
    cGL_Surface *surf = pFont->Render_Text( UI_FONT, text, col );
    if( surf )
    {
        float tx = bx + ( bw - static_cast<float>( surf->m_w ) ) * 0.5f;
        float ty = by + ( bh - static_cast<float>( surf->m_h ) ) * 0.5f;
        surf->Blit( tx, ty, z );
        pd.push_back( surf );
    }
}

// ---------------------------------------------------------------------------
namespace ModernUI
{

// ---------------------------------------------------------------------------
void Begin_Frame( void )
{
    // Call once per frame before the SDL event poll loop to reset one-frame flags.
    // Do NOT reset inside Feed_Event — a MOUSEMOTION following MOUSEBUTTONUP
    // in the same frame would otherwise clear the click before widgets see it.
    s_mouse_clicked = false;
    s_scroll_delta  = 0;
}

void Feed_Event( const SDL_Event &e )
{
    switch( e.type )
    {
    case SDL_MOUSEMOTION:
        s_mouse_x = ( static_cast<float>( e.motion.x ) - global_view_x ) / global_upscalex;
        s_mouse_y = ( static_cast<float>( e.motion.y ) - global_view_y ) / global_upscaley;
        break;

    case SDL_MOUSEBUTTONDOWN:
        if( e.button.button == SDL_BUTTON_LEFT )
        {
            s_mouse_x    = ( static_cast<float>( e.button.x ) - global_view_x ) / global_upscalex;
            s_mouse_y    = ( static_cast<float>( e.button.y ) - global_view_y ) / global_upscaley;
            s_mouse_held = true;
        }
        break;

    case SDL_MOUSEBUTTONUP:
        if( e.button.button == SDL_BUTTON_LEFT )
        {
            s_mouse_x       = ( static_cast<float>( e.button.x ) - global_view_x ) / global_upscalex;
            s_mouse_y       = ( static_cast<float>( e.button.y ) - global_view_y ) / global_upscaley;
            s_mouse_held    = false;
            s_mouse_clicked = true;
            // End slider drag on release
            s_slider_dragging = false;
        }
        break;

    case SDL_MOUSEWHEEL:
        // negative y = scroll down (increase offset), positive y = scroll up
        s_scroll_delta = -e.wheel.y;
        break;

    case SDL_FINGERDOWN:
        s_mouse_x    = ( e.tfinger.x * static_cast<float>( pPreferences->m_video_screen_w ) - global_view_x ) / global_upscalex;
        s_mouse_y    = ( e.tfinger.y * static_cast<float>( pPreferences->m_video_screen_h ) - global_view_y ) / global_upscaley;
        s_mouse_held = true;
        break;

    case SDL_FINGERUP:
        s_mouse_x       = ( e.tfinger.x * static_cast<float>( pPreferences->m_video_screen_w ) - global_view_x ) / global_upscalex;
        s_mouse_y       = ( e.tfinger.y * static_cast<float>( pPreferences->m_video_screen_h ) - global_view_y ) / global_upscaley;
        s_mouse_held    = false;
        s_mouse_clicked = true;
        s_slider_dragging = false;
        break;

    case SDL_FINGERMOTION:
        s_mouse_x = ( e.tfinger.x * static_cast<float>( pPreferences->m_video_screen_w ) - global_view_x ) / global_upscalex;
        s_mouse_y = ( e.tfinger.y * static_cast<float>( pPreferences->m_video_screen_h ) - global_view_y ) / global_upscaley;
        break;

    default:
        break;
    }
}

// ---------------------------------------------------------------------------
int Tab_Bar( float x, float y, float w, float h,
             const std::vector<std::string> &labels, int active_tab,
             std::vector<cGL_Surface*> &pd )
{
    if( labels.empty() ) return active_tab;

    int   new_tab  = active_tab;
    float tab_w    = w / static_cast<float>( labels.size() );

    for( int i = 0; i < static_cast<int>( labels.size() ); ++i )
    {
        float tx = x + static_cast<float>( i ) * tab_w;

        bool is_active = ( i == active_tab );

        // Border
        pVideo->Draw_Rect( tx, y, tab_w, h, Z_BG, &COL_BORDER );

        // Background fill (inset by BORDER_T)
        Color bg = is_active ? COL_HEADER_BG : COL_CARD_BG;
        pVideo->Draw_Rect( tx + BORDER_T, y + BORDER_T,
                           tab_w - BORDER_T * 2.0f, h - BORDER_T * 2.0f,
                           Z_CONT, &bg );

        // Label
        Color text_col = is_active ? COL_TITLE : COL_BODY;
        Draw_Text_Centered( labels[static_cast<size_t>( i )],
                            tx + BORDER_T, y + BORDER_T,
                            tab_w - BORDER_T * 2.0f, h - BORDER_T * 2.0f,
                            Z_TEXT, text_col, pd );

        // Hit test
        if( s_mouse_clicked && Hit( tx, y, tab_w, h ) )
            new_tab = i;
    }

    return new_tab;
}

// ---------------------------------------------------------------------------
int Scroll_List( float x, float y, float w, float h,
                 const std::vector<std::string> &items, int selected,
                 int &scroll_offset, std::vector<cGL_Surface*> &pd )
{
    int result = -1;

    // Apply scroll wheel delta
    if( s_scroll_delta != 0 && Hit( x, y, w, h ) )
    {
        int max_offset = std::max( 0, static_cast<int>( items.size() ) - static_cast<int>( h / ITEM_H ) );
        scroll_offset  = std::max( 0, std::min( scroll_offset + s_scroll_delta, max_offset ) );
    }

    // Background
    pVideo->Draw_Rect( x, y, w, h, Z_BG, &COL_CARD_BG );
    // Outer border
    pVideo->Draw_Rect( x - BORDER_T, y - BORDER_T,
                       w + BORDER_T * 2.0f, h + BORDER_T * 2.0f,
                       Z_BG - 0.001f, &COL_BORDER );

    int visible_count = static_cast<int>( h / ITEM_H ) + 1;
    int item_count    = static_cast<int>( items.size() );

    for( int i = scroll_offset; i < item_count && i < scroll_offset + visible_count; ++i )
    {
        float iy = y + static_cast<float>( i - scroll_offset ) * ITEM_H;

        // Clip items that overflow the list box
        if( iy + ITEM_H < y ) continue;
        if( iy > y + h ) break;

        bool is_selected = ( i == selected );
        bool is_hover    = Hit( x, iy, w, ITEM_H );

        // Row background
        if( is_selected )
            pVideo->Draw_Rect( x, iy, w, ITEM_H, Z_CONT, &COL_SELECTED );
        else if( is_hover )
            pVideo->Draw_Rect( x, iy, w, ITEM_H, Z_CONT, &COL_HOVER );

        // Item text
        Color text_col = is_selected ? COL_TITLE : COL_BODY;
        float text_pad = 4.0f;
        Draw_Text( items[static_cast<size_t>( i )],
                   x + text_pad, iy + ( ITEM_H - 14.0f ) * 0.5f,
                   Z_TEXT, text_col, pd );

        // Click detection
        if( s_mouse_clicked && Hit( x, iy, w, ITEM_H ) )
        {
            Uint32 now = SDL_GetTicks();
            bool double_click = ( s_last_clicked_item == i )
                             && ( now - s_last_click_time < DCLICK_MS );

            s_last_clicked_item = i;
            s_last_click_time   = now;

            result = double_click ? ( i | SCROLL_LIST_DCLICK_FLAG ) : i;
        }
    }

    return result;
}

// ---------------------------------------------------------------------------
bool Toggle_Row( float x, float y, float w,
                 const std::string &label, bool value,
                 std::vector<cGL_Surface*> &pd )
{
    bool new_value = value;

    float half_w = w * 0.5f;

    // Row background + border
    pVideo->Draw_Rect( x - BORDER_T, y - BORDER_T,
                       w + BORDER_T * 2.0f, ROW_H + BORDER_T * 2.0f,
                       Z_BG - 0.001f, &COL_BORDER );
    pVideo->Draw_Rect( x, y, w, ROW_H, Z_BG, &COL_CARD_BG );

    // Label (left half)
    float lbl_pad = 4.0f;
    Draw_Text( label, x + lbl_pad, y + ( ROW_H - 14.0f ) * 0.5f, Z_TEXT, COL_BODY, pd );

    // ON/OFF button (right half, inset slightly)
    float btn_x = x + half_w + 4.0f;
    float btn_y = y + 2.0f;
    float btn_w = half_w - 8.0f;
    float btn_h = ROW_H - 4.0f;

    // Button border
    pVideo->Draw_Rect( btn_x - BORDER_T, btn_y - BORDER_T,
                       btn_w + BORDER_T * 2.0f, btn_h + BORDER_T * 2.0f,
                       Z_CONT - 0.001f, &COL_BTN_BORDER );

    // Button fill: golden if ON, cream if OFF
    Color btn_bg = value ? COL_BTN_BG : COL_CARD_BG;
    pVideo->Draw_Rect( btn_x, btn_y, btn_w, btn_h, Z_CONT, &btn_bg );

    // Button label
    Color btn_text = value ? COL_TITLE : COL_BODY;
    Draw_Text_Centered( value ? "ON" : "OFF", btn_x, btn_y, btn_w, btn_h, Z_TEXT, btn_text, pd );

    // Hit test
    if( s_mouse_clicked && Hit( btn_x, btn_y, btn_w, btn_h ) )
        new_value = !value;

    return new_value;
}

// ---------------------------------------------------------------------------
int Select_Row( float x, float y, float w,
                const std::string &label,
                const std::vector<std::string> &options, int current,
                std::vector<cGL_Surface*> &pd )
{
    int new_index = current;
    if( options.empty() ) return new_index;

    float half_w = w * 0.5f;

    // Row background + border
    pVideo->Draw_Rect( x - BORDER_T, y - BORDER_T,
                       w + BORDER_T * 2.0f, ROW_H + BORDER_T * 2.0f,
                       Z_BG - 0.001f, &COL_BORDER );
    pVideo->Draw_Rect( x, y, w, ROW_H, Z_BG, &COL_CARD_BG );

    // Label (left half)
    float lbl_pad = 4.0f;
    Draw_Text( label, x + lbl_pad, y + ( ROW_H - 14.0f ) * 0.5f, Z_TEXT, COL_BODY, pd );

    // Control area: right half
    float ctrl_x = x + half_w;
    float ctrl_w = half_w;

    // Left arrow "<"
    float arrow_h = ROW_H - 4.0f;
    float larrow_x = ctrl_x + 2.0f;
    float larrow_y = y + 2.0f;

    pVideo->Draw_Rect( larrow_x - BORDER_T, larrow_y - BORDER_T,
                       ARROW_W + BORDER_T * 2.0f, arrow_h + BORDER_T * 2.0f,
                       Z_CONT - 0.001f, &COL_BTN_BORDER );
    pVideo->Draw_Rect( larrow_x, larrow_y, ARROW_W, arrow_h, Z_CONT, &COL_BTN_BG );
    Draw_Text_Centered( "<", larrow_x, larrow_y, ARROW_W, arrow_h, Z_TEXT, COL_TITLE, pd );

    // Right arrow ">"
    float rarrow_x = ctrl_x + ctrl_w - ARROW_W - 2.0f;
    pVideo->Draw_Rect( rarrow_x - BORDER_T, larrow_y - BORDER_T,
                       ARROW_W + BORDER_T * 2.0f, arrow_h + BORDER_T * 2.0f,
                       Z_CONT - 0.001f, &COL_BTN_BORDER );
    pVideo->Draw_Rect( rarrow_x, larrow_y, ARROW_W, arrow_h, Z_CONT, &COL_BTN_BG );
    Draw_Text_Centered( ">", rarrow_x, larrow_y, ARROW_W, arrow_h, Z_TEXT, COL_TITLE, pd );

    // Current value label (centre of control area)
    float val_x = larrow_x + ARROW_W + 2.0f;
    float val_w = rarrow_x - val_x - 2.0f;
    Draw_Text_Centered( options[static_cast<size_t>( current )],
                        val_x, y, val_w, ROW_H, Z_TEXT, COL_BODY, pd );

    // Hit tests for arrows
    if( s_mouse_clicked )
    {
        if( Hit( larrow_x, larrow_y, ARROW_W, arrow_h ) )
        {
            new_index = current - 1;
            if( new_index < 0 )
                new_index = static_cast<int>( options.size() ) - 1;
        }
        else if( Hit( rarrow_x, larrow_y, ARROW_W, arrow_h ) )
        {
            new_index = ( current + 1 ) % static_cast<int>( options.size() );
        }
    }

    return new_index;
}

// ---------------------------------------------------------------------------
float Slider_Row( float x, float y, float w,
                  const std::string &label, float value, float min_val, float max_val,
                  std::vector<cGL_Surface*> &pd )
{
    float new_value = value;

    float half_w = w * 0.5f;

    // Row background + border
    pVideo->Draw_Rect( x - BORDER_T, y - BORDER_T,
                       w + BORDER_T * 2.0f, ROW_H + BORDER_T * 2.0f,
                       Z_BG - 0.001f, &COL_BORDER );
    pVideo->Draw_Rect( x, y, w, ROW_H, Z_BG, &COL_CARD_BG );

    // Label (left half)
    float lbl_pad = 4.0f;
    Draw_Text( label, x + lbl_pad, y + ( ROW_H - 14.0f ) * 0.5f, Z_TEXT, COL_BODY, pd );

    // Slider track (right half, vertically centred)
    float track_x = x + half_w + 4.0f;
    float track_y = y + ROW_H * 0.5f - 3.0f;
    float track_w = half_w - 8.0f;
    // Une barre de 6 px au milieu de lignes epaissies pour le pouce a l'air
    // maigre, et ne montre pas qu'elle se manipule.
#ifdef __ANDROID__
    float track_h = 12.0f;
#else
    float track_h = 6.0f;
#endif

    // Track border + background
    pVideo->Draw_Rect( track_x - BORDER_T, track_y - BORDER_T,
                       track_w + BORDER_T * 2.0f, track_h + BORDER_T * 2.0f,
                       Z_CONT - 0.001f, &COL_BTN_BORDER );
    pVideo->Draw_Rect( track_x, track_y, track_w, track_h, Z_CONT, &COL_CARD_BG );

    // Filled portion
    float range = max_val - min_val;
    float t = ( range > 0.0f ) ? ( ( value - min_val ) / range ) : 0.0f;
    t = std::max( 0.0f, std::min( 1.0f, t ) );
    float fill_w = track_w * t;
    if( fill_w > 0.0f )
        pVideo->Draw_Rect( track_x, track_y, fill_w, track_h, Z_TEXT - 0.001f, &COL_SELECTED );

    // Thumb
#ifdef __ANDROID__
    float thumb_r = 11.0f;
#else
    float thumb_r = 6.0f;
#endif
    float thumb_x = track_x + fill_w - thumb_r;
    float thumb_y = track_y + track_h * 0.5f - thumb_r;
    pVideo->Draw_Rect( thumb_x - 1.0f, thumb_y - 1.0f,
                       thumb_r * 2.0f + 2.0f, thumb_r * 2.0f + 2.0f,
                       Z_TEXT - 0.001f, &COL_BTN_BORDER );
    pVideo->Draw_Rect( thumb_x, thumb_y, thumb_r * 2.0f, thumb_r * 2.0f,
                       Z_TEXT, &COL_BTN_BG );

    // Grab area. The track is 6 px tall and the thumb 12: aiming at that with
    // a finger means about 12 dp, well under what a touch target needs. The
    // whole height of the row counts instead, over the track's half of it.
    float hit_x = track_x - thumb_r;
    float hit_w = track_w + thumb_r * 2.0f;
    float hit_y = y;
    float hit_h = ROW_H;

    if( s_mouse_held && Hit( hit_x, hit_y, hit_w, hit_h ) && !s_slider_dragging )
    {
        s_slider_dragging = true;
        s_slider_drag_min = min_val;
        s_slider_drag_max = max_val;
        s_slider_drag_x   = track_x;
        s_slider_drag_w   = track_w;
    }

    if( s_slider_dragging )
    {
        float local_t = ( s_mouse_x - s_slider_drag_x ) / s_slider_drag_w;
        local_t = std::max( 0.0f, std::min( 1.0f, local_t ) );
        new_value = s_slider_drag_min + local_t * ( s_slider_drag_max - s_slider_drag_min );
    }
    else if( s_mouse_clicked && Hit( hit_x, hit_y, hit_w, hit_h ) )
    {
        // A plain tap anywhere on the track jumps there. Dragging a 12 px thumb
        // is a mouse gesture; on a touch screen the tap is the whole gesture.
        float local_t = ( s_mouse_x - track_x ) / track_w;
        local_t = std::max( 0.0f, std::min( 1.0f, local_t ) );
        new_value = min_val + local_t * ( max_val - min_val );
    }

    return new_value;
}

// ---------------------------------------------------------------------------
bool Button( float x, float y, float w, float h,
             const std::string &label, std::vector<cGL_Surface*> &pd )
{
    bool hover = Hit( x, y, w, h );

    // Border
    pVideo->Draw_Rect( x - BORDER_T, y - BORDER_T,
                       w + BORDER_T * 2.0f, h + BORDER_T * 2.0f,
                       Z_BG - 0.001f, &COL_BTN_BORDER );

    // Fill — slightly lighter when hovered
    Color bg = hover ? COL_HOVER : COL_BTN_BG;
    pVideo->Draw_Rect( x, y, w, h, Z_BG, &bg );

    // Label
    Color text_col = hover ? COL_BODY : COL_TITLE;
    Draw_Text_Centered( label, x, y, w, h, Z_TEXT, text_col, pd );

    return s_mouse_clicked && hover;
}

} // namespace ModernUI
