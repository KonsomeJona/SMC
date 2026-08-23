// Shim for <SDL2/SDL_opengl.h>: there is no desktop GL on Android, the
// renderer targets OpenGL ES 2.0. core/glu_android.h supplies the handful of
// GLU helpers the tree still calls.
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
