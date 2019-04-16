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

#ifndef COBALT_DOM_EME_MEDIA_ENCRYPTED_EVENT_H_
#define COBALT_DOM_EME_MEDIA_ENCRYPTED_EVENT_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "cobalt/dom/eme/media_encrypted_event_init.h"
#include "cobalt/dom/event.h"
#include "cobalt/script/array_buffer.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {
namespace eme {

// The MediaEncryptedEvent object is used for the encrypted event.
//   https://www.w3.org/TR/encrypted-media/#mediaencryptedevent
class MediaEncryptedEvent : public Event {
 public:
  // Web API: MediaEncryptedEvent
  //

  explicit MediaEncryptedEvent(const std::string& type);
  MediaEncryptedEvent(const std::string& type,
                      const MediaEncryptedEventInit& event_init_dict);

  const std::string& init_data_type() const { return init_data_type_; }
  script::Handle<script::ArrayBuffer> init_data() const {
    if (!init_data_reference_) {
      return script::Handle<script::ArrayBuffer>();
    }
    return script::Handle<script::ArrayBuffer>(*init_data_reference_);
  }

  DEFINE_WRAPPABLE_TYPE(MediaEncryptedEvent);

 private:
  std::string init_data_type_;
  std::unique_ptr<script::ScriptValue<script::ArrayBuffer>::Reference>
      init_data_reference_;
};

}  // namespace eme
}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_EME_MEDIA_ENCRYPTED_EVENT_H_
