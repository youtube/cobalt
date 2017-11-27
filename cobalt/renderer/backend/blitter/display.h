// Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef COBALT_RENDERER_BACKEND_BLITTER_DISPLAY_H_
#define COBALT_RENDERER_BACKEND_BLITTER_DISPLAY_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "cobalt/renderer/backend/display.h"
#include "cobalt/renderer/backend/graphics_system.h"
#include "starboard/blitter.h"

#if SB_HAS(BLITTER)

namespace cobalt {
namespace renderer {
namespace backend {

class RenderTarget;

class DisplayBlitter : public Display {
 public:
  DisplayBlitter(SbBlitterDevice device,
                 system_window::SystemWindow* system_window);

  scoped_refptr<RenderTarget> GetRenderTarget() override;

 private:
  scoped_refptr<RenderTarget> render_target_;
};

}  // namespace backend
}  // namespace renderer
}  // namespace cobalt

#endif  // #if SB_HAS(BLITTER)

#endif  // COBALT_RENDERER_BACKEND_BLITTER_DISPLAY_H_
