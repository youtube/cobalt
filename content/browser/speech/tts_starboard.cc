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

#include "content/browser/speech/tts_platform_impl.h"

#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/tts_controller.h"
#include "starboard/speech_synthesis.h"
#include "starboard/system.h"

namespace content {

class TtsPlatformImplStarboard : public TtsPlatformImpl {
 public:
  TtsPlatformImplStarboard(const TtsPlatformImplStarboard&) = delete;
  TtsPlatformImplStarboard& operator=(const TtsPlatformImplStarboard&) = delete;

  // TtsPlatformImpl implementation.
  bool PlatformImplSupported() override;
  bool PlatformImplInitialized() override;
  void Speak(int utterance_id,
             const std::string& utterance,
             const std::string& lang,
             const VoiceData& voice,
             const UtteranceContinuousParameters& params,
             base::OnceCallback<void(bool)> on_speak_finished) override;
  bool StopSpeaking() override;
  void Pause() override;
  void Resume() override;
  bool IsSpeaking() override;
  void GetVoices(std::vector<VoiceData>* out_voices) override;

  static TtsPlatformImplStarboard* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<TtsPlatformImplStarboard>;
  TtsPlatformImplStarboard();
  ~TtsPlatformImplStarboard() override;

  void ProcessSpeech(int utterance_id,
                     const std::string& lang,
                     const VoiceData& voice,
                     const UtteranceContinuousParameters& params,
                     base::OnceCallback<void(bool)> on_speak_finished,
                     const std::string& parsed_utterance);

  int utterance_id_ = -1;
  base::WeakPtrFactory<TtsPlatformImplStarboard> weak_factory_{this};
};

TtsPlatformImplStarboard::TtsPlatformImplStarboard() = default;

TtsPlatformImplStarboard::~TtsPlatformImplStarboard() = default;

bool TtsPlatformImplStarboard::PlatformImplSupported() {
  return SbSpeechSynthesisIsSupported();
}

bool TtsPlatformImplStarboard::PlatformImplInitialized() {
  return SbSpeechSynthesisIsSupported();
}

void TtsPlatformImplStarboard::Speak(
    int utterance_id,
    const std::string& utterance,
    const std::string& lang,
    const VoiceData& voice,
    const UtteranceContinuousParameters& params,
    base::OnceCallback<void(bool)> on_speak_finished) {
  if (!SbSpeechSynthesisIsSupported()) {
    std::move(on_speak_finished).Run(false);
    return;
  }

  // Parse SSML and process speech.
  TtsController::GetInstance()->StripSSML(
      utterance, base::BindOnce(&TtsPlatformImplStarboard::ProcessSpeech,
                                weak_factory_.GetWeakPtr(), utterance_id, lang,
                                voice, params, std::move(on_speak_finished)));
}

void TtsPlatformImplStarboard::ProcessSpeech(
    int utterance_id,
    const std::string& lang,
    const VoiceData& voice,
    const UtteranceContinuousParameters& params,
    base::OnceCallback<void(bool)> on_speak_finished,
    const std::string& parsed_utterance) {
  utterance_id_ = utterance_id;

  SbSpeechSynthesisSpeak(parsed_utterance.c_str());

  std::move(on_speak_finished).Run(true);

  // Starboard API doesn't provide callbacks for when speech starts or ends.
  // We simulate a start event immediately.
  TtsController::GetInstance()->OnTtsEvent(
      utterance_id_, TTS_EVENT_START, 0,
      static_cast<int>(parsed_utterance.length()), std::string());

  // And we immediately send an end event as we can't track it.
  TtsController::GetInstance()->OnTtsEvent(
      utterance_id_, TTS_EVENT_END, static_cast<int>(parsed_utterance.length()),
      0, std::string());

  utterance_id_ = -1;
}

bool TtsPlatformImplStarboard::StopSpeaking() {
  if (!SbSpeechSynthesisIsSupported()) {
    return false;
  }
  SbSpeechSynthesisCancel();
  utterance_id_ = -1;
  return true;
}

void TtsPlatformImplStarboard::Pause() {
  StopSpeaking();
}

void TtsPlatformImplStarboard::Resume() {}

bool TtsPlatformImplStarboard::IsSpeaking() {
  return utterance_id_ != -1;
}

void TtsPlatformImplStarboard::GetVoices(std::vector<VoiceData>* out_voices) {
  if (!SbSpeechSynthesisIsSupported()) {
    return;
  }
  out_voices->emplace_back();
  VoiceData& voice = out_voices->back();
  voice.native = true;
  voice.name = "Starboard";
  // Starboard API dictates language should be the same as
  // SbSystemGetLocaleId().
  voice.lang = SbSystemGetLocaleId();
  voice.events.insert(TTS_EVENT_START);
  voice.events.insert(TTS_EVENT_END);
}

// static
TtsPlatformImplStarboard* TtsPlatformImplStarboard::GetInstance() {
  return base::Singleton<
      TtsPlatformImplStarboard,
      base::LeakySingletonTraits<TtsPlatformImplStarboard>>::get();
}

// static
TtsPlatformImpl* TtsPlatformImpl::GetInstance() {
  return TtsPlatformImplStarboard::GetInstance();
}

}  // namespace content
