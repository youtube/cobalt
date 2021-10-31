// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

// Similar to window_internal.h.

#ifndef STARBOARD_SHARED_DIRECTFB_BLITTER_INTERNAL_H_
#define STARBOARD_SHARED_DIRECTFB_BLITTER_INTERNAL_H_

#include <directfb.h>

#include "starboard/blitter.h"
#include "starboard/common/mutex.h"
#include "starboard/shared/internal_only.h"

struct SbBlitterDevicePrivate {
  // Internally we store our DirectFB interface object inside of the
  // SbBlitterDevice object.
  IDirectFB* dfb;

  // Mutex to ensure thread-safety in all SbBlitterDevice-related function
  // calls.
  starboard::Mutex mutex;
};

struct SbBlitterRenderTargetPrivate {
  // Every render target has a reference to a DirectFB surface.
  IDirectFBSurface* surface;
};

struct SbBlitterSwapChainPrivate {
  // In DirectFB, swap chains are synonymous with [specifically flagged]
  // surfaces, so we just need to keep a handle to it (through a
  // SbBlitterRenderTarget object).
  SbBlitterRenderTargetPrivate render_target;
};

struct SbBlitterPixelDataPrivate {
  // Keep track of the device that was used to create this
  // SbBlitterPixelData object.
  SbBlitterDevicePrivate* device;

  // The actual DirectFB surface that contains the pixels.
  IDirectFBSurface* surface;
  // The CPU-accessible data pointer to the (locked) surface pixels.
  void* data;

  // The pitch of the pixel data, in bytes.
  int pitch_in_bytes;

  // Surface information including its dimensions.
  SbBlitterSurfaceInfo info;
};

struct SbBlitterSurfacePrivate {
  // Keep track of the device used to create this surface.
  SbBlitterDevicePrivate* device;

  // This is where our internal reference to a DirectFB surface lives.
  IDirectFBSurface* surface;

  // Surface information including its dimensions.
  SbBlitterSurfaceInfo info;

  // Surfaces may own a render target object that they can setup as context
  // render targets.
  SbBlitterRenderTargetPrivate render_target;
};

struct SbBlitterContextPrivate {
  // Keep track of the device used to create this context.
  SbBlitterDevicePrivate* device;

  // Keep track of the last set render target on this context.
  SbBlitterRenderTargetPrivate* current_render_target;

  // Keep track of whether or not blending is enabled on this context.
  bool blending_enabled;

  // The current color that will be used in draw calls such as to determine
  // the color of fill rectangles and to modulate the color of blit calls.
  SbBlitterColor current_color;

  // Keep track of whether blits should be modulated by the current color.
  bool modulate_blits_with_color;

  // Track the current scissor rectangle.
  SbBlitterRect scissor;
};

struct SbBlitterDeviceRegistry {
  // This implementation only supports a single, default, device, so we remember
  // it here.
  SbBlitterDevicePrivate* default_device;

  // The mutex is necessary since SbBlitterDeviceRegistry is a global structure
  // that must be accessed by any thread to create/destroy devices.
  starboard::Mutex mutex;
};

SbBlitterDeviceRegistry* GetBlitterDeviceRegistry();

// DirectFB stores context state information within its surfaces.  The surface's
// state is active whenever it is set as a render target.  This method sets
// up a surface's state based on the current context state.
bool SetupDFBSurfaceBlitStateFromBlitterContextState(SbBlitterContext context,
                                                     SbBlitterSurface source,
                                                     IDirectFBSurface* surface);

bool SetupDFBSurfaceDrawStateFromBlitterContextState(SbBlitterContext context,
                                                     IDirectFBSurface* surface);

#endif  // STARBOARD_SHARED_DIRECTFB_BLITTER_INTERNAL_H_
