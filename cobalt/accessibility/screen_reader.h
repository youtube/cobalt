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

#ifndef COBALT_ACCESSIBILITY_SCREEN_READER_H_
#define COBALT_ACCESSIBILITY_SCREEN_READER_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "cobalt/accessibility/tts_engine.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/mutation_observer.h"
#include "cobalt/dom/mutation_observer_task_manager.h"

namespace cobalt {
namespace accessibility {
// ScreenReader tracks changes in the state of the Document and provides speech
// utterances to a TTSEngine. The main use cases covered are when the document's
// focused element changes, usually in response to user input, or if there is
// an asynchronous change to "live region" that should be communicated to the
// user.
class ScreenReader : public dom::DocumentObserver {
 public:
  ScreenReader(dom::Document* document, TTSEngine* tts_engine,
               dom::MutationObserverTaskManager* task_manager);

  void set_enabled(bool enabled);
  bool enabled() const { return enabled_; }

 protected:
  ~ScreenReader();
  // dom::DocumentObserver overrides.
  void OnLoad() override;
  void OnMutation() override {}
  void OnFocusChanged() override;

 private:
  typedef script::Sequence<scoped_refptr<dom::MutationRecord> >
      MutationRecordSequence;
  void FocusChangedCallback();
  // MutationObserver callback for tracking live regions.
  void MutationObserverCallback(
      const MutationRecordSequence& sequence,
      const scoped_refptr<dom::MutationObserver>& observer);

  bool enabled_;
  dom::Document* document_;
  TTSEngine* tts_engine_;
  scoped_refptr<dom::MutationObserver> live_region_observer_;
  bool focus_changed_;

  friend class scoped_ptr<ScreenReader>;
  DISALLOW_COPY_AND_ASSIGN(ScreenReader);
};
}  // namespace accessibility
}  // namespace cobalt

#endif  // COBALT_ACCESSIBILITY_SCREEN_READER_H_
