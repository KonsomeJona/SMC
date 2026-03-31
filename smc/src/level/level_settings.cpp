/***************************************************************************
 * level_settings.cpp  - level editor settings class
 *
 * Copyright (C) 2006 - 2011 Florian Richter
 ***************************************************************************/
/*
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "../core/global_basic.h"
#include "../core/game_core.h"
#include "../level/level_settings.h"
#include "../input/mouse.h"
#include "../level/level.h"
#include "../video/font.h"
#include "../video/renderer.h"
#include "../core/filesystem/filesystem.h"
#include "../core/framerate.h"
#include "../audio/audio.h"
#include "../gui/generic.h"
#include "../core/i18n.h"
#include "../gui/modern_ui.h"
#include "../video/gl_surface.h"
// CEGUI (still needed for Game_Action_Data_Start/End and XMLAttributes helpers)
#include <CEGUI/WindowManager.h>
#include <CEGUI/widgets/Spinner.h>
#include <CEGUI/widgets/Editbox.h>
#include <CEGUI/widgets/Combobox.h>
#include <CEGUI/widgets/MultiLineEditbox.h>
#include <CEGUI/widgets/PushButton.h>
#include <CEGUI/widgets/TabControl.h>
#include <CEGUI/widgets/Slider.h>
#include <CEGUI/widgets/Listbox.h>
#include <CEGUI/widgets/ListboxTextItem.h>

#include <cstdio>
#include <algorithm>

namespace SMC
{

/* *** *** *** *** *** cLevel_Settings *** *** *** *** *** *** *** *** *** *** *** *** */

// ---------------------------------------------------------------------------
// Persistent dialog state — lives between Enter() and Leave() calls.
// ---------------------------------------------------------------------------
namespace
{

struct LevelSettingsState
{
    // navigation
    int  active_tab;            // 0=Main  1=Background  2=Global Effect

    // Main tab
    std::string level_filename;
    std::string music_filename;
    std::string author;
    std::string version;
    std::string description;
    float       difficulty;     // 0–100
    int         land_type_idx;  // 0 .. LLT_LAST-1
    float       cam_limit_w;
    float       cam_limit_h;
    float       fixed_hor_vel;

    // Background tab: image layers
    std::vector<cBackground*> bg_images;  // non-gradient layers
    int  bg_selected;
    int  bg_scroll_offset;

    // Edit fields for the currently selected bg image
    int         bg_type_idx;
    std::string bg_filename;
    float       bg_pos_x;
    float       bg_pos_y;
    float       bg_pos_z;
    float       bg_speed_x;
    float       bg_speed_y;
    float       bg_const_vel_x;
    float       bg_const_vel_y;

    // pending-delete GL surfaces from the last Draw() call
    std::vector<cGL_Surface*> pd;

    bool initialised;
};

static LevelSettingsState g_ls;

// Background type arrays (BG_NONE, BG_IMG_TOP, BG_IMG_BOTTOM, BG_IMG_ALL)
static const char* const  BG_TYPE_NAMES[]  = { "Disabled", "Top", "Bottom", "All" };
static const BackgroundType BG_TYPE_VALUES[] = { BG_NONE, BG_IMG_TOP, BG_IMG_BOTTOM, BG_IMG_ALL };
static const int BG_TYPE_COUNT = 4;

} // anonymous namespace

// ---------------------------------------------------------------------------
// Layout constants
// ---------------------------------------------------------------------------
static const float LS_PANEL_X    = 40.0f;
static const float LS_PANEL_Y    = 30.0f;
static const float LS_PANEL_W    = 720.0f;
static const float LS_PANEL_H    = 520.0f;
static const float LS_TAB_H      = 28.0f;
static const float LS_ROW_H      = 26.0f;
static const float LS_ROW_STEP   = 32.0f;
static const float LS_BTN_H      = 26.0f;
static const float LS_BTN_W      = 120.0f;
static const float LS_CONTENT_PAD = 10.0f;

