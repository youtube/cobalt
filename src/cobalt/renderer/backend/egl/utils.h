/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef COBALT_RENDERER_BACKEND_EGL_UTILS_H_
#define COBALT_RENDERER_BACKEND_EGL_UTILS_H_

#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include "base/logging.h"

// Provide an easy method of calling EGL and GL function and verifying that they
// did not generate any errors.  CHECKs is used for EGL calls for release-build
// error checking.  DCHECKs are used for GL calls instead of CHECKs because
// GL calls are expected to occur much more frequently.

#define GL_CALL(x) \
    do {\
      x;\
      DCHECK_EQ(GL_NO_ERROR, glGetError());\
    } while (false)

#define EGL_CALL(x) \
    do {\
      x;\
      CHECK_EQ(EGL_SUCCESS, eglGetError());\
    } while (false)

namespace cobalt {
namespace renderer {
namespace backend {

// Uses EGL to create a GL ES 3 context.  While EGL specifies a standard way
// to do this, some platforms have finicky implementations and this function
// smooths that over.
EGLContext CreateGLES3Context(EGLDisplay display, EGLConfig config,
                              EGLContext share_context);

int BytesPerPixelForGLFormat(GLenum format);

}  // namespace backend
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_BACKEND_EGL_UTILS_H_
