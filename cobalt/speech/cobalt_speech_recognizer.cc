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

#include <memory>

#include "cobalt/speech/cobalt_speech_recognizer.h"

#include "base/bind.h"
#include "cobalt/base/tokens.h"
#if defined(ENABLE_FAKE_MICROPHONE)
#include "cobalt/speech/microphone_fake.h"
#include "cobalt/speech/url_fetcher_fake.h"
#endif  // defined(ENABLE_FAKE_MICROPHONE)
#include "cobalt/speech/microphone_manager.h"
#include "cobalt/speech/speech_recognition_error.h"
#if defined(SB_USE_SB_MICROPHONE)
#include "cobalt/speech/microphone_starboard.h"
#endif  // defined(SB_USE_SB_MICROPHONE)
#include "net/url_request/url_fetcher.h"

namespace cobalt {
namespace speech {

namespace {
const int kSampleRate = 16000;
const float kAudioPacketDurationInSeconds = 0.1f;

std::unique_ptr<net::URLFetcher> CreateURLFetcher(
    const GURL& url, net::URLFetcher::RequestType request_type,
    net::URLFetcherDelegate* delegate) {
  return std::unique_ptr<net::URLFetcher>(
      net::URLFetcher::Create(url, request_type, delegate));
}

std::unique_ptr<Microphone> CreateMicrophone(int buffer_size_bytes) {
#if defined(SB_USE_SB_MICROPHONE)
  return std::unique_ptr<Microphone>(
      new MicrophoneStarboard(kSampleRate, buffer_size_bytes));
#else
  return std::unique_ptr<Microphone>();
#endif  // defined(SB_USE_SB_MICROPHONE)
}

#if defined(SB_USE_SB_MICROPHONE)
#if defined(ENABLE_FAKE_MICROPHONE)
std::unique_ptr<net::URLFetcher> CreateFakeURLFetcher(
    const GURL& url, net::URLFetcher::RequestType request_type,
    net::URLFetcherDelegate* delegate) {
  return std::unique_ptr<net::URLFetcher>(
      new URLFetcherFake(url, request_type, delegate));
}

std::unique_ptr<Microphone> CreateFakeMicrophone(
    const Microphone::Options& options, int buffer_size_bytes) {
  return std::unique_ptr<Microphone>(new MicrophoneFake(options));
}
#endif  // defined(ENABLE_FAKE_MICROPHONE)
#endif  // defined(SB_USE_SB_MICROPHONE)
}  // namespace

CobaltSpeechRecognizer::CobaltSpeechRecognizer(
    network::NetworkModule* network_module,
    const Microphone::Options& microphone_options,
    const EventCallback& event_callback)
    : SpeechRecognizer(event_callback), endpointer_delegate_(kSampleRate) {

  GoogleSpeechService::URLFetcherCreator url_fetcher_creator =
      base::Bind(&CreateURLFetcher);
  MicrophoneManager::MicrophoneCreator microphone_creator =
      base::Bind(&CreateMicrophone);

#if defined(SB_USE_SB_MICROPHONE)
#if defined(ENABLE_FAKE_MICROPHONE)
  if (microphone_options.enable_fake_microphone) {
    // If fake microphone is enabled, fake URL fetchers should be enabled as
    // well.
    url_fetcher_creator = base::Bind(&CreateFakeURLFetcher);
    microphone_creator = base::Bind(&CreateFakeMicrophone, microphone_options);
  }
#endif  // defined(ENABLE_FAKE_MICROPHONE)
#endif  // defined(SB_USE_SB_MICROPHONE)

  service_.reset(new GoogleSpeechService(
      network_module,
      base::Bind(&CobaltSpeechRecognizer::OnRecognizerEvent,
                 base::Unretained(this)),
      url_fetcher_creator));
  microphone_manager_.reset(new MicrophoneManager(
      base::Bind(&CobaltSpeechRecognizer::OnDataReceived,
                 base::Unretained(this)),
      base::Closure(),
      base::Bind(&CobaltSpeechRecognizer::OnDataCompletion,
                 base::Unretained(this)),
      base::Bind(&CobaltSpeechRecognizer::OnMicrophoneError,
                 base::Unretained(this)),
      microphone_creator));
}

CobaltSpeechRecognizer::~CobaltSpeechRecognizer() {}

void CobaltSpeechRecognizer::Start(const SpeechRecognitionConfig& config) {
  service_->Start(config, kSampleRate);
  microphone_manager_->Open();
  endpointer_delegate_.Start();
}

void CobaltSpeechRecognizer::Stop() {
  endpointer_delegate_.Stop();
  microphone_manager_->Close();
  service_->Stop();
  RunEventCallback(new dom::Event(base::Tokens::soundend()));
}

void CobaltSpeechRecognizer::OnDataReceived(
    std::unique_ptr<AudioBus> audio_bus) {
  if (endpointer_delegate_.IsFirstTimeSoundStarted(*audio_bus)) {
    RunEventCallback(new dom::Event(base::Tokens::soundstart()));
  }
  service_->RecognizeAudio(std::move(audio_bus), false);
}

void CobaltSpeechRecognizer::OnDataCompletion() {
  // The encoder requires a non-empty final buffer, so encoding a packet of
  // silence at the end in case encoder had no data already.
  size_t dummy_frames =
      static_cast<size_t>(kSampleRate * kAudioPacketDurationInSeconds);
  std::unique_ptr<AudioBus> dummy_audio_bus(
      new AudioBus(1, dummy_frames, AudioBus::kInt16, AudioBus::kInterleaved));
  dummy_audio_bus->ZeroAllFrames();
  service_->RecognizeAudio(std::move(dummy_audio_bus), true);
}

void CobaltSpeechRecognizer::OnRecognizerEvent(
    const scoped_refptr<dom::Event>& event) {
  RunEventCallback(event);
}

void CobaltSpeechRecognizer::OnMicrophoneError(
    MicrophoneManager::MicrophoneError error, std::string error_message) {
  // An error is occured in Mic, so stop the energy endpointer and recognizer.

  SpeechRecognitionErrorCode speech_error_code =
      kSpeechRecognitionErrorCodeAborted;
  switch (error) {
    case MicrophoneManager::MicrophoneError::kAborted:
      speech_error_code = kSpeechRecognitionErrorCodeAborted;
      break;
    case MicrophoneManager::MicrophoneError::kAudioCapture:
      speech_error_code = kSpeechRecognitionErrorCodeAudioCapture;
      break;
  }
  scoped_refptr<dom::Event> event(
      new SpeechRecognitionError(speech_error_code, error_message));
  endpointer_delegate_.Stop();
  service_->Stop();
  RunEventCallback(event);
}

}  // namespace speech
}  // namespace cobalt
