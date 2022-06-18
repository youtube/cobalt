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

#include "cobalt/web/message_port.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/task_runner.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/web/context.h"
#include "cobalt/web/event.h"
#include "cobalt/web/event_target.h"
#include "cobalt/web/message_event.h"

namespace cobalt {
namespace web {

MessagePort::MessagePort(web::EventTarget* event_target)
    : event_target_(event_target) {}

MessagePort::~MessagePort() { event_target_ = nullptr; }

void MessagePort::PostMessage(const std::string& messages) {
  // TODO: Forward the location of the origating API call to the PostTask call.
  base::MessageLoop* message_loop =
      event_target_->environment_settings()->context()->message_loop();
  if (message_loop) {
    //   https://html.spec.whatwg.org/multipage/workers.html#handler-worker-onmessage
    // TODO: Update MessageEvent to support more types. (b/227665847)
    // TODO: Remove dependency of MessageEvent on net iobuffer (b/227665847)
    message_loop->task_runner()->PostTask(
        FROM_HERE,
        base::Bind(
            [](web::EventTarget* event_target, const std::string& messages) {
              scoped_refptr<net::IOBufferWithSize> buf = base::WrapRefCounted(
                  new net::IOBufferWithSize(messages.length()));
              memcpy(buf->data(), messages.data(), messages.length());
              if (event_target) {
                event_target->DispatchEvent(new web::MessageEvent(
                    base::Tokens::message(), web::MessageEvent::kText, buf));
              }
              LOG_IF(WARNING, !event_target)
                  << "MessagePort event not dispatched "
                     "because there is no EventTarget.";
            },
            base::Unretained(event_target_), messages));
  }
}

}  // namespace web
}  // namespace cobalt
