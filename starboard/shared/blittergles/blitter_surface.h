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

#ifndef STARBOARD_SHARED_BLITTERGLES_BLITTER_SURFACE_H_
#define STARBOARD_SHARED_BLITTERGLES_BLITTER_SURFACE_H_

#include <GLES2/gl2.h>

#include "starboard/blitter.h"

struct SbBlitterSurfacePrivate {
 public:
  ~SbBlitterSurfacePrivate();

  // Keep track of the device that created this surface.
  SbBlitterDevicePrivate* device;

  // Surfaces may have a render target depending on the way they were created.
  SbBlitterRenderTargetPrivate* render_target;

  // Store information about this surface.
  SbBlitterSurfaceInfo info;

  // Keep track of the current texture.
  GLuint color_texture_handle;

  // Surfaces may have data if they were created from pixel data, and have not
  // been converted into textures yet.
  void* data;

  // Ensures color_texture_handle is initialized, assuming that an EGLContext
  // and EGLSurface are currently bound.
  bool EnsureInitialized();
};

#endif  // STARBOARD_SHARED_BLITTERGLES_BLITTER_SURFACE_H_