// ---------------------------------------------------------------------------
// Colors
// ---------------------------------------------------------------------------
static const Color LS_COL_PANEL  = Color( static_cast<Uint8>(0x1A), static_cast<Uint8>(0x1A), static_cast<Uint8>(0x2E), static_cast<Uint8>(230) );
static const Color LS_COL_HEADER = Color( static_cast<Uint8>(0xC8), static_cast<Uint8>(0x96), static_cast<Uint8>(0x28), static_cast<Uint8>(255) );
static const Color LS_COL_TITLE  = Color( static_cast<Uint8>(0xFF), static_cast<Uint8>(0xFF), static_cast<Uint8>(0xFF), static_cast<Uint8>(255) );
static const Color LS_COL_BODY   = Color( static_cast<Uint8>(0xE0), static_cast<Uint8>(0xD8), static_cast<Uint8>(0xC8), static_cast<Uint8>(255) );
static const Color LS_COL_DIM    = Color( static_cast<Uint8>(0x80), static_cast<Uint8>(0x78), static_cast<Uint8>(0x60), static_cast<Uint8>(200) );
static const Color LS_COL_SEC    = Color( static_cast<Uint8>(0xC8), static_cast<Uint8>(0x96), static_cast<Uint8>(0x28), static_cast<Uint8>(220) );

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::vector<std::string> Build_Land_Type_Options( void )
{
    std::vector<std::string> v;
    v.reserve( LLT_LAST );
    for( int i = 0; i < LLT_LAST; ++i )
        v.push_back( Get_Level_Land_Type_Name( static_cast<LevelLandType>( i ) ) );
    return v;
}

static void Rebuild_BG_Image_List( cLevel *level )
{
    g_ls.bg_images.clear();
    for( auto *bg : level->m_background_manager->objects )
    {
        if( bg->m_type != BG_GR_HOR && bg->m_type != BG_GR_VER )
            g_ls.bg_images.push_back( bg );
    }
    if( g_ls.bg_selected >= static_cast<int>( g_ls.bg_images.size() ) )
        g_ls.bg_selected = static_cast<int>( g_ls.bg_images.size() ) - 1;
}

static void Load_Selected_BG_Fields( void )
{
    if( g_ls.bg_selected < 0 ||
        g_ls.bg_selected >= static_cast<int>( g_ls.bg_images.size() ) )
    {
        g_ls.bg_type_idx    = 0;
        g_ls.bg_filename    = "";
        g_ls.bg_pos_x       = 0.0f;
        g_ls.bg_pos_y       = 0.0f;
        g_ls.bg_pos_z       = 0.0f;
        g_ls.bg_speed_x     = 1.0f;
        g_ls.bg_speed_y     = 1.0f;
        g_ls.bg_const_vel_x = 0.0f;
        g_ls.bg_const_vel_y = 0.0f;
        return;
    }

    cBackground *bg = g_ls.bg_images[ g_ls.bg_selected ];

    g_ls.bg_type_idx = 0;
    for( int i = 0; i < BG_TYPE_COUNT; ++i )
    {
        if( BG_TYPE_VALUES[i] == bg->m_type )
        {
            g_ls.bg_type_idx = i;
            break;
        }
    }
    g_ls.bg_filename    = bg->m_image_1_filename;
    g_ls.bg_pos_x       = bg->m_start_pos_x;
    g_ls.bg_pos_y       = bg->m_start_pos_y;
    g_ls.bg_pos_z       = bg->m_pos_z;
    g_ls.bg_speed_x     = bg->m_speed_x;
    g_ls.bg_speed_y     = bg->m_speed_y;
    g_ls.bg_const_vel_x = bg->m_const_vel_x;
    g_ls.bg_const_vel_y = bg->m_const_vel_y;
}

static void Commit_BG_Fields( void )
{
    if( g_ls.bg_selected < 0 ||
        g_ls.bg_selected >= static_cast<int>( g_ls.bg_images.size() ) )
        return;

    cBackground *bg = g_ls.bg_images[ g_ls.bg_selected ];

    bg->Set_Type( BG_TYPE_VALUES[ g_ls.bg_type_idx ] );
    bg->Set_Start_Pos( g_ls.bg_pos_x, g_ls.bg_pos_y );
    bg->Set_Pos_Z( g_ls.bg_pos_z );
    bg->Set_Scroll_Speed( g_ls.bg_speed_x, g_ls.bg_speed_y );
    bg->Set_Const_Velocity_X( g_ls.bg_const_vel_x );
    bg->Set_Const_Velocity_Y( g_ls.bg_const_vel_y );

    // validate and apply filename
    std::string full = g_ls.bg_filename;
    if( !full.empty() && full.find( DATA_DIR ) == std::string::npos )
        full = std::string(DATA_DIR) + "/" + GAME_PIXMAPS_DIR + "/" + g_ls.bg_filename;
    if( !g_ls.bg_filename.empty() && !File_Exists( full ) )
        full.clear();
    bg->Set_Image( full.empty() ? g_ls.bg_filename : full );
}

