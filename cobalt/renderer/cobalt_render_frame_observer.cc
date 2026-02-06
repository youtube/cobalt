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

#include "cobalt/renderer/cobalt_render_frame_observer.h"

#include "base/command_line.h"
#include "cobalt/browser/switches.h"
#include "content/public/renderer/render_frame.h"
#include "starboard/extension/graphics.h"
#include "starboard/system.h"

namespace cobalt {

CobaltRenderFrameObserver::CobaltRenderFrameObserver(
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame) {}

CobaltRenderFrameObserver::~CobaltRenderFrameObserver() = default;

void CobaltRenderFrameObserver::OnDestruct() {
  delete this;
}

void CobaltRenderFrameObserver::DidMeaningfulLayout(
    blink::WebMeaningfulLayout meaningful_layout) {
  if (meaningful_layout == blink::WebMeaningfulLayout::kVisuallyNonEmpty) {
    const CobaltExtensionGraphicsApi* graphics_extension =
        static_cast<const CobaltExtensionGraphicsApi*>(
            SbSystemGetExtension(kCobaltExtensionGraphicsName));
    if (graphics_extension &&
        strcmp(graphics_extension->name, kCobaltExtensionGraphicsName) == 0 &&
        graphics_extension->version >= 1) {
      graphics_extension->ReportFullyDrawn();
    }
  }
}

}  // namespace cobalt
