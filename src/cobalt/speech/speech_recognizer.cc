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

#include "cobalt/speech/speech_recognizer.h"

#include "base/bind.h"

namespace cobalt {
namespace speech {

SpeechRecognizer::SpeechRecognizer(loader::FetcherFactory* fetcher_factory)
    : fetcher_factory_(fetcher_factory),
      thread_("speech_recognizer"),
      started_(false) {
  thread_.StartWithOptions(base::Thread::Options(MessageLoop::TYPE_IO, 0));
}

SpeechRecognizer::~SpeechRecognizer() {
  Stop();
}

void SpeechRecognizer::Start(const SpeechRecognitionConfig& config) {
  // Called by the speech recognition manager thread.
  thread_.message_loop()->PostTask(FROM_HERE,
                                   base::Bind(&SpeechRecognizer::StartInternal,
                                              base::Unretained(this), config));
}

void SpeechRecognizer::Stop() {
  // Called by the speech recognition manager thread.
  thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&SpeechRecognizer::StopInternal, base::Unretained(this)));
}

void SpeechRecognizer::RecognizeAudio(scoped_array<uint8> encoded_audio_data,
                                      size_t size, bool is_last_chunk) {
  // Called by the speech recognition manager thread.
  thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&SpeechRecognizer::UploadAudioDataInternal,
                 base::Unretained(this), base::Passed(&encoded_audio_data),
                 size, is_last_chunk));
}

void SpeechRecognizer::OnURLFetchDownloadData(
    const net::URLFetcher* source, scoped_ptr<std::string> download_data) {
  DCHECK_EQ(thread_.message_loop(), MessageLoop::current());

  // TODO: process the download data.
  NOTIMPLEMENTED();

  UNREFERENCED_PARAMETER(source);
  UNREFERENCED_PARAMETER(download_data);
}

void SpeechRecognizer::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK_EQ(thread_.message_loop(), MessageLoop::current());
  UNREFERENCED_PARAMETER(source);
}

void SpeechRecognizer::StartInternal(const SpeechRecognitionConfig& config) {
  DCHECK_EQ(thread_.message_loop(), MessageLoop::current());
  if (started_) {
    // Recognizer is already started.
    return;
  }
  started_ = true;

  // TODO: set up url fetchers with this URLFetcherDelegate.
  NOTIMPLEMENTED();

  UNREFERENCED_PARAMETER(config);
  UNREFERENCED_PARAMETER(fetcher_factory_);
}

void SpeechRecognizer::StopInternal() {
  DCHECK_EQ(thread_.message_loop(), MessageLoop::current());
  if (!started_) {
    // Recognizer is not started.
    return;
  }
  started_ = false;

  // TODO: terminate url fetchers.
  NOTIMPLEMENTED();
}

void SpeechRecognizer::UploadAudioDataInternal(
    scoped_array<uint8> encoded_audio_data, size_t size, bool is_last_chunk) {
  DCHECK_EQ(thread_.message_loop(), MessageLoop::current());

  // TODO: upload encoded audio data chunk.
  NOTIMPLEMENTED();

  UNREFERENCED_PARAMETER(encoded_audio_data);
  UNREFERENCED_PARAMETER(size);
  UNREFERENCED_PARAMETER(is_last_chunk);
}

}  // namespace speech
}  // namespace cobalt
