//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// SurfaceGLX.h: common interface for GLX surfaces

#ifndef LIBANGLE_RENDERER_GL_GLX_SURFACEGLX_H_
#define LIBANGLE_RENDERER_GL_GLX_SURFACEGLX_H_

#include "libANGLE/renderer/gl/SurfaceGL.h"
#include "libANGLE/renderer/gl/glx/platform_glx.h"

namespace rx
{

class FunctionsGLX;

class SurfaceGLX : public SurfaceGL
{
  public:
<<<<<<< HEAD
    SurfaceGLX(const egl::SurfaceState &state, RendererGL *renderer, const FunctionsGLX &glx)
        : SurfaceGL(state, renderer), mGLX(glx) {}

    virtual egl::Error checkForResize() = 0;

    // We must explicitly make no context current on GLX so that when thread A
    // is done using a context, thread B can make it current without an error.
    egl::Error unMakeCurrent() override;

  private:
   const FunctionsGLX& mGLX;
=======
    SurfaceGLX(const egl::SurfaceState &state) : SurfaceGL(state) {}

    virtual egl::Error checkForResize()       = 0;
    virtual glx::Drawable getDrawable() const = 0;
>>>>>>> 1ba4cc530e9156a73f50daff4affa367dedd5a8a
};
}  // namespace rx

#endif  // LIBANGLE_RENDERER_GL_GLX_SURFACEGLX_H_
