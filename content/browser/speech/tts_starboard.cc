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

// This is a stub implementation of the TTS platform for Starboard.
//
// TODO: (b/420913744) Properly implement the TtsPlatformImplStarboard
// and TtsPlatformImplBackgroundWorker classes.

#include "base/no_destructor.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "content/public/browser/browser_thread.h"
#include "content/browser/speech/tts_platform_impl.h"


namespace content {

// A stubbed-out background worker for the TTS platform. All methods are no-ops.
class TtsPlatformImplBackgroundWorker {
 public:
  TtsPlatformImplBackgroundWorker() = default;
  TtsPlatformImplBackgroundWorker(const TtsPlatformImplBackgroundWorker&) =
      delete;
  TtsPlatformImplBackgroundWorker& operator=(
      const TtsPlatformImplBackgroundWorker&) = delete;
  ~TtsPlatformImplBackgroundWorker() = default;

  void Initialize() {}
  void ProcessSpeech(int utterance_id,
                     const std::string& parsed_utterance,
                     const std::string& lang,
                     float rate,
                     float pitch,
                     base::OnceCallback<void(bool)> on_speak_finished) {
    // The worker is non-functional, so it immediately reports failure.
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(on_speak_finished), false));
  }
  void Pause() {}
  void Resume() {}
  void StopSpeaking() {}
  void Shutdown() {}
};

class TtsPlatformImplStarboard : public TtsPlatformImpl {
 public:
  TtsPlatformImplStarboard(const TtsPlatformImplStarboard&) = delete;
  TtsPlatformImplStarboard& operator=(const TtsPlatformImplStarboard&) = delete;

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
  void Shutdown() override;

  static TtsPlatformImplStarboard* GetInstance();

 private:
  friend base::NoDestructor<TtsPlatformImplStarboard>;
  TtsPlatformImplStarboard();

  base::SequenceBound<TtsPlatformImplBackgroundWorker> worker_;
};

//
// TtsPlatformImplStarboard
//

TtsPlatformImplStarboard::TtsPlatformImplStarboard()
    // Initialize the worker on a background sequence.
    : worker_(base::ThreadPool::CreateSequencedTaskRunner({})) {}

bool TtsPlatformImplStarboard::PlatformImplSupported() {
  return false;
}

bool TtsPlatformImplStarboard::PlatformImplInitialized() {
  return false;
}

void TtsPlatformImplStarboard::Speak(
    int utterance_id,
    const std::string& utterance,
    const std::string& lang,
    const VoiceData& voice,
    const UtteranceContinuousParameters& params,
    base::OnceCallback<void(bool)> on_speak_finished) {
  // Although we forward the call to the stubbed worker, the task will fail.
  worker_.AsyncCall(&TtsPlatformImplBackgroundWorker::ProcessSpeech)
      .WithArgs(utterance_id, utterance, lang, params.rate, params.pitch,
                std::move(on_speak_finished));
}

bool TtsPlatformImplStarboard::StopSpeaking() {
  worker_.AsyncCall(&TtsPlatformImplBackgroundWorker::StopSpeaking);
  return true;
}

void TtsPlatformImplStarboard::Pause() {
  worker_.AsyncCall(&TtsPlatformImplBackgroundWorker::Pause);
}

void TtsPlatformImplStarboard::Resume() {
  worker_.AsyncCall(&TtsPlatformImplBackgroundWorker::Resume);
}

bool TtsPlatformImplStarboard::IsSpeaking() {
  return false;
}

void TtsPlatformImplStarboard::GetVoices(std::vector<VoiceData>* out_voices) {}

void TtsPlatformImplStarboard::Shutdown() {
  worker_.AsyncCall(&TtsPlatformImplBackgroundWorker::Shutdown);
}

// static
TtsPlatformImplStarboard* TtsPlatformImplStarboard::GetInstance() {
  static base::NoDestructor<TtsPlatformImplStarboard> tts_platform;
  return tts_platform.get();
}

// static
TtsPlatformImpl* TtsPlatformImpl::GetInstance() {
  return TtsPlatformImplStarboard::GetInstance();
}

}  // namespace content
