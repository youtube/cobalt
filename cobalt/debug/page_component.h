/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef COBALT_DEBUG_PAGE_COMPONENT_H_
#define COBALT_DEBUG_PAGE_COMPONENT_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "cobalt/debug/component_connector.h"
#include "cobalt/debug/json_object.h"
#include "cobalt/debug/render_layer.h"
#include "cobalt/dom/window.h"
#include "cobalt/render_tree/resource_provider.h"
#include "cobalt/script/script_debugger.h"

namespace cobalt {
namespace debug {

class PageComponent {
 public:
  PageComponent(ComponentConnector* connector, dom::Window* window,
                scoped_ptr<RenderLayer> render_layer,
                render_tree::ResourceProvider* resource_provider);

 private:
  JSONObject Enable(const JSONObject& params);
  JSONObject Disable(const JSONObject& params);
  JSONObject GetResourceTree(const JSONObject& params);
  JSONObject SetOverlayMessage(const JSONObject& params);

  // Helper object to connect to the debug server, etc.
  ComponentConnector* connector_;
  // No ownership.
  dom::Window* window_;
  // Owned by this object.
  scoped_ptr<RenderLayer> render_layer_;
  // No ownership.
  render_tree::ResourceProvider* resource_provider_;
};

}  // namespace debug
}  // namespace cobalt

#endif  // COBALT_DEBUG_PAGE_COMPONENT_H_
