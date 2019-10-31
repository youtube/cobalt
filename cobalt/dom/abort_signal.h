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

#ifndef COBALT_DOM_ABORT_SIGNAL_H_
#define COBALT_DOM_ABORT_SIGNAL_H_

#include <vector>

#include "cobalt/base/tokens.h"
#include "cobalt/dom/event_target.h"
#include "cobalt/script/environment_settings.h"

namespace cobalt {
namespace dom {

// This represents the DOM AbortSignal object.
//    https://dom.spec.whatwg.org/#interface-AbortSignal
class AbortSignal : public EventTarget {
 public:
  explicit AbortSignal(script::EnvironmentSettings* settings)
      : EventTarget(settings) {}

  // Web API: AbortSignal
  bool aborted() const { return aborted_; }
  const EventListenerScriptValue* onabort() const {
    return GetAttributeEventListener(base::Tokens::abort());
  }
  void set_onabort(const EventListenerScriptValue& event_listener) {
    SetAttributeEventListener(base::Tokens::abort(), event_listener);
  }

  // https://dom.spec.whatwg.org/#abortsignal-follow
  void Follow(const scoped_refptr<AbortSignal>& parent_signal);

  // Run the abort algorithms then trigger the abort event.
  //   https://dom.spec.whatwg.org/#abortsignal-signal-abort
  void SignalAbort();

  DEFINE_WRAPPABLE_TYPE(AbortSignal);
  void TraceMembers(script::Tracer* tracer) override;

 private:
  // Keep track of the signals following this abort signal. These will be
  // traced to keep them alive and also signalled when this is signalled.
  std::vector<scoped_refptr<AbortSignal>> following_signals_;

  bool aborted_ = false;

  DISALLOW_COPY_AND_ASSIGN(AbortSignal);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_ABORT_SIGNAL_H_
