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

#ifndef COBALT_DOM_EME_MEDIA_KEY_MESSAGE_EVENT_H_
#define COBALT_DOM_EME_MEDIA_KEY_MESSAGE_EVENT_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "cobalt/dom/array_buffer.h"
#include "cobalt/dom/eme/media_key_message_event_init.h"
#include "cobalt/dom/eme/media_key_message_type.h"
#include "cobalt/dom/event.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {
namespace eme {

// The MediaKeyMessageEvent object is used for the message event.
//   https://www.w3.org/TR/encrypted-media/#mediakeymessageevent
class MediaKeyMessageEvent : public Event {
 public:
  // Web IDL: MediaKeyMessageEvent
  //

  MediaKeyMessageEvent(const std::string& type,
                       const MediaKeyMessageEventInit& event_init_dict);

  MediaKeyMessageType message_type() const { return message_type_; }
  const scoped_refptr<ArrayBuffer>& message() const { return message_; }

  DEFINE_WRAPPABLE_TYPE(MediaKeyMessageEvent);
  void TraceMembers(script::Tracer* tracer) override;

 private:
  MediaKeyMessageType message_type_;
  scoped_refptr<ArrayBuffer> message_;
};

}  // namespace eme
}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_EME_MEDIA_KEY_MESSAGE_EVENT_H_