// Draw a text label then a button showing the value; returns true if clicked.
static bool Draw_Text_Field_Row( const std::string &label, const std::string &value,
                                  float x, float y, float w,
                                  float z, std::vector<cGL_Surface*> &pd )
{
    const float lw = w * 0.40f;
    const float bw = w * 0.58f;
    const float bx = x + lw + w * 0.02f;

    cGL_Surface *ls = pFont->Render_Text( pFont->m_font_small, label, LS_COL_BODY );
    if( ls )
    {
        ls->Blit( x, y + ( LS_ROW_H - ls->m_h ) * 0.5f, z );
        pd.push_back( ls );
    }
    return ModernUI::Button( bx, y, bw, LS_ROW_H, value, pd );
}

// ---------------------------------------------------------------------------
// cLevel_Settings
// ---------------------------------------------------------------------------

cLevel_Settings :: cLevel_Settings( cSprite_Manager *sprite_manager, cLevel *level )
{
    m_active      = 0;
    m_level       = level;
    m_camera      = new cCamera( sprite_manager );
    m_gui_window  = NULL;
    m_tabcontrol  = NULL;
    m_spinner_difficulty   = NULL;
    m_slider_difficulty    = NULL;
    m_text_difficulty_name = NULL;
}

cLevel_Settings :: ~cLevel_Settings( void )
{
    Unload();
    delete m_camera;
}

void cLevel_Settings :: Init( void )
{
    // free any surfaces from a previous session
    for( auto *s : g_ls.pd ) delete s;
    g_ls.pd.clear();

    g_ls.active_tab = 0;

    // Main tab
    g_ls.level_filename = Trim_Filename( m_level->m_level_filename, 0, 0 );
    g_ls.music_filename = m_level->Get_Music_Filename( 1 );
    g_ls.author         = m_level->m_author;
    g_ls.version        = m_level->m_version;
    g_ls.description    = m_level->m_description;
    g_ls.difficulty     = static_cast<float>( m_level->m_difficulty );
    g_ls.land_type_idx  = static_cast<int>( m_level->m_land_type );
    g_ls.cam_limit_w    = m_level->m_camera_limits.m_w;
    g_ls.cam_limit_h    = m_level->m_camera_limits.m_h;
    g_ls.fixed_hor_vel  = m_level->m_fixed_camera_hor_vel;

    // Background: gradient colors
    m_bg_color_1 = Color( m_level->m_background_manager->Get_Pointer(0)->m_color_1.red,
                          m_level->m_background_manager->Get_Pointer(0)->m_color_1.green,
                          m_level->m_background_manager->Get_Pointer(0)->m_color_1.blue,
                          255 );
    m_bg_color_2 = Color( m_level->m_background_manager->Get_Pointer(0)->m_color_2.red,
                          m_level->m_background_manager->Get_Pointer(0)->m_color_2.green,
                          m_level->m_background_manager->Get_Pointer(0)->m_color_2.blue,
                          255 );

    // Background: image layers
    g_ls.bg_selected      = -1;
    g_ls.bg_scroll_offset = 0;
    Rebuild_BG_Image_List( m_level );
    Load_Selected_BG_Fields();

    g_ls.initialised = true;
}

void cLevel_Settings :: Exit( void )
{
    // Back to level
    Game_Action = GA_ENTER_LEVEL;
    Game_Action_Data_Start.add( "screen_fadeout",
        CEGUI::PropertyHelper<int>::toString( EFFECT_OUT_BLACK ) );
    Game_Action_Data_Start.add( "screen_fadeout_speed", "3" );
    Game_Action_Data_End.add( "screen_fadein",
        CEGUI::PropertyHelper<int>::toString( EFFECT_IN_BLACK ) );
    Game_Action_Data_End.add( "screen_fadein_speed", "3" );
}

