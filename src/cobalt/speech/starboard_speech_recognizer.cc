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

#include "cobalt/speech/starboard_speech_recognizer.h"

#if defined(SB_USE_SB_SPEECH_RECOGNIZER)

#include <utility>

#include "cobalt/base/tokens.h"
#include "cobalt/speech/speech_recognition_error.h"
#include "cobalt/speech/speech_recognition_event.h"
#include "starboard/common/log.h"
#include "starboard/types.h"

namespace cobalt {
namespace speech {

// static
bool StarboardSpeechRecognizer::IsSupported() {
#if SB_API_VERSION >= 13
  return false;
#elif SB_API_VERSION >= 12
  return SbSpeechRecognizerIsSupported();
#else
  return true;
#endif
}

// static
void StarboardSpeechRecognizer::OnSpeechDetected(void* context, bool detected) {
  StarboardSpeechRecognizer* recognizer =
      static_cast<StarboardSpeechRecognizer*>(context);
  recognizer->message_loop_->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&StarboardSpeechRecognizer::OnRecognizerSpeechDetected,
                 recognizer->weak_factory_.GetWeakPtr(), detected));
}

// static
void StarboardSpeechRecognizer::OnError(void* context,
                                        SbSpeechRecognizerError error) {
  StarboardSpeechRecognizer* recognizer =
      static_cast<StarboardSpeechRecognizer*>(context);
  recognizer->message_loop_->task_runner()->PostTask(
      FROM_HERE, base::Bind(&StarboardSpeechRecognizer::OnRecognizerError,
                            recognizer->weak_factory_.GetWeakPtr(), error));
}

// static
void StarboardSpeechRecognizer::OnResults(void* context,
                                          SbSpeechResult* results,
                                          int results_size, bool is_final) {
  StarboardSpeechRecognizer* recognizer =
      static_cast<StarboardSpeechRecognizer*>(context);

  std::vector<SpeechRecognitionAlternative::Data> results_copy;
  results_copy.reserve(results_size);
  for (int i = 0; i < results_size; ++i) {
    results_copy.emplace_back(SpeechRecognitionAlternative::Data{
        results[i].transcript, results[i].confidence});
  }

  recognizer->message_loop_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&StarboardSpeechRecognizer::OnRecognizerResults,
                                recognizer->weak_factory_.GetWeakPtr(),
                                std::move(results_copy), is_final));
}

StarboardSpeechRecognizer::StarboardSpeechRecognizer(
    const EventCallback& event_callback)
    : SpeechRecognizer(event_callback),
      message_loop_(base::MessageLoop::current()),
      weak_factory_(this) {
  SbSpeechRecognizerHandler handler = {&OnSpeechDetected, &OnError, &OnResults,
                                       this};
  speech_recognizer_ = SbSpeechRecognizerCreate(&handler);

  if (!SbSpeechRecognizerIsValid(speech_recognizer_)) {
    scoped_refptr<dom::Event> error_event(new SpeechRecognitionError(
        kSpeechRecognitionErrorCodeServiceNotAllowed, ""));
    RunEventCallback(error_event);
  }
}

StarboardSpeechRecognizer::~StarboardSpeechRecognizer() {
  if (SbSpeechRecognizerIsValid(speech_recognizer_)) {
    SbSpeechRecognizerDestroy(speech_recognizer_);
  }
}

void StarboardSpeechRecognizer::Start(const SpeechRecognitionConfig& config) {
  SB_DCHECK(config.max_alternatives < INT_MAX);
  SbSpeechConfiguration configuration = {
      config.continuous, config.interim_results,
      static_cast<int>(config.max_alternatives)};
  if (SbSpeechRecognizerIsValid(speech_recognizer_)) {
    SbSpeechRecognizerStart(speech_recognizer_, &configuration);
  }
}

