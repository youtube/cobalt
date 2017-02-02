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

#ifndef COBALT_ACCESSIBILITY_FOCUS_OBSERVER_H_
#define COBALT_ACCESSIBILITY_FOCUS_OBSERVER_H_

#include "base/memory/scoped_ptr.h"
#include "cobalt/accessibility/tts_engine.h"
#include "cobalt/dom/document.h"

namespace cobalt {
namespace accessibility {
// FocusObserver registers an observer on the document such that it can be
// notified whenever the document's focused element changes. Upon change of the
// focused element, FocusObserver will calculate the text contents of the newly
// focused element and send that string to a TTSEngine.
class FocusObserver : public dom::DocumentObserver {
 public:
  FocusObserver(dom::Document* document, scoped_ptr<TTSEngine> tts_engine);

 protected:
  ~FocusObserver();
  void OnLoad() OVERRIDE {}
  void OnMutation() OVERRIDE {}
  void OnFocusChanged() OVERRIDE;

 private:
  dom::Document* document_;
  scoped_ptr<TTSEngine> tts_engine_;

  friend class scoped_ptr<FocusObserver>;
  DISALLOW_COPY_AND_ASSIGN(FocusObserver);
};
}  // namespace accessibility
}  // namespace cobalt

#endif  // COBALT_ACCESSIBILITY_FOCUS_OBSERVER_H_
