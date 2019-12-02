// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/renderer/backend/graphics_context.h"

#include "base/logging.h"
#include "starboard/string.h"

namespace cobalt {
namespace renderer {
namespace backend {

GraphicsContext::GraphicsContext(GraphicsSystem* system)
    : system_(system) {
#if SB_API_VERSION < 11
  graphics_extension_ = nullptr;
#else
  graphics_extension_ = static_cast<const CobaltExtensionGraphicsApi*>(
      SbSystemGetExtension(kCobaltExtensionGraphicsName));
  if (graphics_extension_) {
    // Verify it's the extension needed.
    if (SbStringCompareAll(graphics_extension_->name,
                           kCobaltExtensionGraphicsName) != 0 ||
        graphics_extension_->version < 1) {
      LOG(WARNING) << "Not using supplied cobalt graphics extension: "
                   << "'" << graphics_extension_->name << "' ("
                   << graphics_extension_->version << ")";
      graphics_extension_ = nullptr;
    }
  }
#endif
}

float GraphicsContext::GetMaximumFrameIntervalInMilliseconds() {
  if (graphics_extension_) {
    return graphics_extension_->GetMaximumFrameIntervalInMilliseconds();
  }

  // Allow the rasterizer to delay rendering indefinitely if nothing has
  // changed.
  return -1.0f;
}

float GraphicsContext::GetMinimumFrameIntervalInMilliseconds() {
  if (graphics_extension_ && graphics_extension_->version >= 2) {
    return graphics_extension_->GetMinimumFrameIntervalInMilliseconds();
  }

  // Return negative value, if the GraphicsExtension is not implemented
  // or the GraphicsExtension version is below 2.
  return -1.0f;
}

bool GraphicsContext::IsMapToMeshEnabled() {
#if SB_API_VERSION >= SB_ALL_RENDERERS_REQUIRED_VERSION
#if defined(ENABLE_MAP_TO_MESH)
#error \
    "ENABLE_MAP_TO_MESH is deprecated after Starboard version 12, use \
the Cobalt graphics extension function IsMapToMeshEnabled() instead."
#endif  // defined(ENABLE_MAP_TO_MESH)
  if (graphics_extension_ && graphics_extension_->version >= 3) {
    return graphics_extension_->IsMapToMeshEnabled();
  }

  // Assume map to mesh is enabled, as it is for most platforms.
  return true;
#else  // SB_API_VERSION >= SB_ALL_RENDERERS_REQUIRED_VERSION
#if defined(ENABLE_MAP_TO_MESH)
  if (graphics_extension_ && graphics_extension_->version >= 3) {
    DLOG(ERROR)
        << "ENABLE_MAP_TO_MESH and "
           "CobaltExtensionGraphicsApi::IsMapToMeshEnabled() are both defined. "
           "Remove 'enable_map_to_mesh' from your \"gyp_configuration.gypi\" "
           "file in favor of using IsMapToMeshEnabled().";
  }
  return static_cast<bool>(ENABLE_MAP_TO_MESH);
#endif

  if (graphics_extension_ && graphics_extension_->version >= 3) {
    return graphics_extension_->IsMapToMeshEnabled();
  }

  return false;
#endif  // SB_API_VERSION >= SB_ALL_RENDERERS_REQUIRED_VERSION
}

}  // namespace backend
}  // namespace renderer
}  // namespace cobalt
