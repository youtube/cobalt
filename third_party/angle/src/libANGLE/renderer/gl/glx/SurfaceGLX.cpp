//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// SurfaceGLX.cpp: common interface for GLX surfaces

#include "libANGLE/renderer/gl/glx/SurfaceGLX.h"
#include "libANGLE/renderer/gl/glx/FunctionsGLX.h"


namespace rx
{

egl::Error SurfaceGLX::unMakeCurrent()
{
    if (mGLX.makeCurrent(None, NULL) != True)
    {
        return egl::Error(EGL_BAD_DISPLAY);
    }
    return egl::Error(EGL_SUCCESS);
}

}  // namespace rx
