// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/blittergles/blitter_internal.h"

#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include "starboard/common/log.h"
#include "starboard/once.h"
#include "starboard/shared/gles/gl_call.h"

namespace starboard {
namespace shared {
namespace blittergles {

namespace {
// Keep track of a global device registry that will be accessed by calls to
// create/destroy devices.
SbBlitterDeviceRegistry* s_device_registry = NULL;
SbOnceControl s_device_registry_once_control = SB_ONCE_INITIALIZER;

void EnsureDeviceRegistryInitialized() {
  s_device_registry = new SbBlitterDeviceRegistry();
  s_device_registry->default_device = NULL;
}

}  // namespace

const EGLint kContextAttributeList[] = {EGL_CONTEXT_CLIENT_VERSION, 2,
                                        EGL_NONE};

const EGLint kConfigAttributeList[] = {EGL_RED_SIZE,
                                       8,
                                       EGL_GREEN_SIZE,
                                       8,
                                       EGL_BLUE_SIZE,
                                       8,
                                       EGL_ALPHA_SIZE,
                                       8,
                                       EGL_BUFFER_SIZE,
                                       32,
                                       EGL_SURFACE_TYPE,
                                       EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
                                       EGL_COLOR_BUFFER_TYPE,
                                       EGL_RGB_BUFFER,
                                       EGL_CONFORMANT,
                                       EGL_OPENGL_ES2_BIT,
                                       EGL_RENDERABLE_TYPE,
                                       EGL_OPENGL_ES2_BIT,
                                       EGL_NONE};

SbBlitterDeviceRegistry* GetBlitterDeviceRegistry() {
  SbOnce(&s_device_registry_once_control, &EnsureDeviceRegistryInitialized);

  return s_device_registry;
}

}  // namespace blittergles
}  // namespace shared
}  // namespace starboard
