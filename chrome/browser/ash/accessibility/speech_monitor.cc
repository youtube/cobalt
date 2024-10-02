// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/speech_monitor.h"

#include "base/strings/pattern.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/tts_controller.h"

namespace ash {
namespace test {
namespace {

constexpr int kPrintExpectationDelayMs = 3000;

}  // namespace

SpeechMonitor::SpeechMonitor() {
  content::TtsController::SkipAddNetworkChangeObserverForTests(true);
  content::TtsController::GetInstance()->SetTtsPlatform(this);
}

SpeechMonitor::~SpeechMonitor() {
  content::TtsController::GetInstance()->SetTtsPlatform(
      content::TtsPlatform::GetInstance());
  if (!replay_queue_.empty() || !replayed_queue_.empty())
    CHECK(replay_called_) << "Expectation was made, but Replay() not called.";
}

bool SpeechMonitor::PlatformImplSupported() {
  return true;
}

bool SpeechMonitor::PlatformImplInitialized() {
  return true;
}

void SpeechMonitor::Speak(int utterance_id,
                          const std::string& utterance,
                          const std::string& lang,
                          const content::VoiceData& voice,
                          const content::UtteranceContinuousParameters& params,
                          base::OnceCallback<void(bool)> on_speak_finished) {
  CHECK(!utterance.empty())
      << "If you're deliberately speaking the "
         "empty string in a test, that's probably not the correct way to "
         "achieve stopping speech. If it is unintended, it indicates a deeper "
         "underlying issue.";
  content::TtsController::GetInstance()->OnTtsEvent(
      utterance_id, content::TTS_EVENT_START, 0,
      static_cast<int>(utterance.size()), std::string());
  content::TtsController::GetInstance()->OnTtsEvent(
      utterance_id, content::TTS_EVENT_END, static_cast<int>(utterance.size()),
      0, std::string());
  std::move(on_speak_finished).Run(true);
  time_of_last_utterance_ = std::chrono::steady_clock::now();
}

bool SpeechMonitor::StopSpeaking() {
  ++stop_count_;
  return true;
}

bool SpeechMonitor::IsSpeaking() {
  return false;
}

void SpeechMonitor::GetVoices(std::vector<content::VoiceData>* out_voices) {
  out_voices->push_back(content::VoiceData());
  content::VoiceData& voice = out_voices->back();
  voice.native = true;
  voice.name = "SpeechMonitor";
  voice.engine_id = extension_misc::kGoogleSpeechSynthesisExtensionId;
  voice.events.insert(content::TTS_EVENT_END);
}

void SpeechMonitor::WillSpeakUtteranceWithVoice(
    content::TtsUtterance* utterance,
    const content::VoiceData& voice_data) {
  utterance_queue_.emplace_back(utterance->GetText(), utterance->GetLang());
  delay_for_last_utterance_ms_ = CalculateUtteranceDelayMS();
  MaybeContinueReplay();
}

void SpeechMonitor::LoadBuiltInTtsEngine(
    content::BrowserContext* browser_context) {}

std::string SpeechMonitor::GetError() {
  return error_;
}

void SpeechMonitor::ClearError() {
  error_ = std::string();
}

void SpeechMonitor::SetError(const std::string& error) {
  error_ = error;
}

void SpeechMonitor::Shutdown() {}

void SpeechMonitor::FinalizeVoiceOrdering(
    std::vector<content::VoiceData>& voices) {}

void SpeechMonitor::RefreshVoices() {}

content::ExternalPlatformDelegate*
SpeechMonitor::GetExternalPlatformDelegate() {
  return nullptr;
}

double SpeechMonitor::CalculateUtteranceDelayMS() {
  std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
  std::chrono::duration<double> time_span =
      std::chrono::duration_cast<std::chrono::duration<double>>(
          now - time_of_last_utterance_);
  return time_span.count() * 1000;
}

double SpeechMonitor::GetDelayForLastUtteranceMS() {
  return delay_for_last_utterance_ms_;
}

void SpeechMonitor::ExpectSpeech(const std::string& text,
                                 const base::Location& location) {
  CHECK(!replay_loop_runner_.get());
  replay_queue_.push_back(
      {[this, text]() {
         std::vector<std::string> all_text;
         for (auto it = utterance_queue_.begin(); it != utterance_queue_.end();
              it++) {
           all_text.push_back(it->text);
           std::string joined_all_text = base::JoinString(all_text, " ");
           if (it->text == text ||
               joined_all_text.find(text) != std::string::npos) {
             // Erase all utterances that came before the
             // match as well as the match itself.
             utterance_queue_.erase(utterance_queue_.begin(), it + 1);
             return true;
           }
         }
         return false;
       },
       "ExpectSpeech(\"" + text + "\") " + location.ToString()});
}

void SpeechMonitor::ExpectSpeechPattern(const std::string& pattern,
                                        const base::Location& location) {
  ExpectSpeechPatternWithLocale(pattern, "", location);
}

void SpeechMonitor::ExpectSpeechPatternWithLocale(
    const std::string& pattern,
    const std::string& locale,
    const base::Location& location) {
  CHECK(!replay_loop_runner_.get());
  replay_queue_.push_back(
      {[this, pattern, locale]() {
         std::vector<std::string> all_text;
         for (auto it = utterance_queue_.begin(); it != utterance_queue_.end();
              it++) {
           all_text.push_back(it->text);
           std::string joined_all_text = base::JoinString(all_text, " ");
           if ((base::MatchPattern(it->text, pattern) &&
                (locale.empty() || it->lang == locale)) ||
               base::MatchPattern(joined_all_text, "*" + pattern)) {
             // Erase all utterances that came before the
             // match as well as the match itself.
             utterance_queue_.erase(utterance_queue_.begin(), it + 1);
             return true;
           }
         }
         return false;
       },
       "ExpectSpeechPattern(\"" + pattern + "\") " + location.ToString()});
}

void SpeechMonitor::ExpectNextSpeechIsNot(const std::string& text,
                                          const base::Location& location) {
  CHECK(!replay_loop_runner_.get());
  replay_queue_.push_back(
      {[this, text]() {
         if (utterance_queue_.empty())
           return false;

         return text != utterance_queue_.front().text;
       },
       "ExpectNextSpeechIsNot(\"" + text + "\") " + location.ToString()});
}

void SpeechMonitor::ExpectNextSpeechIsNotPattern(
    const std::string& pattern,
    const base::Location& location) {
  CHECK(!replay_loop_runner_.get());
  replay_queue_.push_back({[this, pattern]() {
                             if (utterance_queue_.empty())
                               return false;

                             return !base::MatchPattern(
                                 utterance_queue_.front().text, pattern);
                           },
                           "ExpectNextSpeechIsNotPattern(\"" + pattern +
                               "\") " + location.ToString()});
}

void SpeechMonitor::Call(std::function<void()> func,
                         const base::Location& location) {
  CHECK(!replay_loop_runner_.get());
  replay_queue_.push_back({[func]() {
                             func();
                             return true;
                           },
                           "Call() " + location.ToString()});
}

void SpeechMonitor::Replay() {
  replay_called_ = true;
  MaybeContinueReplay();
}

void SpeechMonitor::MaybeContinueReplay() {
  // This method can be called prior to Replay() being called.
  if (!replay_called_)
    return;

  auto it = replay_queue_.begin();
  while (it != replay_queue_.end()) {
    ReplayArgs current = *it;
    it = replay_queue_.erase(it);
    if (current.first()) {
      // Careful here; the above callback may have triggered more speech which
      // causes |MaybeContinueReplay| to be called recursively. We have to
      // ensure to check |replay_queue_| here.
      if (replay_queue_.empty())
        break;

      replayed_queue_.push_back(current.second);
    } else {
      replay_queue_.insert(replay_queue_.begin(), current);
      it = replay_queue_.begin();
      break;
    }
  }

  if (!replay_queue_.empty()) {
    content::GetUIThreadTaskRunner({})->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&SpeechMonitor::MaybePrintExpectations,
                       weak_factory_.GetWeakPtr()),
        base::Milliseconds(kPrintExpectationDelayMs));

