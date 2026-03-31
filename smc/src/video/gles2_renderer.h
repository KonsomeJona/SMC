#ifndef SMC_GLES2_RENDERER_H
#define SMC_GLES2_RENDERER_H

#ifdef __ANDROID__
#include <GLES2/gl2.h>
#include <SDL2/SDL.h>  // for Uint8

namespace GLES2 {

// Initialize shaders and VBO. Call once after GL context is created.
void Init(void);
void Shutdown(void);

// Set the orthographic projection (call when screen size changes).
// Maps [0..w] x [0..h] to NDC, with y=0 at top (matches desktop glOrtho setup).
void Set_Projection(float w, float h);

// Draw a textured quad (GL_TRIANGLE_FAN, 4 vertices)
// x, y  = top-left corner in screen space
// w, h  = size
// tex   = GL texture ID
// u0..v1 = texture coordinate rectangle (default full [0,1])
// r,g,b,a = color modulation (255 = full intensity / no tint)
// rot   = rotation in degrees around quad center (counter-clockwise in screen space)
void Draw_Texture(float x, float y, float w, float h,
                  GLuint tex,
                  float u0 = 0.0f, float v0 = 0.0f,
                  float u1 = 1.0f, float v1 = 1.0f,
                  Uint8 r = 255, Uint8 g = 255, Uint8 b = 255, Uint8 a = 255,
                  float rot = 0.0f);

// Draw a filled rectangle (no texture)
void Draw_Rect(float x, float y, float w, float h,
               Uint8 r, Uint8 g, Uint8 b, Uint8 a);

// Draw a filled circle approximated with a triangle fan (~32 segments)
void Draw_Circle(float cx, float cy, float radius,
                 Uint8 r, Uint8 g, Uint8 b, Uint8 a);

// Draw a line
void Draw_Line(float x1, float y1, float x2, float y2,
               Uint8 r, Uint8 g, Uint8 b, Uint8 a, float width = 1.0f);

// Draw a gradient rectangle
// DIR_VERTICAL:   top = color1, bottom = color2
// DIR_HORIZONTAL: left = color1, right = color2
void Draw_Gradient_Vertical(float x, float y, float w, float h,
                             Uint8 r1, Uint8 g1, Uint8 b1, Uint8 a1,
                             Uint8 r2, Uint8 g2, Uint8 b2, Uint8 a2);

void Draw_Gradient_Horizontal(float x, float y, float w, float h,
                               Uint8 r1, Uint8 g1, Uint8 b1, Uint8 a1,
                               Uint8 r2, Uint8 g2, Uint8 b2, Uint8 a2);

} // namespace GLES2

#endif // __ANDROID__
#endif // SMC_GLES2_RENDERER_H