void cLevel_Settings :: Enter( void )
{
    pActive_Camera = m_camera;
    editor_enabled = 0;

    if( pMouseCursor->m_active_object )
        pMouseCursor->Clear_Active_Object();

    Init();
    m_active = 1;
    m_camera->Update_Position();
}

void cLevel_Settings :: Leave( void )
{
    if( !g_ls.initialised )
    {
        Unload();
        return;
    }

    // Main tab: filename
    if( g_ls.level_filename.length() > 1 &&
        Trim_Filename( m_level->m_level_filename, 0, 0 ).compare( g_ls.level_filename ) != 0 )
    {
        m_level->Set_Filename( g_ls.level_filename );
        if( Box_Question( _("Save ") + Trim_Filename( g_ls.level_filename, 0, 0 ) + " ?" ) )
            m_level->Save();
    }

    // music – crossfade if changed
    if( pAudio->Is_Music_Playing() &&
        g_ls.music_filename.compare( m_level->Get_Music_Filename( 1 ) ) != 0 )
    {
        m_level->Set_Music( g_ls.music_filename );
        pAudio->Fadeout_Music( 1000 );
    }
    else
    {
        m_level->Set_Music( g_ls.music_filename );
    }

    m_level->Set_Author( g_ls.author );
    m_level->Set_Version( g_ls.version );
    m_level->Set_Description( g_ls.description );
    m_level->Set_Difficulty( static_cast<Uint8>( g_ls.difficulty ) );
    m_level->Set_Land_Type( static_cast<LevelLandType>( g_ls.land_type_idx ) );

    // Camera
    pLevel_Manager->m_camera->Set_Limit_W( g_ls.cam_limit_w );
    pLevel_Manager->m_camera->Set_Limit_H( g_ls.cam_limit_h );
    m_level->m_camera_limits.m_w = pLevel_Manager->m_camera->m_limit_rect.m_w;
    m_level->m_camera_limits.m_h = pLevel_Manager->m_camera->m_limit_rect.m_h;
    pLevel_Manager->m_camera->m_fixed_hor_vel = g_ls.fixed_hor_vel;
    m_level->m_fixed_camera_hor_vel           = pLevel_Manager->m_camera->m_fixed_hor_vel;

    // Background: gradient
    m_level->m_background_manager->Get_Pointer(0)->Set_Color_1( m_bg_color_1 );
    m_level->m_background_manager->Get_Pointer(0)->Set_Color_2( m_bg_color_2 );

    // Background image layers are committed live during Draw()

    Unload();
}

void cLevel_Settings :: Unload( void )
{
    for( auto *s : g_ls.pd ) delete s;
    g_ls.pd.clear();
    g_ls.initialised = false;
    m_active = 0;
}

void cLevel_Settings :: Update( void )
{
    pFramerate->m_perf_timer[PERF_UPDATE_LEVEL_SETTINGS]->Update();
}

