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

#include "base/memory/weak_ptr.h"
#include "cobalt/debug/debug_server.h"
#include "cobalt/debug/json_object.h"
#include "cobalt/dom/document.h"

namespace cobalt {
namespace debug {

class PageComponent : public DebugServer::Component {
 public:
  PageComponent(const base::WeakPtr<DebugServer>& server,
                dom::Document* document);

 private:
  JSONObject Enable(const JSONObject& params);
  JSONObject Disable(const JSONObject& params);
  JSONObject GetResourceTree(const JSONObject& params);

  // No ownership.
  dom::Document* document_;
};

}  // namespace debug
}  // namespace cobalt

#endif  // COBALT_DEBUG_PAGE_COMPONENT_H_
