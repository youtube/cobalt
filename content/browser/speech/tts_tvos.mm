// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "content/browser/speech/tts_tvos.h"

#import <UIKit/UIKit.h>

#include <string_view>

#include "base/apple/foundation_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "content/public/browser/tts_controller.h"

namespace content {

namespace {

std::vector<content::VoiceData>& VoicesRef() {
  static base::NoDestructor<std::vector<content::VoiceData>> voices([]() {
    [NSNotificationCenter.defaultCenter
        addObserverForName:UIApplicationDidBecomeActiveNotification
                    object:nil
                     queue:nil
                usingBlock:^(NSNotification* notification) {
                  // The user might have switched to Settings or some other app
                  // to change voices or locale settings. Avoid a stale cache by
                  // forcing a rebuild of the voices vector after the app
                  // becomes active.
                  //
                  // Note that in the current implementation that only returns
                  // one fake voice called Cobalt, this only serves to update
                  // the voice's language.
                  VoicesRef().clear();
                }];
    return std::vector<content::VoiceData>();
  }());

  return *voices;
}

constexpr std::string_view kVoiceName = "Cobalt";
constexpr int kInvalidUtteranceId = -1;

}  // namespace

TtsPlatformImpTVOS::TtsPlatformImpTVOS() = default;

TtsPlatformImpTVOS::~TtsPlatformImpTVOS() {
  for (const auto& item : pending_observers_) {
    [[NSNotificationCenter defaultCenter] removeObserver:item.second];
  }
}

bool TtsPlatformImpTVOS::PlatformImplSupported() {
  return true;
}

bool TtsPlatformImpTVOS::PlatformImplInitialized() {
  return true;
}

void TtsPlatformImpTVOS::Speak(
    int utterance_id,
    const std::string& utterance,
    const std::string& lang,
    const VoiceData& voice,
    const UtteranceContinuousParameters& params,
    base::OnceCallback<void(bool)> on_speak_finished) {
  // These checks come from C25:
  // https://github.com/youtube/cobalt/blob/25.lts.1%2B/cobalt/speech/speech_synthesis.cc#L118
  // Contrary to the AVSpeechSynthesizer APIs,
  // UIAccessibilityPostNotification() offers few customization possibilities.
  // It is possible to change the pitch with a NSAttributedString, but the
  // other parameters cannot be tuned.
  if (params.volume != 1.0f || params.pitch != 1.0f || params.rate != 1.0f) {
    LOG(ERROR) << "Invalid values for either volume, pitch or rate";
    std::move(on_speak_finished).Run(false);
    return;
  }

  // Parse SSML and process speech.
  content::TtsController::GetInstance()->StripSSML(
      utterance, base::BindOnce(&TtsPlatformImpTVOS::ProcessSpeech,
                                base::Unretained(this), utterance_id, lang,
                                voice, params, std::move(on_speak_finished)));
}

bool TtsPlatformImpTVOS::StopSpeaking() {
  if (utterance_id_ == kInvalidUtteranceId) {
    return true;
  }

  if (auto it = pending_observers_.find(utterance_id_);
      it != pending_observers_.end()) {
    [[NSNotificationCenter defaultCenter] removeObserver:it->second];
    pending_observers_.erase(it);
  }

  utterance_id_ = kInvalidUtteranceId;

  // From C25:
  //
  // There isn't an API to stop the current VoiceOver speech. However,
  // announcements immediately interrupt whatever is currently spoken, so
  // issuing a silent announcement will serve to cancel the current speech.
  // NOTE: An announcement with an empty string will be ignored; single
  // characters will be spoken / described (e.g. " " is pronounced "space"). As
  // a workaround, use two spaces for silence. However, there is no
  // documentation that guarantees two spaces will continue to be silent. In
  // addition, speaking two spaces will lower the volume of content for a few
  // seconds -- just as if something was being spoken.
  UIAccessibilityPostNotification(UIAccessibilityAnnouncementNotification,
                                  @"  ");

  return true;
}

bool TtsPlatformImpTVOS::IsSpeaking() {
  return utterance_id_ != kInvalidUtteranceId;
}

void TtsPlatformImpTVOS::GetVoices(std::vector<VoiceData>* out_voices) {
  auto& voices = VoicesRef();
  if (voices.empty()) {
    voices.emplace_back();
    VoiceData& voice = voices.back();
    voice.name = kVoiceName;
    voice.native = true;
    voice.lang =
        base::SysNSStringToUTF8([[NSLocale preferredLanguages] firstObject]);
    // TODO: add fallback code and maybe more fine-grained parsing like Chrome
    // for iOS does. Check e.g. en-ZA.
    CHECK(!voice.lang.empty());
    voice.events = {
        content::TTS_EVENT_START,
        content::TTS_EVENT_END,
    };
  }
  *out_voices = voices;
}

// The UIKit accessibility-based implementation cannot pause speech
// mid-utterance. The closest solution is to just stop speaking, just like
// TtsPlatformImplAndroid does. Note that it is not possible to resume a
// paused utterance.
void TtsPlatformImpTVOS::Pause() {
  StopSpeaking();
}

// The UIKit accessibility-based implementation does not support resuming a
// paused utterance.
void TtsPlatformImpTVOS::Resume() {}

// static
TtsPlatformImpTVOS* TtsPlatformImpTVOS::GetInstance() {
  static base::NoDestructor<TtsPlatformImpTVOS> tts_platform;
  return tts_platform.get();
}

void TtsPlatformImpTVOS::ProcessSpeech(
    int utterance_id,
    const std::string& lang,
    const content::VoiceData& voice,
    const content::UtteranceContinuousParameters& params,
    base::OnceCallback<void(bool)> on_speak_finished,
    const std::string& parsed_utterance) {
  if (!UIAccessibilityIsVoiceOverRunning()) {
    std::move(on_speak_finished).Run(false);
    return;
  }

  utterance_id_ = utterance_id;

  NSString* utterance_as_nsstring = base::SysUTF8ToNSString(parsed_utterance);

  auto callback = base::BindRepeating(
      [](const base::WeakPtr<TtsPlatformImpTVOS>& impl, int utterance_id,
         NSString* utterance, NSNotification* notification) {
        if (!impl) {
          return;
        }

        NSString* announcement_text = [[notification userInfo]
            valueForKey:UIAccessibilityAnnouncementKeyStringValue];

        if (utterance_id != impl->utterance_id_) {
          return;
        }
        if (![announcement_text isEqualToString:utterance]) {
          return;
        }

        impl->utterance_id_ = kInvalidUtteranceId;

        auto it = impl->pending_observers_.find(utterance_id);
        CHECK(it != impl->pending_observers_.end());
        [[NSNotificationCenter defaultCenter] removeObserver:it->second];
        impl->pending_observers_.erase(it);

        content::TtsController::GetInstance()->OnTtsEvent(
            utterance_id, content::TTS_EVENT_END, 0, [utterance length],
            std::string());
      },
      weak_ptr_factory_.GetWeakPtr(), utterance_id,
      [utterance_as_nsstring copy]);
  const auto observer = [[NSNotificationCenter defaultCenter]
      addObserverForName:UIAccessibilityAnnouncementDidFinishNotification
                  object:nil
                   queue:nil
              usingBlock:base::CallbackToBlock(base::BindPostTask(
                             base::SequencedTaskRunner::GetCurrentDefault(),
                             std::move(callback)))];
  auto result =
      pending_observers_.insert(std::make_pair(utterance_id, observer));
  // There should not be an observer with the same utterance ID at this point.
  CHECK(result.second);

  std::move(on_speak_finished).Run(true);
  content::TtsController::GetInstance()->OnTtsEvent(
      utterance_id_, content::TTS_EVENT_START, 0,
      [utterance_as_nsstring length], std::string());

  UIAccessibilityPostNotification(UIAccessibilityAnnouncementNotification,
                                  utterance_as_nsstring);
}

// static
TtsPlatformImpl* TtsPlatformImpl::GetInstance() {
  return TtsPlatformImpTVOS::GetInstance();
}

}  // namespace content
