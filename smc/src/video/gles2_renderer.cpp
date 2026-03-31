/***************************************************************************
 * gles2_renderer.cpp  -  OpenGL ES 2.0 shader-based draw calls
 *
 * Replaces the fixed-function glBegin/glEnd paths on Android.
 * Two shader programs are used:
 *   1. Textured shader  — for cSurface_Request (sprites/images)
 *   2. Colored shader   — for rects, circles, lines, gradients
 ***************************************************************************/

#ifdef __ANDROID__

#include "gles2_renderer.h"
#include <GLES2/gl2.h>
#include <cstdio>
#include <cmath>
#include <cstring>  // memcpy

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

namespace GLES2 {

// ---------------------------------------------------------------------------
// Shader source strings
// ---------------------------------------------------------------------------

static const char* k_vert_textured =
    "attribute vec2 a_position;\n"
    "attribute vec2 a_texcoord;\n"
    "uniform mat4 u_projection;\n"
    "varying vec2 v_texcoord;\n"
    "void main() {\n"
    "    gl_Position = u_projection * vec4(a_position, 0.0, 1.0);\n"
    "    v_texcoord = a_texcoord;\n"
    "}\n";

static const char* k_frag_textured =
    "precision mediump float;\n"
    "uniform sampler2D u_texture;\n"
    "uniform vec4 u_color;\n"
    "varying vec2 v_texcoord;\n"
    "void main() {\n"
    "    gl_FragColor = texture2D(u_texture, v_texcoord) * u_color;\n"
    "}\n";

static const char* k_vert_colored =
    "attribute vec2 a_position;\n"
    "attribute vec4 a_color;\n"
    "uniform mat4 u_projection;\n"
    "varying vec4 v_color;\n"
    "void main() {\n"
    "    gl_Position = u_projection * vec4(a_position, 0.0, 1.0);\n"
    "    v_color = a_color;\n"
    "}\n";

static const char* k_frag_colored =
    "precision mediump float;\n"
    "varying vec4 v_color;\n"
    "void main() {\n"
    "    gl_FragColor = v_color;\n"
    "}\n";

// ---------------------------------------------------------------------------
// Internal state
// ---------------------------------------------------------------------------

// Textured program
static GLuint s_prog_tex      = 0;
static GLint  s_loc_tex_proj  = -1;
static GLint  s_loc_tex_color = -1;
static GLint  s_loc_tex_tex   = -1;
static GLint  s_attr_tex_pos  = -1;
static GLint  s_attr_tex_uv   = -1;

// Colored program
static GLuint s_prog_col      = 0;
static GLint  s_loc_col_proj  = -1;
static GLint  s_attr_col_pos  = -1;
static GLint  s_attr_col_col  = -1;

// Shared VBO (large enough for circle: center + 34 verts; textured uses 4)
// Each textured vertex: 2 floats pos + 2 floats uv  = 4 floats = 16 bytes
// Each colored  vertex: 2 floats pos + 4 floats col = 6 floats = 24 bytes
// Max verts needed: 34 (circle with 32 segments + center + closing)
static const int k_max_verts = 64;
static GLuint s_vbo = 0;

// Current orthographic projection matrix (column-major, 4x4)
static float s_proj[16];

// ---------------------------------------------------------------------------
// Helper: compile one shader stage
// ---------------------------------------------------------------------------
static GLuint compile_shader(GLenum type, const char* src)
{
    GLuint id = glCreateShader(type);
    if (!id) return 0;
    glShaderSource(id, 1, &src, nullptr);
    glCompileShader(id);

    GLint ok = 0;
    glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(id, sizeof(log), nullptr, log);
        printf("GLES2: shader compile error: %s\n", log);
        glDeleteShader(id);
        return 0;
    }
    return id;
}

