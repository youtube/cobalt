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

#include "cobalt/worker/message_port.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "cobalt/dom/event.h"
#include "cobalt/dom/event_target.h"
#include "cobalt/dom/message_event.h"
#include "cobalt/script/environment_settings.h"

namespace cobalt {
namespace worker {

MessagePort::MessagePort(dom::EventTarget* event_target,
                         script::EnvironmentSettings* settings)
    : event_target_(event_target),
      message_loop_(base::MessageLoop::current()),
      settings_(settings) {}

MessagePort::~MessagePort() {}

void MessagePort::PostMessage(const std::string& messages) {
  // TODO: Forward the location of the origating API call to the PostTask call.
  if (message_loop_) {
    //   https://html.spec.whatwg.org/multipage/workers.html#handler-worker-onmessage
    // TODO: Update MessageEvent to support more types. (b/227665847)
    // TODO: Remove dependency of MessageEvent on net iobuffer (b/227665847)
    scoped_refptr<net::IOBufferWithSize> buf =
        base::WrapRefCounted(new net::IOBufferWithSize(messages.length()));
    memcpy(buf->data(), messages.data(), messages.length());
    scoped_refptr<dom::MessageEvent> event(new dom::MessageEvent(
        base::Tokens::message(), dom::MessageEvent::kText, buf));
    message_loop_->task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&MessagePort::DispatchEvent, base::Unretained(this), event));
  }
}

void MessagePort::DispatchEvent(scoped_refptr<dom::MessageEvent> event) {
  if (event_target_) event_target_->DispatchEvent(event);
  LOG_IF(WARNING, !event_target_)
      << "MessagePort event not dispatched because there is no EventTarget.";
}

}  // namespace worker
}  // namespace cobalt
