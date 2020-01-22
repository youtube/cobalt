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
#ifndef COBALT_DEBUG_BACKEND_CSS_AGENT_H_
#define COBALT_DEBUG_BACKEND_CSS_AGENT_H_

#include "cobalt/cssom/css_style_rule.h"
#include "cobalt/debug/backend/command_map.h"
#include "cobalt/debug/backend/debug_dispatcher.h"
#include "cobalt/debug/command.h"
#include "cobalt/dom/element.h"
#include "cobalt/script/sequence.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace debug {
namespace backend {

class CSSAgent : public script::Wrappable {
 public:
  typedef script::Sequence<scoped_refptr<cssom::CSSStyleRule>>
      CSSStyleRuleSequence;

  explicit CSSAgent(DebugDispatcher* dispatcher);

  void Thaw(JSONObject agent_state);
  JSONObject Freeze();

  // IDL: Returns a sequence of CSSStyleRules that match the element.
  CSSStyleRuleSequence GetMatchingCSSRules(
      const scoped_refptr<dom::Element>& element);

  DEFINE_WRAPPABLE_TYPE(CSSAgent);

 private:
  void Enable(Command command);
  void Disable(Command command);

  // Helper object to connect to the debug dispatcher, etc.
  DebugDispatcher* dispatcher_;

  // Map of member functions implementing commands.
  CommandMap commands_;

  // Whether we successfully loaded the agent's JavaScript implementation.
  bool script_loaded_ = false;
};

}  // namespace backend
}  // namespace debug
}  // namespace cobalt

#endif  // COBALT_DEBUG_BACKEND_CSS_AGENT_H_