void cLevel_Settings :: Draw( void )
{
    // Free surfaces queued in the previous frame
    for( auto *s : g_ls.pd ) delete s;
    g_ls.pd.clear();

    pVideo->Clear_Screen();
    pVideo->Draw_Rect( NULL, 0.00001f, &black );

    if( !g_ls.initialised )
    {
        pFramerate->m_perf_timer[PERF_DRAW_LEVEL_SETTINGS]->Update();
        return;
    }

    const float panel_z = 0.85f;

    // Panel background
    pVideo->Draw_Rect( LS_PANEL_X, LS_PANEL_Y, LS_PANEL_W, LS_PANEL_H,
                       panel_z, &LS_COL_PANEL );

    // Title bar
    pVideo->Draw_Rect( LS_PANEL_X, LS_PANEL_Y, LS_PANEL_W, 24.0f,
                       panel_z + 0.001f, &LS_COL_HEADER );
    {
        cGL_Surface *t = pFont->Render_Text( pFont->m_font_small,
                                             _("Level Settings"), LS_COL_TITLE );
        if( t )
        {
            t->Blit( LS_PANEL_X + ( LS_PANEL_W - t->m_w ) * 0.5f,
                     LS_PANEL_Y + ( 24.0f - t->m_h ) * 0.5f,
                     panel_z + 0.002f );
            g_ls.pd.push_back( t );
        }
    }

    // Tab bar
    const float tab_y = LS_PANEL_Y + 24.0f;
    const std::vector<std::string> tab_labels = {
        _("Main"), _("Background"), _("Global Effect")
    };
    g_ls.active_tab = ModernUI::Tab_Bar( LS_PANEL_X, tab_y,
                                          LS_PANEL_W, LS_TAB_H,
                                          tab_labels, g_ls.active_tab,
                                          g_ls.pd );

    // Content area (leaves room for OK/Cancel at the bottom)
    const float content_x = LS_PANEL_X + LS_CONTENT_PAD;
    const float content_y = tab_y + LS_TAB_H + LS_CONTENT_PAD;
    const float content_w = LS_PANEL_W - 2.0f * LS_CONTENT_PAD;
    const float row_x     = content_x;
    const float row_w     = content_w;
    const float text_z    = panel_z + 0.01f;

    // -----------------------------------------------------------------------
    // TAB 0: Main
    // -----------------------------------------------------------------------
    if( g_ls.active_tab == 0 )
    {
        float ry = content_y;

        static const std::vector<std::string> land_opts = Build_Land_Type_Options();

        // Text fields (click to edit via Box_Text_Input)
        if( Draw_Text_Field_Row( _("Name"), g_ls.level_filename, row_x, ry, row_w, text_z, g_ls.pd ) )
        {
            std::string v = Box_Text_Input( g_ls.level_filename, _("Level Filename") );
            if( !v.empty() ) g_ls.level_filename = v;
        }
        ry += LS_ROW_STEP;

        if( Draw_Text_Field_Row( _("Music"), g_ls.music_filename, row_x, ry, row_w, text_z, g_ls.pd ) )
        {
            std::string v = Box_Text_Input( g_ls.music_filename, _("Music Filename") );
            if( !v.empty() ) g_ls.music_filename = v;
        }
        ry += LS_ROW_STEP;

        if( Draw_Text_Field_Row( _("Author"), g_ls.author, row_x, ry, row_w, text_z, g_ls.pd ) )
        {
            std::string v = Box_Text_Input( g_ls.author, _("Author") );
            if( !v.empty() ) g_ls.author = v;
        }
        ry += LS_ROW_STEP;

        if( Draw_Text_Field_Row( _("Version"), g_ls.version, row_x, ry, row_w, text_z, g_ls.pd ) )
        {
            std::string v = Box_Text_Input( g_ls.version, _("Version") );
            if( !v.empty() ) g_ls.version = v;
        }
        ry += LS_ROW_STEP;

        if( Draw_Text_Field_Row( _("Description"), g_ls.description, row_x, ry, row_w, text_z, g_ls.pd ) )
        {
            std::string v = Box_Text_Input( g_ls.description, _("Description") );
            if( !v.empty() ) g_ls.description = v;
        }
        ry += LS_ROW_STEP;

        // Difficulty slider (label includes the named difficulty string)
        {
            const std::string diff_label =
                std::string(_("Difficulty")) + " (" +
                Get_Difficulty_Name( static_cast<Uint8>( g_ls.difficulty ) ) + ")";
            float nv = ModernUI::Slider_Row( row_x, ry, row_w,
                                              diff_label, g_ls.difficulty,
                                              0.0f, 100.0f, g_ls.pd );
            if( nv != g_ls.difficulty ) g_ls.difficulty = nv;
            ry += LS_ROW_STEP;
        }

        // Land type
        {
            int nv = ModernUI::Select_Row( row_x, ry, row_w,
                                           _("Land Type"), land_opts,
                                           g_ls.land_type_idx, g_ls.pd );
            if( nv != g_ls.land_type_idx ) g_ls.land_type_idx = nv;
            ry += LS_ROW_STEP;
        }

        // Camera width
        {
            float nv = ModernUI::Slider_Row( row_x, ry, row_w,
                                              _("Camera Width"), g_ls.cam_limit_w,
                                              0.0f, 32767.0f, g_ls.pd );
            if( nv != g_ls.cam_limit_w ) g_ls.cam_limit_w = nv;
            ry += LS_ROW_STEP;
        }

        // Camera height (negative values allowed)
        {
            float nv = ModernUI::Slider_Row( row_x, ry, row_w,
                                              _("Camera Height"), g_ls.cam_limit_h,
                                              -32767.0f, 0.0f, g_ls.pd );
            if( nv != g_ls.cam_limit_h ) g_ls.cam_limit_h = nv;
            ry += LS_ROW_STEP;
        }

        // Fixed horizontal velocity
        {
            float nv = ModernUI::Slider_Row( row_x, ry, row_w,
                                              _("Fixed Hor. Vel."), g_ls.fixed_hor_vel,
                                              -100.0f, 100.0f, g_ls.pd );
            if( nv != g_ls.fixed_hor_vel ) g_ls.fixed_hor_vel = nv;
            ry += LS_ROW_STEP;
        }

        // Last save time (read-only)
        {
            std::string s = _("Last saved: ") +
                            Time_to_String( m_level->m_last_saved, "%Y-%m-%d %H:%M:%S" );
            cGL_Surface *ts = pFont->Render_Text( pFont->m_font_small, s, LS_COL_DIM );
            if( ts )
            {
                ts->Blit( row_x, ry + ( LS_ROW_H - ts->m_h ) * 0.5f, text_z );
                g_ls.pd.push_back( ts );
            }
        }
    }

    // -----------------------------------------------------------------------
    // TAB 1: Background
    // -----------------------------------------------------------------------
    else if( g_ls.active_tab == 1 )
    {
        float ry = content_y;

        // Section header
        {
            cGL_Surface *sh = pFont->Render_Text( pFont->m_font_small,
                                                   _("Gradient Colors"), LS_COL_SEC );
            if( sh ) { sh->Blit( row_x, ry, text_z ); g_ls.pd.push_back( sh ); }
            ry += 20.0f;
        }

        // Color channel sliders — modifies m_bg_color_1 / m_bg_color_2 in-place
        auto color_slider = [&]( const std::string &label, Uint8 &ch, float y )
        {
            float fv = static_cast<float>( ch );
            float nv = ModernUI::Slider_Row( row_x, y, row_w, label, fv, 0.0f, 255.0f, g_ls.pd );
            if( nv != fv ) ch = static_cast<Uint8>( nv );
        };

        color_slider( _("Start R"), m_bg_color_1.red,   ry ); ry += LS_ROW_STEP;
        color_slider( _("Start G"), m_bg_color_1.green, ry ); ry += LS_ROW_STEP;
        color_slider( _("Start B"), m_bg_color_1.blue,  ry ); ry += LS_ROW_STEP;
        color_slider( _("End R"),   m_bg_color_2.red,   ry ); ry += LS_ROW_STEP;
        color_slider( _("End G"),   m_bg_color_2.green, ry ); ry += LS_ROW_STEP;
        color_slider( _("End B"),   m_bg_color_2.blue,  ry ); ry += LS_ROW_STEP + 4.0f;

        // Section header
        {
            cGL_Surface *sh = pFont->Render_Text( pFont->m_font_small,
                                                   _("Image Layers"), LS_COL_SEC );
            if( sh ) { sh->Blit( row_x, ry, text_z ); g_ls.pd.push_back( sh ); }
            ry += 20.0f;
        }

        // Build display names for the scroll list
        std::vector<std::string> bg_display;
        bg_display.reserve( g_ls.bg_images.size() );
        for( auto *bg : g_ls.bg_images )
        {
            char buf[80];
            std::snprintf( buf, sizeof(buf), "z=%.3f  %s",
                           bg->m_pos_z,
                           bg->m_image_1_filename.empty() ? "(no image)" :
                               Trim_Filename( bg->m_image_1_filename, 0, 0 ).c_str() );
            bg_display.push_back( buf );
        }

        const float list_w = row_w * 0.45f;
        const float list_h = 120.0f;
        const int   old_sel = g_ls.bg_selected;

        int sel = ModernUI::Scroll_List( row_x, ry, list_w, list_h,
                                          bg_display, g_ls.bg_selected,
                                          g_ls.bg_scroll_offset, g_ls.pd );
        if( sel >= 0 )
        {
            int idx = sel & ~ModernUI::SCROLL_LIST_DCLICK_FLAG;
            if( idx != old_sel )
            {
                if( old_sel >= 0 ) Commit_BG_Fields();
                g_ls.bg_selected = idx;
                Load_Selected_BG_Fields();
            }
        }

        // Add / Delete buttons to the right of the list
        const float btn_x  = row_x + list_w + 8.0f;
        const float btn_bw = row_w - list_w - 8.0f;
        const bool  can_add = static_cast<int>( m_level->m_background_manager->size() ) < 10;

        if( can_add )
        {
            if( ModernUI::Button( btn_x, ry, btn_bw, LS_BTN_H, _("Add"), g_ls.pd ) )
            {
                cBackground *bg = new cBackground( m_level->m_sprite_manager );
                bg->Set_Type( BG_IMG_BOTTOM );
                bg->Set_Image( LEVEL_DEFAULT_BACKGROUND );
                m_level->m_background_manager->Add( bg );
                Rebuild_BG_Image_List( m_level );
                g_ls.bg_selected = static_cast<int>( g_ls.bg_images.size() ) - 1;
                Load_Selected_BG_Fields();
            }
        }
        else
        {
            cGL_Surface *gs = pFont->Render_Text( pFont->m_font_small,
                                                   _("Max layers reached"), LS_COL_DIM );
            if( gs ) { gs->Blit( btn_x, ry + 4.0f, text_z ); g_ls.pd.push_back( gs ); }
        }

        if( g_ls.bg_selected >= 0 )
        {
            if( ModernUI::Button( btn_x, ry + LS_ROW_STEP, btn_bw, LS_BTN_H,
                                   _("Delete"), g_ls.pd ) )
            {
                cBackground *bg = g_ls.bg_images[ g_ls.bg_selected ];
                m_level->m_background_manager->Delete( bg );
                Rebuild_BG_Image_List( m_level );
                g_ls.bg_selected = -1;
                Load_Selected_BG_Fields();
            }
        }

        ry += list_h + 8.0f;

        // Edit fields for the selected layer
        if( g_ls.bg_selected >= 0 )
        {
            // Type
            {
                std::vector<std::string> type_opts;
                for( int i = 0; i < BG_TYPE_COUNT; ++i )
                    type_opts.push_back( BG_TYPE_NAMES[i] );

                int nv = ModernUI::Select_Row( row_x, ry, row_w,
                                               _("Type"), type_opts,
                                               g_ls.bg_type_idx, g_ls.pd );
                if( nv != g_ls.bg_type_idx )
                {
                    g_ls.bg_type_idx = nv;
                    Commit_BG_Fields();
                }
                ry += LS_ROW_STEP;
            }

            // Image filename (click to edit)
            if( Draw_Text_Field_Row( _("Image File"), g_ls.bg_filename,
                                      row_x, ry, row_w, text_z, g_ls.pd ) )
            {
                std::string v = Box_Text_Input( g_ls.bg_filename,
                                                _("Background Image Filename") );
                if( !v.empty() )
                {
                    g_ls.bg_filename = v;
                    Commit_BG_Fields();
                }
            }
            ry += LS_ROW_STEP;

            // Z depth
            {
                float nv = ModernUI::Slider_Row( row_x, ry, row_w,
                                                  _("Pos Z"), g_ls.bg_pos_z,
                                                  0.00011f, 0.121f, g_ls.pd );
                if( nv != g_ls.bg_pos_z ) { g_ls.bg_pos_z = nv; Commit_BG_Fields(); }
                ry += LS_ROW_STEP;
            }

            // Speed X/Y
            {
                float nv = ModernUI::Slider_Row( row_x, ry, row_w,
                                                  _("Speed X"), g_ls.bg_speed_x,
                                                  0.0f, 32767.0f, g_ls.pd );
                if( nv != g_ls.bg_speed_x ) { g_ls.bg_speed_x = nv; Commit_BG_Fields(); }
                ry += LS_ROW_STEP;

                nv = ModernUI::Slider_Row( row_x, ry, row_w,
                                           _("Speed Y"), g_ls.bg_speed_y,
                                           0.0f, 32767.0f, g_ls.pd );
                if( nv != g_ls.bg_speed_y ) { g_ls.bg_speed_y = nv; Commit_BG_Fields(); }
                ry += LS_ROW_STEP;
            }

            // Constant velocity X/Y
            {
                float nv = ModernUI::Slider_Row( row_x, ry, row_w,
                                                  _("Const Vel X"), g_ls.bg_const_vel_x,
                                                  -32767.0f, 32767.0f, g_ls.pd );
                if( nv != g_ls.bg_const_vel_x ) { g_ls.bg_const_vel_x = nv; Commit_BG_Fields(); }
                ry += LS_ROW_STEP;

                nv = ModernUI::Slider_Row( row_x, ry, row_w,
                                           _("Const Vel Y"), g_ls.bg_const_vel_y,
                                           -32767.0f, 32767.0f, g_ls.pd );
                if( nv != g_ls.bg_const_vel_y ) { g_ls.bg_const_vel_y = nv; Commit_BG_Fields(); }
                ry += LS_ROW_STEP;
            }
        }
    }

    // -----------------------------------------------------------------------
    // TAB 2: Global Effect
    // -----------------------------------------------------------------------
    else if( g_ls.active_tab == 2 )
    {
        const float content_h = LS_PANEL_H - 24.0f - LS_TAB_H - LS_CONTENT_PAD * 2.0f - 40.0f;
        cGL_Surface *ts = pFont->Render_Text( pFont->m_font_small,
            _("The Global Effect is now a Particle Emitter."), LS_COL_BODY );
        if( ts )
        {
            ts->Blit( content_x + ( content_w - ts->m_w ) * 0.5f,
                      content_y + content_h * 0.4f,
                      text_z );
            g_ls.pd.push_back( ts );
        }
    }

    // -----------------------------------------------------------------------
    // OK / Cancel buttons
    // -----------------------------------------------------------------------
    {
        const float btn_y    = LS_PANEL_Y + LS_PANEL_H - 36.0f;
        const float ok_x     = LS_PANEL_X + LS_PANEL_W * 0.5f - LS_BTN_W - 8.0f;
        const float cancel_x = LS_PANEL_X + LS_PANEL_W * 0.5f + 8.0f;

        if( ModernUI::Button( ok_x, btn_y, LS_BTN_W, LS_BTN_H, _("OK"), g_ls.pd ) )
        {
            Leave();
            Exit();
            return;
        }
        if( ModernUI::Button( cancel_x, btn_y, LS_BTN_W, LS_BTN_H, _("Cancel"), g_ls.pd ) )
        {
            Unload();
            Exit();
            return;
        }
    }

    pFramerate->m_perf_timer[PERF_DRAW_LEVEL_SETTINGS]->Update();
}