void StarboardSpeechRecognizer::Stop() {
  if (SbSpeechRecognizerIsValid(speech_recognizer_)) {
    SbSpeechRecognizerStop(speech_recognizer_);
  }
  // Clear the final results.
  final_results_.clear();
}

void StarboardSpeechRecognizer::OnRecognizerSpeechDetected(bool detected) {
  scoped_refptr<dom::Event> event(new dom::Event(
      detected ? base::Tokens::soundstart() : base::Tokens::soundend()));
  RunEventCallback(event);
}

void StarboardSpeechRecognizer::OnRecognizerError(
    SbSpeechRecognizerError error) {
  scoped_refptr<dom::Event> error_event;
  switch (error) {
    case kSbNoSpeechError:
      error_event =
          new SpeechRecognitionError(kSpeechRecognitionErrorCodeNoSpeech, "");
      break;
    case kSbAborted:
      error_event =
          new SpeechRecognitionError(kSpeechRecognitionErrorCodeAborted, "");
      break;
    case kSbAudioCaptureError:
      error_event = new SpeechRecognitionError(
          kSpeechRecognitionErrorCodeAudioCapture, "");
      break;
    case kSbNetworkError:
      error_event =
          new SpeechRecognitionError(kSpeechRecognitionErrorCodeNetwork, "");
      break;
    case kSbNotAllowed:
      error_event =
          new SpeechRecognitionError(kSpeechRecognitionErrorCodeNotAllowed, "");
      break;
    case kSbServiceNotAllowed:
      error_event = new SpeechRecognitionError(
          kSpeechRecognitionErrorCodeServiceNotAllowed, "");
      break;
    case kSbBadGrammar:
      error_event =
          new SpeechRecognitionError(kSpeechRecognitionErrorCodeBadGrammar, "");
      break;
    case kSbLanguageNotSupported:
      error_event = new SpeechRecognitionError(
          kSpeechRecognitionErrorCodeLanguageNotSupported, "");
      break;
  }
  SB_DCHECK(error_event);
  RunEventCallback(error_event);
}

void StarboardSpeechRecognizer::OnRecognizerResults(
    std::vector<SpeechRecognitionAlternative::Data>&& results, bool is_final) {
  SpeechRecognitionResultList::SpeechRecognitionResults recognition_results;
  SpeechRecognitionResult::SpeechRecognitionAlternatives alternatives;
  for (auto& result : results) {
    scoped_refptr<SpeechRecognitionAlternative> alternative(
        new SpeechRecognitionAlternative(std::move(result)));
    alternatives.push_back(alternative);
  }
  scoped_refptr<SpeechRecognitionResult> recognition_result(
      new SpeechRecognitionResult(alternatives, is_final));
  recognition_results.push_back(recognition_result);

  // Gather all results for the SpeechRecognitionEvent, including all final
  // results we've previously accumulated, plus all (final or not) results that
  // we just received.
  SpeechRecognitionResults success_results;
  size_t total_size = final_results_.size() + recognition_results.size();
  success_results.reserve(total_size);
  success_results = final_results_;
  success_results.insert(success_results.end(), recognition_results.begin(),
                         recognition_results.end());

  size_t result_index = final_results_.size();
  // Update final results list with final results that we just received, so we
  // have them for the next event.
  for (size_t i = 0; i < recognition_results.size(); ++i) {
    if (recognition_results[i]->is_final()) {
      final_results_.push_back(recognition_results[i]);
    }
  }

  scoped_refptr<SpeechRecognitionResultList> recognition_list(
      new SpeechRecognitionResultList(success_results));
  scoped_refptr<SpeechRecognitionEvent> recognition_event(
      new SpeechRecognitionEvent(SpeechRecognitionEvent::kResult,
                                 static_cast<uint32>(result_index),
                                 recognition_list));
  RunEventCallback(recognition_event);
}

}  // namespace speech
}  // namespace cobalt

#endif  // defined(SB_USE_SB_SPEECH_RECOGNIZER)
