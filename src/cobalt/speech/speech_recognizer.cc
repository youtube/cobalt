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
#include "base/rand_util.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "cobalt/loader/fetcher_factory.h"
#include "cobalt/network/network_module.h"
#include "net/base/escape.h"
#include "net/url_request/url_fetcher.h"

namespace cobalt {
namespace speech {

namespace {
const char kBaseStreamURL[] =
    "https://www.google.com/speech-api/full-duplex/v1";
// TODO: hide this key to somewhere else.
const char kSpeechAPIKey[] = "";
const char kUp[] = "up";
const char kDown[] = "down";
const char kClient[] = "com.speech.tv";

GURL AppendPath(const GURL& url, const std::string& value) {
  std::string path(url.path());

  if (!path.empty()) path += "/";

  path += net::EscapePath(value);
  GURL::Replacements replacements;
  replacements.SetPathStr(path);
  return url.ReplaceComponents(replacements);
}

GURL AppendQueryParameter(const GURL& url, const std::string& new_query,
                          const std::string& value) {
  std::string query(url.query());

  if (!query.empty()) query += "&";

  query += net::EscapeQueryParamValue(new_query, true);

  if (!value.empty()) {
    query += "=" + net::EscapeQueryParamValue(value, true);
  }

  GURL::Replacements replacements;
  replacements.SetQueryStr(query);
  return url.ReplaceComponents(replacements);
}

}  // namespace

SpeechRecognizer::SpeechRecognizer(network::NetworkModule* network_module,
                                   const ResultCallback& result_callback,
                                   const ErrorCallback& error_callback)
    : network_module_(network_module),
      thread_("speech_recognizer"),
      started_(false),
      result_callback_(result_callback),
      error_callback_(error_callback) {
  thread_.StartWithOptions(base::Thread::Options(MessageLoop::TYPE_IO, 0));
}

SpeechRecognizer::~SpeechRecognizer() {
  Stop();
}

void SpeechRecognizer::Start(const SpeechRecognitionConfig& config,
                             int sample_rate) {
  // Called by the speech recognition manager thread.
  thread_.message_loop()->PostTask(
      FROM_HERE, base::Bind(&SpeechRecognizer::StartInternal,
                            base::Unretained(this), config, sample_rate));
}

void SpeechRecognizer::Stop() {
  // Called by the speech recognition manager thread.
  thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&SpeechRecognizer::StopInternal, base::Unretained(this)));
}

void SpeechRecognizer::RecognizeAudio(scoped_ptr<AudioBus> audio_bus,
                                      bool is_last_chunk) {
  // Called by the speech recognition manager thread.
  thread_.message_loop()->PostTask(
      FROM_HERE, base::Bind(&SpeechRecognizer::UploadAudioDataInternal,
                            base::Unretained(this), base::Passed(&audio_bus),
                            is_last_chunk));
}

void SpeechRecognizer::OnURLFetchDownloadData(
    const net::URLFetcher* source, scoped_ptr<std::string> download_data) {
  DCHECK_EQ(thread_.message_loop(), MessageLoop::current());
  // TODO: Parse the serialized protocol buffers data.
  NOTIMPLEMENTED();

  UNREFERENCED_PARAMETER(source);
  UNREFERENCED_PARAMETER(download_data);
}

void SpeechRecognizer::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK_EQ(thread_.message_loop(), MessageLoop::current());
  UNREFERENCED_PARAMETER(source);
  started_ = false;
}

void SpeechRecognizer::StartInternal(const SpeechRecognitionConfig& config,
                                     int sample_rate) {
  DCHECK_EQ(thread_.message_loop(), MessageLoop::current());

  if (started_) {
    // Recognizer is already started.
    return;
  }
  started_ = true;

  encoder_.reset(new AudioEncoderFlac(sample_rate));

  // Required for streaming on both up and down connections.
  std::string pair = base::Uint64ToString(base::RandUint64());

  // Set up down stream first.
  GURL down_url(kBaseStreamURL);
  down_url = AppendPath(down_url, kDown);
  down_url = AppendQueryParameter(down_url, "pair", pair);
  // Use protobuffer as the output format.
  down_url = AppendQueryParameter(down_url, "output", "pb");

  downstream_fetcher_.reset(
      net::URLFetcher::Create(down_url, net::URLFetcher::GET, this));
  downstream_fetcher_->SetRequestContext(
      network_module_->url_request_context_getter());
  downstream_fetcher_->Start();

  // Up stream.
  GURL up_url(kBaseStreamURL);
  up_url = AppendPath(up_url, kUp);
  up_url = AppendQueryParameter(up_url, "client", kClient);
  up_url = AppendQueryParameter(up_url, "pair", pair);
  up_url = AppendQueryParameter(up_url, "output", "pb");
  up_url = AppendQueryParameter(up_url, "key", kSpeechAPIKey);

  if (!config.lang.empty()) {
    up_url = AppendQueryParameter(up_url, "lang", config.lang);
  }

  if (config.max_alternatives) {
    up_url = AppendQueryParameter(up_url, "maxAlternatives",
                                  base::UintToString(config.max_alternatives));
  }

  if (config.continuous) {
    up_url = AppendQueryParameter(up_url, "continuous", "");
  }
  if (config.interim_results) {
    up_url = AppendQueryParameter(up_url, "interim", "");
  }

  upstream_fetcher_.reset(
      net::URLFetcher::Create(up_url, net::URLFetcher::POST, this));
  upstream_fetcher_->SetRequestContext(
      network_module_->url_request_context_getter());
  upstream_fetcher_->SetChunkedUpload(encoder_->GetMimeType());
  upstream_fetcher_->Start();
}

void SpeechRecognizer::StopInternal() {
  DCHECK_EQ(thread_.message_loop(), MessageLoop::current());

  if (!started_) {
    // Recognizer is not started.
    return;
  }
  started_ = false;

  upstream_fetcher_.reset();
  downstream_fetcher_.reset();
  encoder_.reset();
}

void SpeechRecognizer::UploadAudioDataInternal(scoped_ptr<AudioBus> audio_bus,
                                               bool is_last_chunk) {
  DCHECK_EQ(thread_.message_loop(), MessageLoop::current());
  DCHECK(audio_bus);

  std::string encoded_audio_data;
  if (encoder_) {
    encoder_->Encode(audio_bus.get());
    if (is_last_chunk) {
      encoder_->Finish();
    }
    encoded_audio_data = encoder_->GetAndClearAvailableEncodedData();
  }

  if (upstream_fetcher_ && !encoded_audio_data.empty()) {
    upstream_fetcher_->AppendChunkToUpload(encoded_audio_data, is_last_chunk);
  }
}

}  // namespace speech
}  // namespace cobalt
