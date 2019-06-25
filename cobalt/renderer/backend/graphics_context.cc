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
#if SB_API_VERSION < SB_EXTENSIONS_API_VERSION
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

}  // namespace backend
}  // namespace renderer
}  // namespace cobalt
