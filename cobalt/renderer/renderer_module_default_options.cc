// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/renderer/get_default_rasterizer_for_platform.h"
#include "cobalt/renderer/renderer_module.h"

#include "base/logging.h"

namespace cobalt {
namespace renderer {

void RendererModule::Options::SetPerPlatformDefaultOptions() {
#if SB_API_VERSION < 12
  // If there is no need to frequently flip the display buffer, then enable
  // support for an optimization where the scene is not re-rasterized each frame
  // if it has not changed from the last frame.
  submit_even_if_render_tree_is_unchanged =
      SB_MUST_FREQUENTLY_FLIP_DISPLAY_BUFFER;
#endif

  create_rasterizer_function =
      GetDefaultRasterizerForPlatform().create_rasterizer_callback;
}

}  // namespace renderer
}  // namespace cobalt
