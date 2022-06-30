// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_WEB_MESSAGE_PORT_H_
#define COBALT_WEB_MESSAGE_PORT_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "cobalt/base/tokens.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/sequence.h"
#include "cobalt/script/value_handle.h"
#include "cobalt/script/wrappable.h"
#include "cobalt/web/event_target.h"
#include "cobalt/web/event_target_listener_info.h"
#include "cobalt/web/message_event.h"

namespace cobalt {
namespace web {

class MessagePort : public script::Wrappable,
                    public base::SupportsWeakPtr<MessagePort> {
 public:
  explicit MessagePort(web::EventTarget* event_target);
  ~MessagePort();
  MessagePort(const MessagePort&) = delete;
  MessagePort& operator=(const MessagePort&) = delete;

  // This may help for adding support of 'object'
  // void postMessage(any message, object transfer);
  // -> void PostMessage(const script::ValueHandleHolder& message,
  //                     script::Sequence<script::ValueHandle*> transfer) {}
  void PostMessage(const std::string& message);

  void Start() {}
  void Close() {}

  const web::EventTargetListenerInfo::EventListenerScriptValue* onmessage()
      const {
    return event_target_ ? event_target_->GetAttributeEventListener(
                               base::Tokens::message())
                         : nullptr;
  }
  void set_onmessage(
      const web::EventTargetListenerInfo::EventListenerScriptValue&
          event_listener) {
    if (event_target_)
      event_target_->SetAttributeEventListener(base::Tokens::message(),
                                               event_listener);
  }

  const web::EventTargetListenerInfo::EventListenerScriptValue* onmessageerror()
      const {
    return event_target_ ? event_target_->GetAttributeEventListener(
                               base::Tokens::messageerror())
                         : nullptr;
  }
  void set_onmessageerror(
      const web::EventTargetListenerInfo::EventListenerScriptValue&
          event_listener) {
    if (event_target_)
      event_target_->SetAttributeEventListener(base::Tokens::messageerror(),
                                               event_listener);
  }

  DEFINE_WRAPPABLE_TYPE(MessagePort);

 private:
  // The event target to dispatch events to.
  web::EventTarget* event_target_ = nullptr;
};

}  // namespace web
}  // namespace cobalt

#endif  // COBALT_WEB_MESSAGE_PORT_H_
