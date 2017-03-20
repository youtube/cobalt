// Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef COBALT_SPEECH_SANDBOX_SPEECH_SANDBOX_H_
#define COBALT_SPEECH_SANDBOX_SPEECH_SANDBOX_H_

#include <string>

#include "base/file_path.h"
#include "base/threading/thread.h"
#include "cobalt/dom/dom_settings.h"
#include "cobalt/speech/sandbox/audio_loader.h"
#include "cobalt/speech/speech_recognition.h"
#include "cobalt/trace_event/scoped_trace_to_file.h"

namespace cobalt {
namespace speech {
namespace sandbox {

// This class is for speech sandbox application to experiment with voice search.
// It takes a wav audio as audio input for a fake microphone, and starts/stops
// speech recognition.
class SpeechSandbox {
 public:
  // The constructor takes a file path string for an audio input and a log path
  // for tracing.
  SpeechSandbox(const std::string& file_path_string,
                const FilePath& trace_log_path);
  ~SpeechSandbox();

 private:
  void StartRecognition(const dom::DOMSettings::Options& dom_settings_options);
  void OnLoadingDone(const uint8* data, int size);

  scoped_ptr<trace_event::ScopedTraceToFile> trace_to_file_;
  scoped_ptr<network::NetworkModule> network_module_;
  scoped_ptr<AudioLoader> audio_loader_;
  scoped_refptr<SpeechRecognition> speech_recognition_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(SpeechSandbox);
};

}  // namespace sandbox
}  // namespace speech
}  // namespace cobalt

#endif  // COBALT_SPEECH_SANDBOX_SPEECH_SANDBOX_H_
