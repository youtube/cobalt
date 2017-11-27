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

#ifndef COBALT_DEBUG_CONSOLE_COMPONENT_H_
#define COBALT_DEBUG_CONSOLE_COMPONENT_H_

#include <string>

#include "cobalt/debug/component_connector.h"
#include "cobalt/debug/json_object.h"
#include "cobalt/dom/console.h"

namespace cobalt {
namespace debug {

class ConsoleComponent {
 public:
  ConsoleComponent(ComponentConnector* connector, dom::Console* console);

 private:
  class Listener : public dom::Console::Listener {
   public:
    Listener(dom::Console* console, ConsoleComponent* console_component);
    void OnMessage(const std::string& message,
                   dom::Console::Level level) override;

   private:
    ConsoleComponent* console_component_;
  };

  JSONObject Enable(const JSONObject& params);
  JSONObject Disable(const JSONObject& params);

  // Called by |console_listener_| when a new message is output.
  void OnMessageAdded(const std::string& text, dom::Console::Level level);

  ComponentConnector* connector_;
  Listener console_listener_;
};

}  // namespace debug
}  // namespace cobalt

#endif  // COBALT_DEBUG_CONSOLE_COMPONENT_H_
