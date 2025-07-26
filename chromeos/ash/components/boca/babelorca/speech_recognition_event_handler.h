// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_SPEECH_RECOGNITION_EVENT_HANDLER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_SPEECH_RECOGNITION_EVENT_HANDLER_H_

#include <optional>
#include <string>

#include "chromeos/ash/components/boca/babelorca/babel_orca_speech_recognizer.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"

namespace ash::babelorca {

// This class implements the behavior of the BabelOrcaSpeechReocgnizerImpl for
// handling speech recognition events.
class SpeechRecognitionEventHandler {
 public:
  explicit SpeechRecognitionEventHandler(const std::string& source_language);
  ~SpeechRecognitionEventHandler();
  SpeechRecognitionEventHandler(const SpeechRecognitionEventHandler&) = delete;
  SpeechRecognitionEventHandler& operator=(
      const SpeechRecognitionEventHandler&) = delete;

  // Set and unset callback for transcription results
  void SetTranscriptionResultCallback(
      BabelOrcaSpeechRecognizer::TranscriptionResultCallback
          transcription_result_callback,
      BabelOrcaSpeechRecognizer::LanguageIdentificationEventCallback
          language_identification_callback);
  void RemoveSpeechRecognitionObservation();

  // Called by the speech recognizer when a transcript is received.
  void OnSpeechResult(
      const std::optional<media::SpeechRecognitionResult>& result);

  // called by the speech recognizer when a language change is identified.
  void OnLanguageIdentificationEvent(
      const media::mojom::LanguageIdentificationEventPtr& event);

 private:
  std::string source_language_;

  BabelOrcaSpeechRecognizer::LanguageIdentificationEventCallback
      language_identification_callback_;
  BabelOrcaSpeechRecognizer::TranscriptionResultCallback
      transcription_result_callback_;
};

}  // namespace ash::babelorca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_SPEECH_RECOGNITION_EVENT_HANDLER_H_
