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

// Similar to directfb/blitter_internal.h.

#ifndef STARBOARD_SHARED_BLITTERGLES_BLITTER_INTERNAL_H_
#define STARBOARD_SHARED_BLITTERGLES_BLITTER_INTERNAL_H_

#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include "starboard/blitter.h"
#include "starboard/common/recursive_mutex.h"
#include "starboard/shared/internal_only.h"

namespace starboard {
namespace shared {
namespace blittergles {

struct SbBlitterDeviceRegistry {
  // This implementation only supports a single default device, so we remember
  // it here.
  SbBlitterDevicePrivate* default_device;

  // The mutex is necessary since SbBlitterDeviceRegistry is a global structure
  // that must be accessed by any thread to create/destroy devices.
  starboard::Mutex mutex;
};

SbBlitterDeviceRegistry* GetBlitterDeviceRegistry();

// Helper function to change data between Blitter and GL formats.
void ChangeDataFormat(SbBlitterPixelDataFormat in_format,
                      SbBlitterPixelDataFormat out_format,
                      int pitch_in_bytes,
                      int height,
                      void* data);

extern const EGLint kContextAttributeList[];

extern const EGLint kConfigAttributeList[];

}  // namespace blittergles
}  // namespace shared
}  // namespace starboard

struct SbBlitterDevicePrivate {
  // Internally we store our EGLDisplay object inside of the SbBlitterDevice
  // object.
  EGLDisplay display;

  // Internally we store our EGLConfig object inside of the SbBlitterDevice
  // object.
  EGLConfig config;

  // Mutex to ensure thread-safety in all SbBlitterDevice-related function
  // calls.
  starboard::RecursiveMutex mutex;
};

struct SbBlitterRenderTargetPrivate {
  // If this SbBlitterRenderTargetPrivate object was created from a swap chain,
  // we store a reference to it. Otherwise, it's set to
  // kSbBlitterInvalidSwapChain.
  SbBlitterSwapChainPrivate* swap_chain;

  // If this SbBlitterRenderTargetPrivate object was created from a surface, we
  // store a reference to it. Otherwise, it's set to kSbBlitterInvalidSurface.
  SbBlitterSurfacePrivate* surface;

  int width;

  int height;

  // We will need to access the config and display of the device used to
  // create this render target, so we keep track of it.
  SbBlitterDevicePrivate* device;

  // Keep track of the current GL framebuffer.
  GLuint framebuffer_handle;

  // Sets framebuffer_handle and binds the texture from the surface field to it.
  // On failure, sets framebuffer_handle to 0 and returns false.
  bool SetFramebuffer();
};

struct SbBlitterSwapChainPrivate {
  SbBlitterRenderTargetPrivate render_target;

  EGLSurface surface;
};

struct SbBlitterPixelDataPrivate {
  // Keep track of the device that was used to create this SbBlitterPixelData
  // object.
  SbBlitterDevicePrivate* device;

  // The pitch of the pixel data, in bytes.
  int pitch_in_bytes;

  int width;

  int height;

  SbBlitterPixelDataFormat format;

  // Points to the pixels.
  void* data;
};

#endif  // STARBOARD_SHARED_BLITTERGLES_BLITTER_INTERNAL_H_
