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

#ifndef COBALT_RENDERER_RASTERIZER_LIB_EXPORTED_GRAPHICS_H_
#define COBALT_RENDERER_RASTERIZER_LIB_EXPORTED_GRAPHICS_H_

#include "starboard/export.h"
#include "starboard/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*CbLibGraphicsContextCreatedCallback)(void* context);
typedef void (*CbLibGraphicsBeginRenderFrameCallback)(
    void* context, intptr_t render_tree_texture_handle);
typedef void (*CbLibGraphicsEndRenderFrameCallback)(void* context);

// Sets a callback which will be called from the rasterization thread once the
// graphics context has been created.
SB_EXPORT_PLATFORM void CbLibGraphicsSetContextCreatedCallback(
    void* context, CbLibGraphicsContextCreatedCallback callback);

// Sets a callback which will be called as often as the platform can swap
// buffers from the rasterization thread. |render_tree_texture_handle| which is
// provided to the callback corresponds to a GLint texture ID for the current
// RenderTree.
SB_EXPORT_PLATFORM void CbLibGraphicsSetBeginRenderFrameCallback(
    void* context, CbLibGraphicsBeginRenderFrameCallback callback);

// Sets a callback which will be called at the end of rendering, after swap
// buffers has been called.
SB_EXPORT_PLATFORM void CbLibGraphicsSetEndRenderFrameCallback(
    void* context, CbLibGraphicsEndRenderFrameCallback callback);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // COBALT_RENDERER_RASTERIZER_LIB_EXPORTED_GRAPHICS_H_
