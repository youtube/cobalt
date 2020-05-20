// Copyright 2015 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "starboard/configuration.h"
#if SB_API_VERSION >= 12 || SB_HAS(GLES2)

#include "cobalt/renderer/backend/egl/utils.h"

#include "cobalt/renderer/egl_and_gles.h"

namespace cobalt {
namespace renderer {
namespace backend {

EGLContext CreateGLES3Context(EGLDisplay display, EGLConfig config,
                              EGLContext share_context) {
  // Create an OpenGL ES 3.0 context.
  EGLint context_attrib_list[] = {
      EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE,
  };
  EGLContext context = EGL_CALL_SIMPLE(
      eglCreateContext(display, config, share_context, context_attrib_list));

  if (EGL_CALL_SIMPLE(eglGetError()) != EGL_SUCCESS) {
    // Some drivers, such as Mesa 3D on Linux, do support GL ES 3, and report
    // it as the version when queried (via glGetString(GL_VERSION)), however
    // for some reason they do not initialize correctly unless 2 is provided
    // as EGL_CONTEXT_CLIENT_VERSION.  Thus, this fallback code exists to
    // facilitate those platforms.
    EGLint context_attrib_list_gles2[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE,
    };
    context = EGL_CALL_SIMPLE(eglCreateContext(display, config, share_context,
                                               context_attrib_list_gles2));
    CHECK_EQ(EGL_SUCCESS, EGL_CALL_SIMPLE(eglGetError()));
  }

  return context;
}

int BytesPerPixelForGLFormat(GLenum format) {
  switch (format) {
    case GL_RGBA:
    case GL_BGRA_EXT:
      return 4;
    case GL_LUMINANCE_ALPHA:
      return 2;
    case GL_ALPHA:
      return 1;
    default:
      NOTREACHED() << "Unknown GL format.";
  }
  return 0;
}

}  // namespace backend
}  // namespace renderer
}  // namespace cobalt

#endif  // SB_API_VERSION >= 12 || SB_HAS(GLES2)
