// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_RASPI_SHARED_OPEN_MAX_DECODE_TARGET_INTERNAL_H_
#define STARBOARD_RASPI_SHARED_OPEN_MAX_DECODE_TARGET_INTERNAL_H_

#include "starboard/decode_target.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>

struct SbDecodeTargetPrivate {
  EGLDisplay display;
  EGLImageKHR images[1];

  SbDecodeTargetInfo info;
};

#endif  // STARBOARD_RASPI_SHARED_OPEN_MAX_DECODE_TARGET_INTERNAL_H_