// ---------------------------------------------------------------------------
// Helper: link two compiled stages into a program
// ---------------------------------------------------------------------------
static GLuint link_program(GLuint vert, GLuint frag)
{
    GLuint prog = glCreateProgram();
    if (!prog) return 0;
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);
    glLinkProgram(prog);

    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        printf("GLES2: program link error: %s\n", log);
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void Init(void)
{
    // --- Textured program ---
    GLuint v = compile_shader(GL_VERTEX_SHADER,   k_vert_textured);
    GLuint f = compile_shader(GL_FRAGMENT_SHADER, k_frag_textured);
    s_prog_tex = link_program(v, f);
    glDeleteShader(v);
    glDeleteShader(f);

    if (s_prog_tex) {
        s_loc_tex_proj  = glGetUniformLocation(s_prog_tex, "u_projection");
        s_loc_tex_color = glGetUniformLocation(s_prog_tex, "u_color");
        s_loc_tex_tex   = glGetUniformLocation(s_prog_tex, "u_texture");
        s_attr_tex_pos  = glGetAttribLocation (s_prog_tex, "a_position");
        s_attr_tex_uv   = glGetAttribLocation (s_prog_tex, "a_texcoord");
    }

    // --- Colored program ---
    v = compile_shader(GL_VERTEX_SHADER,   k_vert_colored);
    f = compile_shader(GL_FRAGMENT_SHADER, k_frag_colored);
    s_prog_col = link_program(v, f);
    glDeleteShader(v);
    glDeleteShader(f);

    if (s_prog_col) {
        s_loc_col_proj = glGetUniformLocation(s_prog_col, "u_projection");
        s_attr_col_pos = glGetAttribLocation (s_prog_col, "a_position");
        s_attr_col_col = glGetAttribLocation (s_prog_col, "a_color");
    }

    // --- Single shared VBO ---
    glGenBuffers(1, &s_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    // Pre-allocate: 64 verts * 6 floats * 4 bytes = 1536 bytes; covers all draw types.
    glBufferData(GL_ARRAY_BUFFER, k_max_verts * 6 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Identity projection until Set_Projection is called
    memset(s_proj, 0, sizeof(s_proj));
    s_proj[0] = s_proj[5] = s_proj[10] = s_proj[15] = 1.0f;

    printf("GLES2: renderer initialized (prog_tex=%u prog_col=%u vbo=%u)\n",
           s_prog_tex, s_prog_col, s_vbo);
}

void Shutdown(void)
{
    if (s_vbo)      { glDeleteBuffers(1, &s_vbo);     s_vbo = 0; }
    if (s_prog_tex) { glDeleteProgram(s_prog_tex);     s_prog_tex = 0; }
    if (s_prog_col) { glDeleteProgram(s_prog_col);     s_prog_col = 0; }
}

void Set_Projection(float w, float h)
{
    // Column-major orthographic matrix:
    // Maps x: [0..w] -> [-1..1],  y: [0..h] -> [1..-1]  (y-flip = y=0 is top)
    //
    //  [ 2/w    0    0   -1 ]
    //  [  0  -2/h   0    1 ]
    //  [  0    0   -1    0 ]
    //  [  0    0    0    1 ]
    //
    // OpenGL expects column-major storage, so element [row][col] is stored at index col*4+row.

    memset(s_proj, 0, sizeof(s_proj));
    s_proj[0]  =  2.0f / w;   // col0, row0
    s_proj[5]  = -2.0f / h;   // col1, row1
    s_proj[10] = -1.0f;       // col2, row2
    s_proj[12] = -1.0f;       // col3, row0  (tx = -1)
    s_proj[13] =  1.0f;       // col3, row1  (ty = +1)
    s_proj[15] =  1.0f;       // col3, row3

    printf("GLES2: Set_Projection %.0f x %.0f\n", w, h);
}

// ---------------------------------------------------------------------------
// Draw_Texture
// ---------------------------------------------------------------------------
void Draw_Texture(float x, float y, float w, float h,
                  GLuint tex,
                  float u0, float v0, float u1, float v1,
                  Uint8 r, Uint8 g, Uint8 b, Uint8 a,
                  float rot)
{
    if (!s_prog_tex || !s_vbo) return;

    // Build 4 vertices as a triangle fan: TL, TR, BR, BL
    // Layout per vertex: [px, py, u, v]  (4 floats)
    float vdata[4 * 4];

    float px[4] = { x,     x + w, x + w, x     };
    float py[4] = { y,     y,     y + h, y + h  };
    float pu[4] = { u0,    u1,    u1,    u0     };
    float pv[4] = { v0,    v0,    v1,    v1     };

    if (rot != 0.0f) {
        float cx = x + w * 0.5f;
        float cy = y + h * 0.5f;
        float rad = rot * (float)(M_PI / 180.0);
        float cosA = cosf(rad);
        float sinA = sinf(rad);
        for (int i = 0; i < 4; ++i) {
            float dx = px[i] - cx;
            float dy = py[i] - cy;
            px[i] = cx + dx * cosA - dy * sinA;
            py[i] = cy + dx * sinA + dy * cosA;
        }
    }

    for (int i = 0; i < 4; ++i) {
        vdata[i * 4 + 0] = px[i];
        vdata[i * 4 + 1] = py[i];
        vdata[i * 4 + 2] = pu[i];
        vdata[i * 4 + 3] = pv[i];
    }

    glUseProgram(s_prog_tex);
    glUniformMatrix4fv(s_loc_tex_proj, 1, GL_FALSE, s_proj);
    glUniform1i(s_loc_tex_tex, 0);
    glUniform4f(s_loc_tex_color,
                r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);

    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vdata), vdata);

    const GLsizei stride = 4 * sizeof(float);
    glEnableVertexAttribArray(s_attr_tex_pos);
    glEnableVertexAttribArray(s_attr_tex_uv);
    glVertexAttribPointer(s_attr_tex_pos, 2, GL_FLOAT, GL_FALSE, stride,
                          (const void*)(0));
    glVertexAttribPointer(s_attr_tex_uv,  2, GL_FLOAT, GL_FALSE, stride,
                          (const void*)(2 * sizeof(float)));

    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    glDisableVertexAttribArray(s_attr_tex_pos);
    glDisableVertexAttribArray(s_attr_tex_uv);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// ---------------------------------------------------------------------------
// Draw_Rect
// ---------------------------------------------------------------------------
void Draw_Rect(float x, float y, float w, float h,
               Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    if (!s_prog_col || !s_vbo) return;

    float fr = r / 255.0f;
    float fg = g / 255.0f;
    float fb = b / 255.0f;
    float fa = a / 255.0f;

    // Layout per vertex: [px, py, r, g, b, a]  (6 floats)
    float vdata[4 * 6] = {
        x,     y,     fr, fg, fb, fa,
        x + w, y,     fr, fg, fb, fa,
        x + w, y + h, fr, fg, fb, fa,
        x,     y + h, fr, fg, fb, fa,
    };

    glUseProgram(s_prog_col);
    glUniformMatrix4fv(s_loc_col_proj, 1, GL_FALSE, s_proj);

    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vdata), vdata);

    const GLsizei stride = 6 * sizeof(float);
    glEnableVertexAttribArray(s_attr_col_pos);
    glEnableVertexAttribArray(s_attr_col_col);
    glVertexAttribPointer(s_attr_col_pos, 2, GL_FLOAT, GL_FALSE, stride,
                          (const void*)(0));
    glVertexAttribPointer(s_attr_col_col, 4, GL_FLOAT, GL_FALSE, stride,
                          (const void*)(2 * sizeof(float)));

    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    glDisableVertexAttribArray(s_attr_col_pos);
    glDisableVertexAttribArray(s_attr_col_col);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// ---------------------------------------------------------------------------
