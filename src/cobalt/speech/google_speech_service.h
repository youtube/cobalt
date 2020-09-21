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

#ifndef COBALT_SPEECH_GOOGLE_SPEECH_SERVICE_H_
#define COBALT_SPEECH_GOOGLE_SPEECH_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/threading/thread.h"
#include "cobalt/loader/url_fetcher_string_writer.h"
#include "cobalt/media/base/audio_bus.h"
#include "cobalt/network/network_module.h"
#include "cobalt/speech/audio_encoder_flac.h"
#include "cobalt/speech/google_streaming_api.pb.h"
#include "cobalt/speech/speech_recognition_config.h"
#include "cobalt/speech/speech_recognition_event.h"
#include "content/browser/speech/chunked_byte_buffer.h"
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
class GoogleSpeechService : public net::URLFetcherDelegate {
 public:
  typedef media::AudioBus AudioBus;
  typedef base::Callback<void(const scoped_refptr<dom::Event>&)> EventCallback;
  typedef SpeechRecognitionResultList::SpeechRecognitionResults
      SpeechRecognitionResults;
  typedef base::Callback<std::unique_ptr<net::URLFetcher>(
      const GURL&, net::URLFetcher::RequestType, net::URLFetcherDelegate*)>
      URLFetcherCreator;

  GoogleSpeechService(network::NetworkModule* network_module,
                      const EventCallback& event_callback,
                      const URLFetcherCreator& fetcher_creator);
  ~GoogleSpeechService() override;

  // Multiple calls to Start/Stop are allowed, the implementation should take
  // care of multiple calls.
  // Start speech recognizer.
  void Start(const SpeechRecognitionConfig& config, int sample_rate);
  // Stop speech recognizer.
  void Stop();
  // An encoded audio data is available and ready to be recognized.
  void RecognizeAudio(std::unique_ptr<AudioBus> audio_bus, bool is_last_chunk);

  // net::URLFetcherDelegate interface
  void OnURLFetchDownloadProgress(const net::URLFetcher* source,
                                  int64_t current, int64_t total,
                                  int64_t current_network_bytes) override;
  void OnURLFetchComplete(const net::URLFetcher* source) override;
  void OnURLFetchUploadProgress(const net::URLFetcher* source, int64 current,
                                int64 total) override {}

  static base::Optional<std::string> GetSpeechAPIKey();

 private:
  void StartInternal(const SpeechRecognitionConfig& config, int sample_rate);
  void StopInternal();
  // This method handles wrappables and should run on the MainWebModule thread.
  void ClearFinalResults();
  void UploadAudioDataInternal(std::unique_ptr<AudioBus> audio_bus,
                               bool is_last_chunk);
  // This method handles wrappables, and so it must run on the MainWebModule.
  void ProcessAndFireSuccessEvent(proto::SpeechRecognitionEvent event);

  // This is used for creating fetchers.
  network::NetworkModule* network_module_;
  // Track the start/stop state of speech recognizer.
  bool started_;

  // Encoder for encoding raw audio data to flac codec.
  std::unique_ptr<AudioEncoderFlac> encoder_;
  // Fetcher for posting the audio data.
  std::unique_ptr<net::URLFetcher> upstream_fetcher_;
  // Fetcher for receiving the streaming results.
  std::unique_ptr<net::URLFetcher> downstream_fetcher_;
  // Used to send speech recognition event.
  const EventCallback event_callback_;
  // Used to create url fetcher.
  const URLFetcherCreator fetcher_creator_;
  // Used for processing proto buffer data.
  content::ChunkedByteBuffer chunked_byte_buffer_;
  // Used for accumulating final results.
  SpeechRecognitionResults final_results_;
  // Speech recognizer is operating in its own thread.
  base::Thread thread_;
  // Stores fetched response.
  loader::URLFetcherStringWriter* download_data_writer_ = nullptr;

  // Use a task runner to deal with all wrappables.
  base::WeakPtrFactory<GoogleSpeechService> weak_ptr_factory_;
  base::WeakPtr<GoogleSpeechService> weak_this_;
  scoped_refptr<base::SingleThreadTaskRunner> const wrappables_task_runner_;
};

}  // namespace speech
}  // namespace cobalt

#endif  // COBALT_SPEECH_GOOGLE_SPEECH_SERVICE_H_
