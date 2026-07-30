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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_H5VCC_ACCESSIBILITY_TEXT_TO_SPEECH_CHANGE_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_H5VCC_ACCESSIBILITY_TEXT_TO_SPEECH_CHANGE_EVENT_H_

#include "third_party/blink/renderer/bindings/modules/v8/v8_text_to_speech_change_event_init.h"
#include "third_party/blink/renderer/modules/event_modules.h"

namespace blink {

class TextToSpeechChangeEvent : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  TextToSpeechChangeEvent(const AtomicString& type,
                          const TextToSpeechChangeEventInit* initializer);
  ~TextToSpeechChangeEvent() override;

  static TextToSpeechChangeEvent* Create(
      const AtomicString& type,
      const TextToSpeechChangeEventInit* initializer) {
    return MakeGarbageCollected<TextToSpeechChangeEvent>(type, initializer);
  }

  bool enabled() const { return enabled_; }

  const AtomicString& InterfaceName() const override;

  void Trace(Visitor*) const override;

 private:
  const bool enabled_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_H5VCC_ACCESSIBILITY_TEXT_TO_SPEECH_CHANGE_EVENT_H_
