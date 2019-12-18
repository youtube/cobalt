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

#include "cobalt/debug/backend/command_map.h"
#include "cobalt/debug/backend/debug_dispatcher.h"
#include "cobalt/debug/backend/render_layer.h"
#include "cobalt/debug/command.h"
#include "cobalt/debug/json_object.h"

namespace cobalt {
namespace debug {
namespace backend {

class OverlayAgent {
 public:
  OverlayAgent(DebugDispatcher* dispatcher,
               std::unique_ptr<RenderLayer> render_layer);

  void Thaw(JSONObject agent_state);
  JSONObject Freeze();

 private:
  void Enable(Command command);
  void Disable(Command command);

  void HighlightNode(Command command);
  void HighlightRect(Command command);
  void HideHighlight(Command command);

  DebugDispatcher* dispatcher_;

  // Render layer owned by this object.
  std::unique_ptr<RenderLayer> render_layer_;

  // Map of member functions implementing commands.
  CommandMap<OverlayAgent> commands_;

  // Whether we successfully loaded the agent's JavaScript implementation.
  bool script_loaded_ = false;

  bool enabled_ = false;
};

}  // namespace backend
}  // namespace debug
}  // namespace cobalt

#endif  // COBALT_DEBUG_BACKEND_OVERLAY_AGENT_H_
