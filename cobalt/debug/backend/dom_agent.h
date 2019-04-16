// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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
#ifndef COBALT_DEBUG_BACKEND_DOM_AGENT_H_
#define COBALT_DEBUG_BACKEND_DOM_AGENT_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "cobalt/debug/backend/command_map.h"
#include "cobalt/debug/backend/debug_dispatcher.h"
#include "cobalt/debug/backend/render_layer.h"
#include "cobalt/debug/command.h"
#include "cobalt/debug/json_object.h"
#include "cobalt/dom/dom_rect.h"

namespace cobalt {
namespace debug {
namespace backend {

class DOMAgent {
 public:
  DOMAgent(DebugDispatcher* dispatcher,
           std::unique_ptr<RenderLayer> render_layer);

  void Thaw(JSONObject agent_state);
  JSONObject Freeze();

 private:
  void Enable(const Command& command);
  void Disable(const Command& command);

  // Highlights a specified node according to highlight parameters.
  void HighlightNode(const Command& command);

  // Hides the node highlighting.
  void HideHighlight(const Command& command);

  // Renders a highlight to the overlay.
  void RenderHighlight(const scoped_refptr<dom::DOMRect>& bounding_rect,
                       const base::DictionaryValue* highlight_config_value);

  // Helper object to connect to the debug dispatcher, etc.
  DebugDispatcher* dispatcher_;

  // Render layer owned by this object.
  std::unique_ptr<RenderLayer> render_layer_;

  // Map of member functions implementing commands.
  CommandMap<DOMAgent> commands_;

  // Whether we successfully loaded the agent's JavaScript implementation.
  bool script_loaded_ = false;
};

}  // namespace backend
}  // namespace debug
}  // namespace cobalt

#endif  // COBALT_DEBUG_BACKEND_DOM_AGENT_H_
