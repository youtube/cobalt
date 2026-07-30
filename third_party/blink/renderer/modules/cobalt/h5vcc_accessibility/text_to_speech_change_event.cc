// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "third_party/blink/renderer/modules/cobalt/h5vcc_accessibility/text_to_speech_change_event.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"

namespace blink {

TextToSpeechChangeEvent::TextToSpeechChangeEvent(
    const AtomicString& type,
    const TextToSpeechChangeEventInit* initializer)
    : Event(type, initializer), enabled_(initializer->enabled()) {}

TextToSpeechChangeEvent::~TextToSpeechChangeEvent() = default;

const AtomicString& TextToSpeechChangeEvent::InterfaceName() const {
  return event_interface_names::kTextToSpeechChangeEvent;
}

void TextToSpeechChangeEvent::Trace(Visitor* visitor) const {
  Event::Trace(visitor);
}

}  // namespace blink
