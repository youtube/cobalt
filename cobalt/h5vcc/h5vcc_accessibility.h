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

#ifndef COBALT_H5VCC_H5VCC_ACCESSIBILITY_H_
#define COBALT_H5VCC_H5VCC_ACCESSIBILITY_H_

#include <memory>

#include "base/message_loop/message_loop.h"
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
  explicit H5vccAccessibility(base::EventDispatcher* event_dispatcher);
  ~H5vccAccessibility();

  bool high_contrast_text() const;
  void AddHighContrastTextListener(
      const H5vccAccessibilityCallbackHolder& holder);
  void OnApplicationEvent(const base::Event* event);

  bool text_to_speech() const;
  void AddTextToSpeechListener(
      const H5vccAccessibilityCallbackHolder& holder);

  bool built_in_screen_reader() const;
  void set_built_in_screen_reader(bool value);

  DEFINE_WRAPPABLE_TYPE(H5vccAccessibility);

 private:
  typedef H5vccAccessibilityCallbackHolder::Reference
      H5vccAccessibilityCallbackReference;

  void InternalOnApplicationEvent(base::TypeId type);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  base::EventCallback on_application_event_callback_;
  base::EventDispatcher* event_dispatcher_;
  std::unique_ptr<H5vccAccessibilityCallbackReference>
      high_contrast_text_listener_;
  std::unique_ptr<H5vccAccessibilityCallbackReference>
      text_to_speech_listener_;

  DISALLOW_COPY_AND_ASSIGN(H5vccAccessibility);
};

}  // namespace h5vcc
}  // namespace cobalt

#endif  // COBALT_H5VCC_H5VCC_ACCESSIBILITY_H_
