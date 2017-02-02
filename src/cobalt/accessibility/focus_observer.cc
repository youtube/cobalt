/*
 * Copyright 2017 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/accessibility/focus_observer.h"

#include <string>

#include "cobalt/accessibility/text_alternative.h"

namespace cobalt {
namespace accessibility {
FocusObserver::FocusObserver(dom::Document* document,
                             scoped_ptr<TTSEngine> tts_engine)
    : document_(document), tts_engine_(tts_engine.Pass()) {
  document_->AddObserver(this);
}

FocusObserver::~FocusObserver() { document_->RemoveObserver(this); }

void FocusObserver::OnFocusChanged() {
  // TODO: Only utter text if text-to-speech is enabled.
  scoped_refptr<dom::Element> element = document_->active_element();
  if (element) {
    std::string text_alternative = ComputeTextAlternative(element);
    tts_engine_->SpeakNow(text_alternative);
  }
}
}  // namespace accessibility
}  // namespace cobalt
