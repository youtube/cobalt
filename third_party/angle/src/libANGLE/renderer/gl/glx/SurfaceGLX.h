//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// SurfaceGLX.h: common interface for GLX surfaces

#ifndef LIBANGLE_RENDERER_GL_GLX_SURFACEGLX_H_
#define LIBANGLE_RENDERER_GL_GLX_SURFACEGLX_H_

#include "libANGLE/renderer/gl/SurfaceGL.h"

namespace rx
{

class FunctionsGLX;

class SurfaceGLX : public SurfaceGL
{
  public:
    SurfaceGLX(const egl::SurfaceState &state, RendererGL *renderer, const FunctionsGLX &glx)
        : SurfaceGL(state, renderer), mGLX(glx) {}

    virtual egl::Error checkForResize() = 0;

    // We must explicitly make no context current on GLX so that when thread A
    // is done using a context, thread B can make it current without an error.
    egl::Error unMakeCurrent() override;

  private:
   const FunctionsGLX& mGLX;
};
}

#endif  // LIBANGLE_RENDERER_GL_GLX_SURFACEGLX_H_
