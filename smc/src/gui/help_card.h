/***************************************************************************
 * help_card.h  -  Modern help card overlay (replaces CEGUI text popup)
 ***************************************************************************/
#ifndef SMC_HELP_CARD_H
#define SMC_HELP_CARD_H

#include <string>
#include <vector>
#include <SDL2/SDL.h>

namespace SMC
{

class cGL_Surface;  // forward declaration

enum HelpCardIcon
{
    ICON_HINT = 0,
    ICON_STAR,
    ICON_WARNING
};

class cHelpCard
{
public:
    /* title  : bold header text (e.g. "Hint!")
     * body   : multi-line message text
     * icon   : which icon to show in header (ICON_HINT, ICON_STAR, ICON_WARNING)
     */
    cHelpCard( const std::string &title, const std::string &body, HelpCardIcon icon = ICON_HINT );

    /* Blocks until dismissed. Handles its own SDL event loop.
     * Pauses game updates while visible (like the original CEGUI loop).
     */
    void Run( void );

private:
    // anim_t: 0.0=hidden, 1.0=fully shown. Queues GL surfaces into pending_delete;
    // caller must delete them AFTER pVideo->Render() flushes the render queue.
    void Render( float anim_t, std::vector<cGL_Surface*> &pending_delete );
    bool Handle_Event( const SDL_Event &e );
    void Render_Text_Wrapped( const std::string &text, float x, float y, float max_w, float line_h, float clip_height, std::vector<cGL_Surface*> &pending_delete );

    std::string m_title;
    std::string m_body;
    HelpCardIcon m_icon;

    float m_scroll_offset;   // pixels scrolled in body
    bool  m_closing;
};

} // namespace SMC

#endif
