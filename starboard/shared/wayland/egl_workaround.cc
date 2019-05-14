// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/renderer/backend/egl/display.h"
#include "starboard/common/log.h"
#include "starboard/shared/wayland/native_display_type.h"

extern "C" EGLDisplay __real_eglGetDisplay(EGLNativeDisplayType native_display);
extern "C" EGLDisplay __wrap_eglGetDisplay(EGLNativeDisplayType native_display);

extern "C" EGLDisplay __wrap_eglGetDisplay(
    EGLNativeDisplayType native_display) {
  SB_LOG(INFO) << " __wrap_eglGetDisplay ";
  return __real_eglGetDisplay((EGLNativeDisplayType)WaylandNativeDisplayType());
}
