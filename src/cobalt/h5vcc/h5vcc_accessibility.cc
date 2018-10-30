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
#include "base/message_loop.h"
#include "cobalt/accessibility/starboard_tts_engine.h"
#include "cobalt/accessibility/tts_engine.h"
#include "cobalt/accessibility/tts_logger.h"
#include "cobalt/base/accessibility_settings_changed_event.h"
#include "cobalt/browser/switches.h"
#include "starboard/accessibility.h"
#include "starboard/memory.h"

namespace cobalt {
namespace h5vcc {

namespace {

bool ShouldForceTextToSpeech() {
#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
  // Check for a command-line override to enable TTS.
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(browser::switches::kUseTTS)) {
    return true;
  }
#endif  // defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
  return false;
}

#if SB_HAS(SPEECH_SYNTHESIS)
bool IsTextToSpeechEnabled() {
  // Check if the tts feature is enabled in Starboard.
  SbAccessibilityTextToSpeechSettings tts_settings = {0};
  // Check platform settings.
  if (SbAccessibilityGetTextToSpeechSettings(&tts_settings)) {
    return tts_settings.has_text_to_speech_setting &&
           tts_settings.is_text_to_speech_enabled;
  }

  return false;
}
#endif  // SB_HAS(SPEECH_SYNTHESIS)

}  // namespace

H5vccAccessibility::H5vccAccessibility(
    base::EventDispatcher* event_dispatcher,
    const scoped_refptr<dom::Window>& window,
    dom::MutationObserverTaskManager* mutation_observer_task_manager)
    : event_dispatcher_(event_dispatcher) {
  message_loop_proxy_ = base::MessageLoopProxy::current();
  event_dispatcher_->AddEventCallback(
      base::AccessibilitySettingsChangedEvent::TypeId(),
      base::Bind(&H5vccAccessibility::OnApplicationEvent,
                 base::Unretained(this)));
  if (ShouldForceTextToSpeech()) {
#if SB_HAS(SPEECH_SYNTHESIS)
    // Create a StarboardTTSEngine if the platform has speech synthesis.
    tts_engine_.reset(new accessibility::StarboardTTSEngine());
#else
    tts_engine_.reset(new accessibility::TTSLogger());
#endif
  }

#if SB_HAS(SPEECH_SYNTHESIS)
  if (!tts_engine_ && IsTextToSpeechEnabled()) {
    // Create a StarboardTTSEngine if TTS is enabled.
    tts_engine_.reset(new accessibility::StarboardTTSEngine());
  }
#endif

  if (tts_engine_) {
    screen_reader_.reset(new accessibility::ScreenReader(
        window->document(), tts_engine_.get(), mutation_observer_task_manager));
  }
}

H5vccAccessibility::~H5vccAccessibility() {
  event_dispatcher_->RemoveEventCallback(
      base::AccessibilitySettingsChangedEvent::TypeId(),
      base::Bind(&H5vccAccessibility::OnApplicationEvent,
                 base::Unretained(this)));
}

bool H5vccAccessibility::built_in_screen_reader() const {
  return screen_reader_ && screen_reader_->enabled();
}

void H5vccAccessibility::set_built_in_screen_reader(bool value) {
  if (!screen_reader_) {
    LOG(WARNING) << "h5vcc.accessibility.builtInScreenReader: not available";
    return;
  }
  screen_reader_->set_enabled(value);
}

bool H5vccAccessibility::high_contrast_text() const {
  SbAccessibilityDisplaySettings settings;
  SbMemorySet(&settings, 0, sizeof(settings));

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
  SbMemorySet(&settings, 0, sizeof(settings));

  if (!SbAccessibilityGetTextToSpeechSettings(&settings)) {
    return false;
  }

  return settings.has_text_to_speech_setting &&
      settings.is_text_to_speech_enabled;
}

void H5vccAccessibility::AddHighContrastTextListener(
    const H5vccAccessibilityCallbackHolder& holder) {
  DCHECK_EQ(base::MessageLoopProxy::current(), message_loop_proxy_);
  high_contrast_text_listener_.reset(
      new H5vccAccessibilityCallbackReference(this, holder));
}

void H5vccAccessibility::OnApplicationEvent(const base::Event* event) {
  UNREFERENCED_PARAMETER(event);
  // This method should be called from the application event thread.
  DCHECK_NE(base::MessageLoopProxy::current(), message_loop_proxy_);
  message_loop_proxy_->PostTask(
      FROM_HERE, base::Bind(&H5vccAccessibility::InternalOnApplicationEvent,
                            base::Unretained(this)));
}

void H5vccAccessibility::InternalOnApplicationEvent() {
  DCHECK_EQ(base::MessageLoopProxy::current(), message_loop_proxy_);
  if (high_contrast_text_listener_) {
    high_contrast_text_listener_->value().Run();
  }
}

}  // namespace h5vcc
}  // namespace cobalt
