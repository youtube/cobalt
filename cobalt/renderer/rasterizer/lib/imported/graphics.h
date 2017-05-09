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

#ifndef COBALT_RENDERER_RASTERIZER_LIB_IMPORTED_GRAPHICS_H_
#define COBALT_RENDERER_RASTERIZER_LIB_IMPORTED_GRAPHICS_H_

#include "starboard/export.h"
#include "starboard/types.h"


#ifdef __cplusplus
extern "C" {
#endif

// Invoked from the rasterization thread after the GL context has been created.
SB_IMPORT_PLATFORM void CbLibOnGraphicsContextCreated();

// Invoked as often as the platform can swap buffers from the rasterization
// thread. |render_tree_texture_handle| corresponds to a GLint texture ID for
// the current RenderTree.
SB_IMPORT_PLATFORM void CbLibRenderFrame(intptr_t render_tree_texture_handle);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // COBALT_RENDERER_RASTERIZER_LIB_IMPORTED_GRAPHICS_H_
