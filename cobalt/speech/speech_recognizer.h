/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef COBALT_SPEECH_SPEECH_RECOGNIZER_H_
#define COBALT_SPEECH_SPEECH_RECOGNIZER_H_

#include <string>

#include "base/threading/thread.h"
#include "cobalt/loader/fetcher_factory.h"
#include "cobalt/speech/speech_recognition_config.h"
#include "net/url_request/url_fetcher_delegate.h"

namespace cobalt {
namespace speech {

// Interacts with Google speech recogniton service, and then parses recognition
// results and forms speech recogniton event.
// It creates an upstream fetcher to upload the encoded audio and a downstream
// fetcher to fetch the speech recognition result. The fetched speech
// recognition result is parsed by JSON parser and a SpeechRecognitionEvent,
// is formed based on the parsed result, would be sent to speech recognition
// manager.
class SpeechRecognizer : public net::URLFetcherDelegate {
 public:
  explicit SpeechRecognizer(loader::FetcherFactory* fetcher_factory);
  ~SpeechRecognizer() OVERRIDE;

  // Start speech recognizer.
  void Start(const SpeechRecognitionConfig& config);
  // Stop speech recognizer.
  void Stop();
  // An encoded audio data is available and ready to be recognized.
  void RecognizeAudio(scoped_array<uint8> encoded_audio_data,
                      bool is_last_chunk);

  // net::URLFetcherDelegate interface
  void OnURLFetchDownloadData(const net::URLFetcher* source,
                              scoped_ptr<std::string> download_data) OVERRIDE;
  void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;
  bool ShouldSendDownloadData() OVERRIDE { return true; }
  void OnURLFetchUploadProgress(const net::URLFetcher* /*source*/,
                                int64 /*current*/, int64 /*total*/) OVERRIDE {}

 private:
  void StartInternal(const SpeechRecognitionConfig& config);
  void StopInternal();

  void UploadAudioDataInternal(scoped_array<uint8> encoded_audio_data,
                               bool is_last_chunk);

  // This is used for creating fetchers.
  loader::FetcherFactory* fetcher_factory_;
  // Speech recognizer is operating in its own thread.
  base::Thread thread_;
  // Track the start/stop state of speech recognizer.
  bool started_;
};

}  // namespace speech
}  // namespace cobalt

#endif  // COBALT_SPEECH_SPEECH_RECOGNIZER_H_
