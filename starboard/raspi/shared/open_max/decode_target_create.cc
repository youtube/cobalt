// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "starboard/decode_target.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "starboard/log.h"
#include "starboard/raspi/shared/open_max/decode_target_internal.h"

SbDecodeTarget SbDecodeTargetCreate(EGLDisplay display,
                                    EGLContext context,
                                    SbDecodeTargetFormat format,
                                    int width,
                                    int height,
                                    GLuint* planes) {
  if (format == kSbDecodeTargetFormat1PlaneRGBA) {
    SbDecodeTargetPrivate* target = new SbDecodeTargetPrivate;
    target->display = display;
    target->format = format;
    target->width = width;
    target->height = height;
    target->num_planes = 1;
    target->planes[0] = planes[0];
    target->images[0] =
        eglCreateImageKHR(display, context, EGL_GL_TEXTURE_2D_KHR,
                          (EGLClientBuffer)target->planes[0], NULL);
    SB_DCHECK(eglGetError() == EGL_SUCCESS);
    SB_DCHECK(target->images[0] != EGL_NO_IMAGE_KHR);

    // Data to be set by the decoder.
    target->is_opaque = false;

    return target;
  } else {
    SB_NOTREACHED() << "Unsupported format " << format;
    return kSbDecodeTargetInvalid;
  }
}
