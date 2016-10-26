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
#include <vector>

#include "base/threading/thread.h"
#include "cobalt/network/network_module.h"
#include "cobalt/speech/audio_encoder_flac.h"
#include "cobalt/speech/chunked_byte_buffer.h"
#include "cobalt/speech/speech_recognition_config.h"
#include "cobalt/speech/speech_recognition_event.h"
#include "media/base/audio_bus.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"

namespace cobalt {
namespace speech {

// Interacts with Google speech recognition service, and then parses recognition
// results and forms speech recognition event.
// It creates an upstream fetcher to upload the encoded audio and a downstream
// fetcher to fetch the speech recognition result. The fetched speech
// recognition result is parsed by JSON parser and a SpeechRecognitionEvent,
// is formed based on the parsed result, would be sent to speech recognition
// manager.
class SpeechRecognizer : public net::URLFetcherDelegate {
 public:
  typedef ::media::AudioBus AudioBus;
  typedef base::Callback<void(const scoped_refptr<dom::Event>&)> EventCallback;
  typedef SpeechRecognitionResultList::SpeechRecognitionResults
      SpeechRecognitionResults;

  SpeechRecognizer(network::NetworkModule* network_module,
                   const EventCallback& event_callback);
  ~SpeechRecognizer() OVERRIDE;

  // Multiple calls to Start/Stop are allowed, the implementation should take
  // care of multiple calls.
  // Start speech recognizer.
  void Start(const SpeechRecognitionConfig& config, int sample_rate);
  // Stop speech recognizer.
  void Stop();
  // An encoded audio data is available and ready to be recognized.
  void RecognizeAudio(scoped_ptr<AudioBus> audio_bus, bool is_last_chunk);

  // net::URLFetcherDelegate interface
  void OnURLFetchDownloadData(const net::URLFetcher* source,
                              scoped_ptr<std::string> download_data) OVERRIDE;
  void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;
  bool ShouldSendDownloadData() OVERRIDE { return true; }
  void OnURLFetchUploadProgress(const net::URLFetcher* /*source*/,
                                int64 /*current*/, int64 /*total*/) OVERRIDE {}

 private:
  void StartInternal(const SpeechRecognitionConfig& config, int sample_rate);
  void StopInternal();
  void UploadAudioDataInternal(scoped_ptr<AudioBus> audio_bus,
                               bool is_last_chunk);
  void ProcessAndFireSuccessEvent(const SpeechRecognitionResults& new_results);

  // This is used for creating fetchers.
  network::NetworkModule* network_module_;
  // Speech recognizer is operating in its own thread.
  base::Thread thread_;
  // Track the start/stop state of speech recognizer.
  bool started_;

  // Encoder for encoding raw audio data to flac codec.
  scoped_ptr<AudioEncoderFlac> encoder_;
  // Fetcher for posting the audio data.
  scoped_ptr<net::URLFetcher> upstream_fetcher_;
  // Fetcher for receiving the streaming results.
  scoped_ptr<net::URLFetcher> downstream_fetcher_;
  EventCallback event_callback_;
  // Used for processing proto buffer data.
  ChunkedByteBuffer chunked_byte_buffer_;
  // Used for accumulating final results.
  SpeechRecognitionResults final_results_;
};

}  // namespace speech
}  // namespace cobalt

#endif  // COBALT_SPEECH_SPEECH_RECOGNIZER_H_
