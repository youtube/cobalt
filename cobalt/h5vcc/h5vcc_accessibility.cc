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

#include "cobalt/h5vcc/h5vcc_accessibility.h"

#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "cobalt/base/accessibility_settings_changed_event.h"
#include "cobalt/base/accessibility_text_to_speech_settings_changed_event.h"
#include "cobalt/browser/switches.h"
#include "starboard/accessibility.h"
#include "starboard/memory.h"

namespace cobalt {
namespace h5vcc {

namespace {

bool ShouldForceTextToSpeech() {
#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
  // Check for a command-line override to enable TTS.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(browser::switches::kUseTTS)) {
    return true;
  }
#endif  // defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
  return false;
}

}  // namespace

H5vccAccessibility::H5vccAccessibility(base::EventDispatcher* event_dispatcher)
    : event_dispatcher_(event_dispatcher) {
  task_runner_ = base::MessageLoop::current()->task_runner();
  on_application_event_callback_ = base::Bind(
      &H5vccAccessibility::OnApplicationEvent, base::Unretained(this));
  event_dispatcher_->AddEventCallback(
      base::AccessibilitySettingsChangedEvent::TypeId(),
      on_application_event_callback_);
  event_dispatcher_->AddEventCallback(
      base::AccessibilityTextToSpeechSettingsChangedEvent::TypeId(),
      on_application_event_callback_);
}

H5vccAccessibility::~H5vccAccessibility() {
  event_dispatcher_->RemoveEventCallback(
      base::AccessibilitySettingsChangedEvent::TypeId(),
      on_application_event_callback_);
  event_dispatcher_->RemoveEventCallback(
      base::AccessibilityTextToSpeechSettingsChangedEvent::TypeId(),
      on_application_event_callback_);
}

bool H5vccAccessibility::built_in_screen_reader() const { return false; }

void H5vccAccessibility::set_built_in_screen_reader(bool value) {
  if (value) {
    LOG(WARNING) << "h5vcc.accessibility.builtInScreenReader: not available";
  }
}

bool H5vccAccessibility::high_contrast_text() const {
  SbAccessibilityDisplaySettings settings;
  memset(&settings, 0, sizeof(settings));

  if (!SbAccessibilityGetDisplaySettings(&settings)) {
    return false;
  }

  return settings.is_high_contrast_text_enabled;
}

bool H5vccAccessibility::text_to_speech() const {
  if (ShouldForceTextToSpeech()) {
    // If forcing TTS, return true so that JavaScript TTS will also be enabled.
    return true;
  }
  SbAccessibilityTextToSpeechSettings settings;
  memset(&settings, 0, sizeof(settings));

  if (!SbAccessibilityGetTextToSpeechSettings(&settings)) {
    return false;
  }

  return settings.has_text_to_speech_setting &&
         settings.is_text_to_speech_enabled;
}

void H5vccAccessibility::AddTextToSpeechListener(
    const H5vccAccessibilityCallbackHolder& holder) {
  DCHECK_EQ(base::MessageLoop::current()->task_runner(), task_runner_);
  text_to_speech_listener_.reset(
      new H5vccAccessibilityCallbackReference(this, holder));
}

void H5vccAccessibility::AddHighContrastTextListener(
    const H5vccAccessibilityCallbackHolder& holder) {
  DCHECK_EQ(base::MessageLoop::current()->task_runner(), task_runner_);
  high_contrast_text_listener_.reset(
      new H5vccAccessibilityCallbackReference(this, holder));
}

void H5vccAccessibility::OnApplicationEvent(const base::Event* event) {
  // This method should be called from the application event thread.
  DCHECK_NE(base::MessageLoop::current()->task_runner(), task_runner_);
  task_runner_->PostTask(
      FROM_HERE, base::Bind(&H5vccAccessibility::InternalOnApplicationEvent,
                            base::Unretained(this),
                            event->GetTypeId()));
}

void H5vccAccessibility::InternalOnApplicationEvent(base::TypeId type) {
  DCHECK_EQ(base::MessageLoop::current()->task_runner(), task_runner_);
  if (type == base::AccessibilitySettingsChangedEvent::TypeId() &&
      high_contrast_text_listener_) {
    high_contrast_text_listener_->value().Run();
  }
  if (type == base::AccessibilityTextToSpeechSettingsChangedEvent::TypeId() &&
      text_to_speech_listener_) {
    text_to_speech_listener_->value().Run();
  }
}

}  // namespace h5vcc
}  // namespace cobalt
