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

#ifndef STARBOARD_SHARED_GLES_GL_CALL_H_
#define STARBOARD_SHARED_GLES_GL_CALL_H_

#include "EGL/egl.h"
#include "GLES2/gl2.h"

#include "starboard/log.h"

#define GL_CALL(x)                          \
  do {                                      \
    x;                                      \
    SB_DCHECK(glGetError() == GL_NO_ERROR); \
  } while (false)

#define EGL_CALL(x)                          \
  do {                                       \
    x;                                       \
    SB_DCHECK(eglGetError() == EGL_SUCCESS); \
  } while (false)

#endif  // STARBOARD_SHARED_GLES_GL_CALL_H_
