// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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
#ifndef COBALT_DEBUG_BACKEND_OVERLAY_AGENT_H_
#define COBALT_DEBUG_BACKEND_OVERLAY_AGENT_H_

#include "cobalt/debug/backend/agent_base.h"
#include "cobalt/debug/backend/debug_dispatcher.h"
#include "cobalt/debug/backend/render_layer.h"
#include "cobalt/debug/command.h"

namespace cobalt {
namespace debug {
namespace backend {

// https://chromedevtools.github.io/devtools-protocol/tot/Overlay
class OverlayAgent : public AgentBase {
 public:
  OverlayAgent(DebugDispatcher* dispatcher,
               std::unique_ptr<RenderLayer> render_layer);

 private:
  void HighlightNode(Command command);
  void HighlightRect(Command command);
  void HideHighlight(Command command);
  void SetShowViewportSizeOnResize(Command command);

  // Render layer owned by this object.
  std::unique_ptr<RenderLayer> render_layer_;
};

}  // namespace backend
}  // namespace debug
}  // namespace cobalt

#endif  // COBALT_DEBUG_BACKEND_OVERLAY_AGENT_H_