    if (!replay_loop_runner_.get()) {
      replay_loop_runner_ = new content::MessageLoopRunner();
      replay_loop_runner_->Run();
    }
  } else if (replay_queue_.empty() && replay_loop_runner_.get()) {
    replay_loop_runner_->Quit();
  }
}

void SpeechMonitor::MaybePrintExpectations() {
  if (CalculateUtteranceDelayMS() < kPrintExpectationDelayMs ||
      replay_queue_.empty())
    return;

  if (last_replay_queue_size_ == replay_queue_.size())
    return;

  last_replay_queue_size_ = replay_queue_.size();
  std::vector<std::string> replay_queue_descriptions;
  for (const auto& pair : replay_queue_)
    replay_queue_descriptions.push_back(pair.second);

  std::vector<std::string> utterance_queue_descriptions;
  for (const auto& item : utterance_queue_)
    utterance_queue_descriptions.push_back("\"" + item.text + "\"");

  LOG(ERROR) << "Still waiting for expectation(s).\n"
             << "Unsatisfied expectations...\n"
             << base::JoinString(replay_queue_descriptions, "\n") << "\n\n"
             << "pending speech utterances...\n"
             << base::JoinString(utterance_queue_descriptions, "\n") << "\n\n"
             << "Satisfied expectations...\n"
             << base::JoinString(replayed_queue_, "\n");
}

}  // namespace test
}  // namespace ash
