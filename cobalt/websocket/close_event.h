// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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
#ifndef COBALT_WEBSOCKET_CLOSE_EVENT_H_
#define COBALT_WEBSOCKET_CLOSE_EVENT_H_

#include <string>

#include "cobalt/base/tokens.h"
#include "cobalt/dom/event.h"
#include "cobalt/websocket/close_event_init.h"
#include "net/websockets/websocket_errors.h"

namespace cobalt {
namespace websocket {

class CloseEvent : public dom::Event {
 public:
  explicit CloseEvent(const base::Token type)
      : Event(type), was_clean_(true), code_(net::kWebSocketNormalClosure) {}
  explicit CloseEvent(const std::string& type)
      : Event(type), was_clean_(true), code_(net::kWebSocketNormalClosure) {}
  CloseEvent(const base::Token type, const CloseEventInit& init_dict)
      : Event(type, init_dict),
        was_clean_(true),
        code_(net::kWebSocketNormalClosure) {
    InitializeFromCloseEventInit(init_dict);
  }
  CloseEvent(const std::string& type, const CloseEventInit& init_dict)
      : Event(type, init_dict),
        was_clean_(true),
        code_(net::kWebSocketNormalClosure) {
    InitializeFromCloseEventInit(init_dict);
  }

  void InitCloseEvent(const std::string& type, bool can_bubble, bool cancelable,
                      bool was_clean, uint16 code, const std::string& reason) {
    InitEvent(type, can_bubble, cancelable);
    was_clean_ = was_clean;
    code_ = code;
    reason_ = reason;
  }

  // Readonly Attributes.
  bool was_clean() const { return was_clean_; }
  uint16 code() const { return code_; }
  std::string reason() const { return reason_; }

  DEFINE_WRAPPABLE_TYPE(CloseEvent)

 private:
  void InitializeFromCloseEventInit(const CloseEventInit& init_dict) {
    if (init_dict.has_was_clean()) {
      was_clean_ = init_dict.was_clean();
    }
    if (init_dict.has_code()) {
      code_ = init_dict.code();
    }
    if (init_dict.has_reason()) {
      reason_ = init_dict.reason();
    }
  }
  bool was_clean_;
  uint16 code_;
  std::string reason_;

  DISALLOW_COPY_AND_ASSIGN(CloseEvent);
};

}  // namespace websocket
}  // namespace cobalt

#endif  // COBALT_WEBSOCKET_CLOSE_EVENT_H_
