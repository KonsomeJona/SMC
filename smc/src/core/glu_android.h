#ifndef SMC_GLU_ANDROID_H
#define SMC_GLU_ANDROID_H

#ifdef __ANDROID__
// ---------------------------------------------------------------------------
// GLU is not available on Android / OpenGL ES 2.0.
// Provide minimal stub replacements for the GLU functions actually used in
// the SMC source tree.
//
// Usage: include this header where GLU functions are called on Android,
// guarded by  #ifdef __ANDROID__  /  #include "core/glu_android.h"
// ---------------------------------------------------------------------------

#include <GLES2/gl2.h>
#include <cstdio>

// gluErrorString — used in renderer.cpp and video.cpp to print GL errors.
// Returns a human-readable string for the given GL error code.
static inline const char* gluErrorString(GLenum error)
{
    switch (error)
    {
        case GL_NO_ERROR:          return "GL_NO_ERROR";
        case GL_INVALID_ENUM:      return "GL_INVALID_ENUM";
        case GL_INVALID_VALUE:     return "GL_INVALID_VALUE";
        case GL_INVALID_OPERATION: return "GL_INVALID_OPERATION";
        case GL_OUT_OF_MEMORY:     return "GL_OUT_OF_MEMORY";
        // ES 2.0 / 3.0 additions
        case GL_INVALID_FRAMEBUFFER_OPERATION:
                                   return "GL_INVALID_FRAMEBUFFER_OPERATION";
        default:                   return "Unknown GL error";
    }
}

// gluBuild2DMipmaps — used in video.cpp to generate mipmaps.
// On OpenGL ES 2.0 we call glGenerateMipmap() instead; the texture must
// already be bound to GL_TEXTURE_2D with the base-level image uploaded.
static inline int gluBuild2DMipmaps(
    GLenum  target,
    GLint   /*internalFormat*/,
    GLsizei width,
    GLsizei height,
    GLenum  format,
    GLenum  type,
    const void* data)
{
    // Upload base level
    glTexImage2D(target, 0, format, width, height, 0, format, type, data);
    // Let the driver generate the remaining mip levels
    glGenerateMipmap(target);
    return 0; // GLU returns 0 on success
}

// gluOrtho2D — sets up a 2D orthographic projection.
// On ES 2.0 there is no fixed-function matrix stack; callers that use this
// must switch to manual matrix computation via a uniform. This stub is a
// placeholder so the translation unit compiles; the actual matrix upload
// happens in the shader uniform path.
static inline void gluOrtho2D(double /*left*/, double /*right*/,
                               double /*bottom*/, double /*top*/)
{
    // No-op stub — callers must use shader uniforms on ES 2.0
}

#endif // __ANDROID__

#endif // SMC_GLU_ANDROID_H
