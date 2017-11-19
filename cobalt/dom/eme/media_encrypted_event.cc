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

#include "cobalt/dom/eme/media_encrypted_event.h"

#include "cobalt/base/tokens.h"

namespace cobalt {
namespace dom {
namespace eme {

// See step 5 in https://www.w3.org/TR/encrypted-media/#initdata-encountered.
MediaEncryptedEvent::MediaEncryptedEvent(const std::string& type)
    : Event(base::Token(type), kNotBubbles, kNotCancelable) {}

// See step 5 in https://www.w3.org/TR/encrypted-media/#initdata-encountered.
MediaEncryptedEvent::MediaEncryptedEvent(
    const std::string& type, const MediaEncryptedEventInit& event_init_dict)
    : Event(base::Token(type), kNotBubbles, kNotCancelable),
      init_data_type_(event_init_dict.init_data_type()),
      init_data_(event_init_dict.init_data()) {}

void MediaEncryptedEvent::TraceMembers(script::Tracer* tracer) {
  Event::TraceMembers(tracer);

  tracer->Trace(init_data_);
}

}  // namespace eme
}  // namespace dom
}  // namespace cobalt
