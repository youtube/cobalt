// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "cobalt/speech/cobalt_speech_recognizer.h"

#include "base/bind.h"
#include "cobalt/base/tokens.h"
#if defined(ENABLE_FAKE_MICROPHONE)
#include "cobalt/speech/microphone_fake.h"
#include "cobalt/speech/url_fetcher_fake.h"
#endif  // defined(ENABLE_FAKE_MICROPHONE)
#include "cobalt/speech/microphone_manager.h"
#if defined(SB_USE_SB_MICROPHONE)
#include "cobalt/speech/microphone_starboard.h"
#endif  // defined(SB_USE_SB_MICROPHONE)
#include "net/url_request/url_fetcher.h"

namespace cobalt {
namespace speech {

namespace {
const int kSampleRate = 16000;
const float kAudioPacketDurationInSeconds = 0.1f;

scoped_ptr<net::URLFetcher> CreateURLFetcher(
    const GURL& url, net::URLFetcher::RequestType request_type,
    net::URLFetcherDelegate* delegate) {
  return make_scoped_ptr<net::URLFetcher>(
      net::URLFetcher::Create(url, request_type, delegate));
}

scoped_ptr<Microphone> CreateMicrophone(int buffer_size_bytes) {
#if defined(SB_USE_SB_MICROPHONE)
  return make_scoped_ptr<Microphone>(
      new MicrophoneStarboard(kSampleRate, buffer_size_bytes));
#else
  UNREFERENCED_PARAMETER(buffer_size_bytes);
  return scoped_ptr<Microphone>();
#endif  // defined(SB_USE_SB_MICROPHONE)
}

#if defined(SB_USE_SB_MICROPHONE)
#if defined(ENABLE_FAKE_MICROPHONE)
scoped_ptr<net::URLFetcher> CreateFakeURLFetcher(
    const GURL& url, net::URLFetcher::RequestType request_type,
    net::URLFetcherDelegate* delegate) {
  return make_scoped_ptr<net::URLFetcher>(
      new URLFetcherFake(url, request_type, delegate));
}

scoped_ptr<Microphone> CreateFakeMicrophone(const Microphone::Options& options,
                                            int /*buffer_size_bytes*/) {
  return make_scoped_ptr<Microphone>(new MicrophoneFake(options));
}
#endif  // defined(ENABLE_FAKE_MICROPHONE)
#endif  // defined(SB_USE_SB_MICROPHONE)
}  // namespace

CobaltSpeechRecognizer::CobaltSpeechRecognizer(
    network::NetworkModule* network_module,
    const Microphone::Options& microphone_options,
    const EventCallback& success_callback, const EventCallback& error_callback)
    : success_callback_(success_callback),
      error_callback_(error_callback),
      endpointer_delegate_(kSampleRate) {
  UNREFERENCED_PARAMETER(microphone_options);

  SpeechRecognizer::URLFetcherCreator url_fetcher_creator =
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

  recognizer_.reset(new SpeechRecognizer(
      network_module, base::Bind(&CobaltSpeechRecognizer::OnRecognizerEvent,
                                 base::Unretained(this)),
      url_fetcher_creator));
  microphone_manager_.reset(new MicrophoneManager(
      base::Bind(&CobaltSpeechRecognizer::OnDataReceived,
                 base::Unretained(this)),
      base::Bind(&CobaltSpeechRecognizer::OnDataCompletion,
                 base::Unretained(this)),
      base::Bind(&CobaltSpeechRecognizer::OnMicError, base::Unretained(this)),
      microphone_creator));
}

CobaltSpeechRecognizer::~CobaltSpeechRecognizer() {}

void CobaltSpeechRecognizer::Start(const SpeechRecognitionConfig& config) {
  recognizer_->Start(config, kSampleRate);
  microphone_manager_->Open();
  endpointer_delegate_.Start();
}

void CobaltSpeechRecognizer::Stop() {
  endpointer_delegate_.Stop();
  microphone_manager_->Close();
  recognizer_->Stop();
  success_callback_.Run(new dom::Event(base::Tokens::soundend()));
}

void CobaltSpeechRecognizer::OnDataReceived(
    scoped_ptr<ShellAudioBus> audio_bus) {
  if (endpointer_delegate_.IsFirstTimeSoundStarted(*audio_bus)) {
    success_callback_.Run(new dom::Event(base::Tokens::soundstart()));
  }
  recognizer_->RecognizeAudio(audio_bus.Pass(), false);
}

void CobaltSpeechRecognizer::OnDataCompletion() {
  // The encoder requires a non-empty final buffer, so encoding a packet of
  // silence at the end in case encoder had no data already.
  size_t dummy_frames =
      static_cast<size_t>(kSampleRate * kAudioPacketDurationInSeconds);
  scoped_ptr<ShellAudioBus> dummy_audio_bus(new ShellAudioBus(
      1, dummy_frames, ShellAudioBus::kInt16, ShellAudioBus::kInterleaved));
  dummy_audio_bus->ZeroAllFrames();
  recognizer_->RecognizeAudio(dummy_audio_bus.Pass(), true);
}

void CobaltSpeechRecognizer::OnRecognizerEvent(
    const scoped_refptr<dom::Event>& event) {
  success_callback_.Run(event);
}

void CobaltSpeechRecognizer::OnMicError(
    const scoped_refptr<dom::Event>& event) {
  // An error is occured in Mic, so stop the energy endpointer and recognizer.
  endpointer_delegate_.Stop();
  recognizer_->Stop();
  error_callback_.Run(event);
}

}  // namespace speech
}  // namespace cobalt
