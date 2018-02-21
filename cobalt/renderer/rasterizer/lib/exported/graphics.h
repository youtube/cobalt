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

// All imported functions defined below MUST be implemented by client
// applications.
// All of callbacks in here will happen on Cobalt's rasterization thread
// (i.e. the thread which owns the GL context). To guarantee no callbacks
// are missed, all callbacks should be set before the rasterization thread
// has started. Any other functions in here (e.g.
// CbLibGrapicsGetMainTextureHandle) are also not thread-safe and should only be
// invoked on the rasterization thread during one of the provided callbacks.

#ifndef COBALT_RENDERER_RASTERIZER_LIB_EXPORTED_GRAPHICS_H_
#define COBALT_RENDERER_RASTERIZER_LIB_EXPORTED_GRAPHICS_H_

#include "starboard/export.h"
#include "starboard/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CbLibRenderContext {
  CbLibRenderContext() {}
  // An EGLSurface that will be mirrored to the system window, if provided.
  // uintptr_t is used for the type, to avoid pulling in possibly conflicting
  // EGL headers.
  uintptr_t surface_to_mirror = 0;
  // Horizontal amount of the surface to mirror, in the range [0.0, 1.0].
  float width_to_mirror = 1.0f;
} CbLibRenderContext;

typedef struct CbLibSize {
  CbLibSize(int width, int height) : width(width), height(height) {}
  int width;
  int height;
} CbLibSize;

typedef void (*CbLibGraphicsContextCreatedCallback)(void* context);
typedef void (*CbLibGraphicsRenderFrameCallback)(void* context);

// Sets a callback which will be called from the rasterization thread once the
// graphics context has been created.
SB_EXPORT_PLATFORM void CbLibGraphicsSetContextCreatedCallback(
    void* context, CbLibGraphicsContextCreatedCallback callback);

// Sets a callback which will be called as often as the platform can swap
// buffers from the rasterization thread.
//
// All rendering must be performed inside of this callback.
SB_EXPORT_PLATFORM void CbLibGraphicsSetRenderFrameCallback(
    void* context, CbLibGraphicsRenderFrameCallback callback);

// Returns the texture ID for the current RenderTree. This should be
// re-retrieved each frame in the event that the underlying texture has
// changed. This method will return 0 if there is not yet a valid texture ID.
// This will never return anything other than 0 until after the first frame is
// started / the CbLibGraphicsBeginRenderFrameCallback is invoked (assuming the
// callback is set). This is not thread-safe and should be invoked on the
// rasterization thread only.
SB_EXPORT_PLATFORM intptr_t CbLibGrapicsGetMainTextureHandle();

// Sets the target main texture buffer size (in pixels) to use when rendering
// out HTML content. The specified size is a target and is not guaranteed. In
// general, the updated size will be picked up and used at the beginning of the
// next frame.
SB_EXPORT_PLATFORM void CbLibGraphicsSetTargetMainTextureSize(
    const CbLibSize& target_render_size);

// Performs all Cobalt-related rendering, including the browser UI and the
// video if one is playing.
//
// This should only be called from the Cobalt rendering thread, inside of a
// callback set by CbLibGraphicsSetRenderFrameCallback.
SB_EXPORT_PLATFORM void CbLibGraphicsRenderCobalt();

// Copies the contents of the offscreen backbuffer to the Cobalt system window.
// Note that this call only copies the contents of the buffer but does not
// perform any presentation.  You must also call CbLibGraphicsSwapBackbuffer
// in order to make the contents of the offscreen backbuffer visible!
//
// This should only be called from the Cobalt rendering thread, inside of a
// callback set by CbLibGraphicsSetRenderFrameCallback.
SB_EXPORT_PLATFORM void CbLibGraphicsCopyBackbuffer(uintptr_t surface,
                                                    float width_scale);

// Swaps the backbuffer for the Cobalt system window, making a new frame
// visibile.
//
// This should only be called from the Cobalt rendering thread, inside of a
// callback set by CbLibGraphicsSetRenderFrameCallback.
SB_EXPORT_PLATFORM void CbLibGraphicsSwapBackbuffer();

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // COBALT_RENDERER_RASTERIZER_LIB_EXPORTED_GRAPHICS_H_
