// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "cobalt/dom/eme/media_key_message_event.h"

#include "cobalt/base/tokens.h"
#include "cobalt/dom/eme/media_key_message_type.h"

namespace cobalt {
namespace dom {
namespace eme {

// See step 2 in https://www.w3.org/TR/encrypted-media/#queue-message.
MediaKeyMessageEvent::MediaKeyMessageEvent(
    const std::string& type, const MediaKeyMessageEventInit& event_init_dict)
    : Event(base::Token(type), kNotBubbles, kNotCancelable),
      message_type_(event_init_dict.message_type()),
      message_(event_init_dict.message()) {}

void MediaKeyMessageEvent::TraceMembers(script::Tracer* tracer) {
  Event::TraceMembers(tracer);

  tracer->Trace(message_);
}

}  // namespace eme
}  // namespace dom
}  // namespace cobalt
