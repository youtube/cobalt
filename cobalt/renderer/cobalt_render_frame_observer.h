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

#ifndef COBALT_RENDERER_COBALT_RENDER_FRAME_OBSERVER_H_
#define COBALT_RENDERER_COBALT_RENDER_FRAME_OBSERVER_H_

#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "starboard/extension/graphics.h"

namespace cobalt {

// Enables Cobalt-specific responses to notifications of changes to the frame.
class CobaltRenderFrameObserver : public content::RenderFrameObserver {
 public:
  explicit CobaltRenderFrameObserver(content::RenderFrame* render_frame);
  ~CobaltRenderFrameObserver() override;

  CobaltRenderFrameObserver(const CobaltRenderFrameObserver&) = delete;
  CobaltRenderFrameObserver& operator=(const CobaltRenderFrameObserver&) =
      delete;

  void DidMeaningfulLayout(
      blink::WebMeaningfulLayout meaningful_layout) override;

 private:
  // content::RenderFrameObserver impl.

  // Overridden so that the observer has the same lifetime as the RenderFrame.
  void OnDestruct() override;
};

}  // namespace cobalt

#endif  // COBALT_RENDERER_COBALT_RENDER_FRAME_OBSERVER_H_
