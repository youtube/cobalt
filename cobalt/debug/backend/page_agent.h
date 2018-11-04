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

#ifndef COBALT_DEBUG_BACKEND_PAGE_AGENT_H_
#define COBALT_DEBUG_BACKEND_PAGE_AGENT_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "cobalt/debug/backend/command_map.h"
#include "cobalt/debug/backend/debug_dispatcher.h"
#include "cobalt/debug/backend/render_layer.h"
#include "cobalt/debug/command.h"
#include "cobalt/debug/json_object.h"
#include "cobalt/dom/window.h"
#include "cobalt/render_tree/resource_provider.h"
#include "cobalt/script/script_debugger.h"

namespace cobalt {
namespace debug {
namespace backend {

class PageAgent {
 public:
  PageAgent(DebugDispatcher* dispatcher, dom::Window* window,
            scoped_ptr<RenderLayer> render_layer,
            render_tree::ResourceProvider* resource_provider);

 private:
  void Enable(const Command& command);
  void Disable(const Command& command);
  void GetResourceTree(const Command& command);
  void SetOverlayMessage(const Command& command);

  dom::Window* window_;
  scoped_ptr<RenderLayer> render_layer_;
  render_tree::ResourceProvider* resource_provider_;

  // Map of member functions implementing commands.
  CommandMap<PageAgent> commands_;
};

}  // namespace backend
}  // namespace debug
}  // namespace cobalt

#endif  // COBALT_DEBUG_BACKEND_PAGE_AGENT_H_