// Draw_Circle  (filled, triangle fan, dynamic step size matching desktop)
// ---------------------------------------------------------------------------
void Draw_Circle(float cx, float cy, float radius,
                 Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    if (!s_prog_col || !s_vbo) return;

    // Replicate the desktop step-size logic
    float step_size = 1.0f / (radius * 0.05f);
    if (step_size > 0.2f)
        step_size = 0.2f;

    // Count how many perimeter verts we'll produce
    int n_peri = 0;
    {
        float angle = 0.0f;
        while (angle < (float)(M_PI * 2.0)) { n_peri++; angle += step_size; }
        n_peri++; // closing vert at angle == 2*pi
    }
    int n_verts = 1 + n_peri; // center + perimeter

    if (n_verts > k_max_verts) n_verts = k_max_verts;

    // Build VBO data: [px, py, r, g, b, a] per vertex
    // Use a stack buffer if small enough, else static fallback
    static float vdata[k_max_verts * 6];

    float fr = r / 255.0f;
    float fg = g / 255.0f;
    float fb = b / 255.0f;
    float fa = a / 255.0f;

    // Center vertex
    vdata[0] = cx;  vdata[1] = cy;
    vdata[2] = fr;  vdata[3] = fg;  vdata[4] = fb;  vdata[5] = fa;

    int vi = 1;
    float angle = 0.0f;
    while (angle < (float)(M_PI * 2.0) && vi < k_max_verts - 1) {
        float* v = vdata + vi * 6;
        v[0] = cx + radius * sinf(angle);
        v[1] = cy + radius * cosf(angle);
        v[2] = fr; v[3] = fg; v[4] = fb; v[5] = fa;
        vi++;
        angle += step_size;
    }
    // Closing vert
    if (vi < k_max_verts) {
        float* v = vdata + vi * 6;
        angle = (float)(M_PI * 2.0);
        v[0] = cx + radius * sinf(angle);
        v[1] = cy + radius * cosf(angle);
        v[2] = fr; v[3] = fg; v[4] = fb; v[5] = fa;
        vi++;
    }

    glUseProgram(s_prog_col);
    glUniformMatrix4fv(s_loc_col_proj, 1, GL_FALSE, s_proj);

    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vi * 6 * (int)sizeof(float), vdata);

    const GLsizei stride = 6 * sizeof(float);
    glEnableVertexAttribArray(s_attr_col_pos);
    glEnableVertexAttribArray(s_attr_col_col);
    glVertexAttribPointer(s_attr_col_pos, 2, GL_FLOAT, GL_FALSE, stride,
                          (const void*)(0));
    glVertexAttribPointer(s_attr_col_col, 4, GL_FLOAT, GL_FALSE, stride,
                          (const void*)(2 * sizeof(float)));

    glDrawArrays(GL_TRIANGLE_FAN, 0, vi);

    glDisableVertexAttribArray(s_attr_col_pos);
    glDisableVertexAttribArray(s_attr_col_col);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// ---------------------------------------------------------------------------
