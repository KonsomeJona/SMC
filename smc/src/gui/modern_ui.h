/***************************************************************************
 * modern_ui.h  -  Lightweight stateless widget toolkit
 *
 * Widgets are drawn by calling a function each frame; state lives in the
 * caller. Style matches cHelpCard (golden header, cream/dark palette).
 * All widgets accept a pending_delete vector and push font surfaces into it
 * for deferred deletion after pVideo->Render().
 ***************************************************************************/
#ifndef SMC_MODERN_UI_H
#define SMC_MODERN_UI_H

#include <string>
#include <vector>
#include <SDL2/SDL.h>

namespace SMC
{
class cGL_Surface;  // forward declaration
}

namespace ModernUI
{

// Call once per frame BEFORE the SDL event poll loop to reset one-frame flags
// (s_mouse_clicked, s_scroll_delta). Must be called before Feed_Event.
void Begin_Frame( void );

// Feed each SDL event from the event poll loop. Call for every event.
void Feed_Event( const SDL_Event &e );

// Tab bar: draws tab labels, highlights active_tab.
// Returns new tab index if a tab was clicked this frame, else active_tab.
int Tab_Bar( float x, float y, float w, float h,
             const std::vector<std::string> &labels, int active_tab,
             std::vector<SMC::cGL_Surface*> &pd );

// Scrollable item list: draws items, highlights selected.
// scroll_offset is in item units (0 = top). Modified in-place when scrolled.
// Returns index of clicked item, -1 if none.
// Double-click (within SCROLL_LIST_DCLICK_MS on same item) sets SCROLL_LIST_DCLICK_FLAG:
//   use (result & ~SCROLL_LIST_DCLICK_FLAG) to get index,
//   (result & SCROLL_LIST_DCLICK_FLAG) != 0 means double-click.
static const int SCROLL_LIST_DCLICK_FLAG = 0x8000;
int Scroll_List( float x, float y, float w, float h,
                 const std::vector<std::string> &items, int selected,
                 int &scroll_offset, std::vector<SMC::cGL_Surface*> &pd );

// Toggle row: label on left, ON/OFF button on right.
// Returns new bool value if clicked, else current value.
bool Toggle_Row( float x, float y, float w,
                 const std::string &label, bool value,
                 std::vector<SMC::cGL_Surface*> &pd );

// Select row: label on left, < VALUE > arrows on right.
// Returns new index if arrows clicked, else current.
int Select_Row( float x, float y, float w,
                const std::string &label,
                const std::vector<std::string> &options, int current,
                std::vector<SMC::cGL_Surface*> &pd );

// Slider row: label on left, horizontal slider on right.
// Returns new float value if dragged/clicked, else current.
float Slider_Row( float x, float y, float w,
                  const std::string &label, float value, float min_val, float max_val,
                  std::vector<SMC::cGL_Surface*> &pd );

// Button: draws a centered-label button.
// Returns true if clicked this frame.
bool Button( float x, float y, float w, float h,
             const std::string &label, std::vector<SMC::cGL_Surface*> &pd );

} // namespace ModernUI

#endif
