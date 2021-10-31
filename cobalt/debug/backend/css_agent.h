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
#include "cobalt/debug/backend/agent_base.h"
#include "cobalt/debug/backend/debug_dispatcher.h"
#include "cobalt/dom/element.h"
#include "cobalt/script/sequence.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace debug {
namespace backend {

// https://chromedevtools.github.io/devtools-protocol/tot/CSS
class CSSAgent : public AgentBase, public script::Wrappable {
 public:
  typedef script::Sequence<scoped_refptr<cssom::CSSStyleRule>>
      CSSStyleRuleSequence;

  explicit CSSAgent(DebugDispatcher* dispatcher);

  // IDL: Returns a sequence of CSSStyleRules that match the element.
  CSSStyleRuleSequence GetMatchingCSSRules(
      const scoped_refptr<dom::Element>& element);

  DEFINE_WRAPPABLE_TYPE(CSSAgent);
};

}  // namespace backend
}  // namespace debug
}  // namespace cobalt

#endif  // COBALT_DEBUG_BACKEND_CSS_AGENT_H_