// Draw_Line
// ---------------------------------------------------------------------------
void Draw_Line(float x1, float y1, float x2, float y2,
               Uint8 r, Uint8 g, Uint8 b, Uint8 a, float /*width*/)
{
    if (!s_prog_col || !s_vbo) return;

    float fr = r / 255.0f;
    float fg = g / 255.0f;
    float fb = b / 255.0f;
    float fa = a / 255.0f;

    // Layout: [px, py, r, g, b, a]
    float vdata[2 * 6] = {
        x1, y1, fr, fg, fb, fa,
        x2, y2, fr, fg, fb, fa,
    };

    // Note: glLineWidth > 1 is not guaranteed on GLES2; width param is accepted
    // but silently clamped to 1.0 by most drivers. We ignore it here.

    glUseProgram(s_prog_col);
    glUniformMatrix4fv(s_loc_col_proj, 1, GL_FALSE, s_proj);

    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vdata), vdata);

    const GLsizei stride = 6 * sizeof(float);
    glEnableVertexAttribArray(s_attr_col_pos);
    glEnableVertexAttribArray(s_attr_col_col);
    glVertexAttribPointer(s_attr_col_pos, 2, GL_FLOAT, GL_FALSE, stride,
                          (const void*)(0));
    glVertexAttribPointer(s_attr_col_col, 4, GL_FLOAT, GL_FALSE, stride,
                          (const void*)(2 * sizeof(float)));

    glDrawArrays(GL_LINES, 0, 2);

    glDisableVertexAttribArray(s_attr_col_pos);
    glDisableVertexAttribArray(s_attr_col_col);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// ---------------------------------------------------------------------------
