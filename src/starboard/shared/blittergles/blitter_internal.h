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
#include "starboard/mutex.h"
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

// Helper class to allow one to create a RAII object that acquires the
// SbBlitterContext object upon construction and handles binding/unbinding of
// the egl_context field, as well as initializing fields that have deferred
// creation.
class ScopedCurrentContext {
 public:
  explicit ScopedCurrentContext(SbBlitterContext context);
  ~ScopedCurrentContext();

  // Returns true if an error occurred during intialization (indicating that
  // this object is invalid).
  bool InitializationError() const { return error_; }

 private:
  SbBlitterContext context_;
  bool error_;

  // Keeps track of whether this context was current on the calling thread.
  bool was_current_;
};

// Will call eglMakeCurrent() and glBindFramebuffer() for context's
// current_render_target. Returns true on success, false on failure.
bool MakeCurrent(SbBlitterContext context);

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
  starboard::Mutex mutex;
};

struct SbBlitterRenderTargetPrivate {
  // If this SbBlitterRenderTargetPrivate object was created from a swap chain,
  // we store a reference to it. Otherwise, it's set to NULL.
  SbBlitterSwapChainPrivate* swap_chain;

  int width;

  int height;

  // We will need to access the config and display of the device used to
  // create this render target, so we keep track of it.
  SbBlitterDevicePrivate* device;

  // Keep track of the current GL framebuffer.
  GLuint framebuffer_handle;
};

struct SbBlitterSwapChainPrivate {
  SbBlitterRenderTargetPrivate render_target;

  EGLSurface surface;
};

struct SbBlitterContextPrivate {
  // Store a reference to the current rendering target.
  SbBlitterRenderTargetPrivate* current_render_target;

  // Keep track of the device used to create this context.
  SbBlitterDevicePrivate* device;

  // If we don't have any information about the display window, this field will
  // be created with a best-guess EGLConfig.
  EGLContext egl_context;

  // GL framebuffers can use a dummy EGLSurface if there isn't a surface bound
  // already.
  EGLSurface dummy_surface;

  // Whether or not blending is enabled on this context.
  bool blending_enabled;

  // The current color, used to determine the color of fill rectangles and blit
  // call color modulation.
  SbBlitterColor current_color;

  // Whether or not blits should be modulated by the current color.
  bool modulate_blits_with_color;

  // The current scissor rectangle.
  SbBlitterRect scissor;

  // Whether or not this context has been set to current or not.
  bool is_current;
};

#endif  // STARBOARD_SHARED_BLITTERGLES_BLITTER_INTERNAL_H_
