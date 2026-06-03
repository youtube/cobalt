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

#ifndef CONTENT_BROWSER_SPEECH_TTS_TVOS_H_
#define CONTENT_BROWSER_SPEECH_TTS_TVOS_H_

#include <map>

#include "base/memory/weak_ptr.h"
#include "content/browser/speech/tts_platform_impl.h"

namespace content {

// This speech synthesis backend implementation is specific to Cobalt's and
// Kabuki's needs. Kabuki uses the Speech Synthesis web API to implement screen
// reader support (rather than relying on Chromium's a11y infrastructure), and
// the Cobalt 25 implementation used UIAccessibilityPostNotification() for
// speech synthesis to ensure the same voice used by VoiceOver is chosen
// (certain Siri voices are not available to AVSpeechSynthesizer even if
// AVSpeechUtterance's prefersAssistiveTechnologySettings property is set to
// YES).
//
// As such, this backend is not completely spec-compliant, as there is little to
// no support for voice customizations as allowed by the web API, and the only
// voice returned by this class is a fake one called "Cobalt", to keep Cobalt
// 25's behavior.
//
// If a more spec-compliant implementation is desired in the future, one should
// copy the tts_mac.{h,mm} one that is based on AVSpeechSynthesizer and requires
// few adjustments to work with with UIKit.
class TtsPlatformImpTVOS : public TtsPlatformImpl {
 public:
  TtsPlatformImpTVOS();
  ~TtsPlatformImpTVOS() override;

  TtsPlatformImpTVOS(const TtsPlatformImpTVOS&) = delete;
  TtsPlatformImpTVOS& operator=(const TtsPlatformImpTVOS&) = delete;

  // TtsPlatform implementation.
  bool PlatformImplSupported() override;
  bool PlatformImplInitialized() override;
  void Speak(int utterance_id,
             const std::string& utterance,
             const std::string& lang,
             const VoiceData& voice,
             const UtteranceContinuousParameters& params,
             base::OnceCallback<void(bool)> on_speak_finished) override;
  bool StopSpeaking() override;
  bool IsSpeaking() override;
  void GetVoices(std::vector<VoiceData>* out_voices) override;
  void Pause() override;
  void Resume() override;

  static TtsPlatformImpTVOS* GetInstance();

 private:
  void ProcessSpeech(int utterance_id,
                     const std::string& lang,
                     const content::VoiceData& voice,
                     const content::UtteranceContinuousParameters& params,
                     base::OnceCallback<void(bool)> on_speak_finished,
                     const std::string& parsed_utterance);

  int utterance_id_ = -1;
  std::map<int, id<NSObject>> pending_observers_;

  base::WeakPtrFactory<TtsPlatformImpTVOS> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SPEECH_TTS_TVOS_H_