// Draw_Gradient_Vertical  (top = color1, bottom = color2)
// Matches desktop DIR_VERTICAL:
//   TL, TR  → color1
//   BR, BL  → color2
// Using GL_TRIANGLE_STRIP order: TL, TR, BL, BR
// ---------------------------------------------------------------------------
void Draw_Gradient_Vertical(float x, float y, float w, float h,
                             Uint8 r1, Uint8 g1, Uint8 b1, Uint8 a1,
                             Uint8 r2, Uint8 g2, Uint8 b2, Uint8 a2)
{
    if (!s_prog_col || !s_vbo) return;

    float fr1 = r1 / 255.0f, fg1 = g1 / 255.0f, fb1 = b1 / 255.0f, fa1 = a1 / 255.0f;
    float fr2 = r2 / 255.0f, fg2 = g2 / 255.0f, fb2 = b2 / 255.0f, fa2 = a2 / 255.0f;

    // GL_TRIANGLE_STRIP: TL, TR, BL, BR
    float vdata[4 * 6] = {
        x,     y,     fr1, fg1, fb1, fa1,  // TL  color1
        x + w, y,     fr1, fg1, fb1, fa1,  // TR  color1
        x,     y + h, fr2, fg2, fb2, fa2,  // BL  color2
        x + w, y + h, fr2, fg2, fb2, fa2,  // BR  color2
    };

    glUseProgram(s_prog_col);
    glUniformMatrix4fv(s_loc_col_proj, 1, GL_FALSE, s_proj);

    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vdata), vdata);

    const GLsizei stride = 6 * sizeof(float);
    glEnableVertexAttribArray(s_attr_col_pos);
    glEnableVertexAttribArray(s_attr_col_col);
    glVertexAttribPointer(s_attr_col_pos, 2, GL_FLOAT, GL_FALSE, stride,
                          (const void*)(0));
    glVertexAttribPointer(s_attr_col_col, 4, GL_FLOAT, GL_FALSE, stride,
                          (const void*)(2 * sizeof(float)));

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableVertexAttribArray(s_attr_col_pos);
    glDisableVertexAttribArray(s_attr_col_col);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// ---------------------------------------------------------------------------
// Draw_Gradient_Horizontal  (left = color1, right = color2)
// Matches desktop DIR_HORIZONTAL:
//   BL, TL  → color1
//   TR, BR  → color2
// Using GL_TRIANGLE_STRIP order: TL, TR, BL, BR with correct color mapping
// ---------------------------------------------------------------------------
void Draw_Gradient_Horizontal(float x, float y, float w, float h,
                               Uint8 r1, Uint8 g1, Uint8 b1, Uint8 a1,
                               Uint8 r2, Uint8 g2, Uint8 b2, Uint8 a2)
{
    if (!s_prog_col || !s_vbo) return;

    float fr1 = r1 / 255.0f, fg1 = g1 / 255.0f, fb1 = b1 / 255.0f, fa1 = a1 / 255.0f;
    float fr2 = r2 / 255.0f, fg2 = g2 / 255.0f, fb2 = b2 / 255.0f, fa2 = a2 / 255.0f;

    // GL_TRIANGLE_STRIP: TL, TR, BL, BR
    float vdata[4 * 6] = {
        x,     y,     fr1, fg1, fb1, fa1,  // TL  color1 (left)
        x + w, y,     fr2, fg2, fb2, fa2,  // TR  color2 (right)
        x,     y + h, fr1, fg1, fb1, fa1,  // BL  color1 (left)
        x + w, y + h, fr2, fg2, fb2, fa2,  // BR  color2 (right)
    };

    glUseProgram(s_prog_col);
    glUniformMatrix4fv(s_loc_col_proj, 1, GL_FALSE, s_proj);

    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vdata), vdata);

    const GLsizei stride = 6 * sizeof(float);
    glEnableVertexAttribArray(s_attr_col_pos);
    glEnableVertexAttribArray(s_attr_col_col);
    glVertexAttribPointer(s_attr_col_pos, 2, GL_FLOAT, GL_FALSE, stride,
                          (const void*)(0));
    glVertexAttribPointer(s_attr_col_col, 4, GL_FLOAT, GL_FALSE, stride,
                          (const void*)(2 * sizeof(float)));

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableVertexAttribArray(s_attr_col_pos);
    glDisableVertexAttribArray(s_attr_col_col);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

} // namespace GLES2

#endif // __ANDROID__
