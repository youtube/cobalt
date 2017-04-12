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

#ifndef COBALT_H5VCC_H5VCC_ACCESSIBILITY_H_
#define COBALT_H5VCC_H5VCC_ACCESSIBILITY_H_

#include "base/message_loop_proxy.h"
#include "cobalt/accessibility/screen_reader.h"
#include "cobalt/accessibility/tts_engine.h"
#include "cobalt/base/event_dispatcher.h"
#include "cobalt/dom/window.h"
#include "cobalt/script/callback_function.h"
#include "cobalt/script/script_value.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace h5vcc {

class H5vccAccessibility : public script::Wrappable {
 public:
  // Type for JavaScript event callback.
  typedef script::CallbackFunction<void()> H5vccAccessibilityCallback;
  typedef script::ScriptValue<H5vccAccessibilityCallback>
      H5vccAccessibilityCallbackHolder;
  H5vccAccessibility(
      base::EventDispatcher* event_dispatcher,
      const scoped_refptr<dom::Window>& window,
      dom::MutationObserverTaskManager* mutation_observer_task_manager);

  bool high_contrast_text() const;
  void AddHighContrastTextListener(
      const H5vccAccessibilityCallbackHolder& holder);
  void OnApplicationEvent(const base::Event* event);

  bool text_to_speech() const;

  DEFINE_WRAPPABLE_TYPE(H5vccAccessibility);

 private:
  typedef H5vccAccessibilityCallbackHolder::Reference
      H5vccAccessibilityCallbackReference;

  void InternalOnApplicationEvent();

  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;

  base::EventDispatcher* event_dispatcher_;
  scoped_ptr<H5vccAccessibilityCallbackReference> high_contrast_text_listener_;
  scoped_ptr<accessibility::TTSEngine> tts_engine_;
  scoped_ptr<accessibility::ScreenReader> screen_reader_;

  DISALLOW_COPY_AND_ASSIGN(H5vccAccessibility);
};

}  // namespace h5vcc
}  // namespace cobalt

#endif  // COBALT_H5VCC_H5VCC_ACCESSIBILITY_H_