bool cLevel_Settings :: Key_Down( SDLKey key )
{
    if( !m_active )
        return 0;

    if( key == SDLK_ESCAPE )
    {
        Unload();
        Exit();
    }
    else if( key == SDLK_RETURN || key == SDLK_KP_ENTER )
    {
        Leave();
        Exit();
    }
    else
    {
        return 0;
    }

    return 1;
}

void cLevel_Settings :: Set_Level( cLevel *level )
{
    m_level = level;
}

void cLevel_Settings :: Set_Sprite_Manager( cSprite_Manager *sprite_manager )
{
    m_camera->Set_Sprite_Manager( sprite_manager );
}

// ---------------------------------------------------------------------------
// CEGUI callback stubs (interface kept for binary compatibility)
// ---------------------------------------------------------------------------

bool cLevel_Settings :: Add_Background_Image( const CEGUI::EventArgs &event )
{
    return 1;
}

bool cLevel_Settings :: Delete_Background_Image( const CEGUI::EventArgs &event )
{
    return 1;
}

bool cLevel_Settings :: Set_Background_Image( const CEGUI::EventArgs &event )
{
    return 1;
}

bool cLevel_Settings :: Button_Apply( const CEGUI::EventArgs &event )
{
    Leave();
    Exit();
    return 1;
}

bool cLevel_Settings :: Update_BG_Colors( const CEGUI::EventArgs &event )
{
    return 1;
}

void cLevel_Settings :: Load_BG_Image_List( void )
{
    Rebuild_BG_Image_List( m_level );
}

bool cLevel_Settings :: Update_BG_Image( const CEGUI::EventArgs &event )
{
    return 1;
}

void cLevel_Settings :: Clear_Layer_Field( void )
{
    g_ls.bg_selected = -1;
    Load_Selected_BG_Fields();
}

bool cLevel_Settings :: Spinner_Difficulty_Changed( const CEGUI::EventArgs &event )
{
    return 1;
}

bool cLevel_Settings :: Slider_Difficulty_Changed( const CEGUI::EventArgs &event )
{
    return 1;
}

/* *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** *** */

} // namespace SMC
