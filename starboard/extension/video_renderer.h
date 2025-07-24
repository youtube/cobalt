// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_EXTENSION_VIDEO_RENDERER_H_
#define STARBOARD_EXTENSION_VIDEO_RENDERER_H_

#include <stdint.h>

#include "cobalt/renderer/rasterizer/egl/textured_mesh_renderer.h"
#include "starboard/configuration.h"

#ifdef __cplusplus
extern "C" {
#endif

#define kCobaltExtensionVideoRendererName "dev.cobalt.extension.VideoRenderer"

typedef cobalt::renderer::rasterizer::egl::TexturedMeshRenderer::Image Image;

typedef struct CobaltExtensionVideoRendererApi {
  // Name should be the string |kCobaltExtensionColorTransformName|.
  // This helps to validate that the extension API is correct.
  const char* name;

  // This specifies the version of the API that is implemented.
  uint32_t version;

  // The fields below this point were added in version 1 or later.

  // This function can be used to replace default image rendering with a custom
  // one. Returns 'true' if the image was rendered.
  bool (*Render)(const Image& image,
                 uint32 vbo,
                 int num_vertices,
                 uint32 mode,
                 const glm::mat4& mvp);

} CobaltExtensionVideoRendererApi;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_EXTENSION_VIDEO_RENDERER_H_
