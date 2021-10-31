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

// Note to Cobalt porters: To use the v1 speech-api API, you must provide an API
// key with v1 speech-api quota. The code is provided here, but not an API key.
//
// This is similar to how Chromium handles API keys:
// https://www.chromium.org/developers/how-tos/api-keys
//
// The API key is provided by SbSystemGetProperty:
// http://cobalt.foo/reference/starboard/modules/system.html#sbsystemgetproperty
//
// Talk with your Google representative about how to get speech-api quota.

#include <memory>

#include "cobalt/speech/google_speech_service.h"

#include "base/bind.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "cobalt/base/language.h"
#include "cobalt/loader/fetcher_factory.h"
#include "cobalt/network/network_module.h"
#include "cobalt/speech/microphone.h"
#include "cobalt/speech/speech_configuration.h"
#include "cobalt/speech/speech_recognition_error.h"
#include "net/base/escape.h"
#include "net/url_request/url_fetcher.h"

#if defined(SB_USE_SB_MICROPHONE)
#include "starboard/system.h"
#endif  // defined(SB_USE_SB_MICROPHONE)

namespace cobalt {
namespace speech {

namespace {
const char kBaseStreamURL[] =
    "https://www.google.com/speech-api/full-duplex/v1";
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

SpeechRecognitionResultList::SpeechRecognitionResults
ProcessProtoSuccessResults(proto::SpeechRecognitionEvent event) {
  // This method handles wrappables and should run on the MainWebModule thread.

  DCHECK_EQ(event.status(), proto::SpeechRecognitionEvent::STATUS_SUCCESS);

  SpeechRecognitionResultList::SpeechRecognitionResults results;
  for (int i = 0; i < event.result_size(); ++i) {
    SpeechRecognitionResult::SpeechRecognitionAlternatives alternatives;
    const proto::SpeechRecognitionResult& proto_result = event.result(i);
    for (int j = 0; j < proto_result.alternative_size(); ++j) {
      const proto::SpeechRecognitionAlternative& proto_alternative =
          proto_result.alternative(j);
      float confidence = 0.0f;
      if (proto_alternative.has_confidence()) {
        confidence = proto_alternative.confidence();
      } else if (proto_result.has_stability()) {
        confidence = proto_result.stability();
      }
      scoped_refptr<SpeechRecognitionAlternative> alternative(
          new SpeechRecognitionAlternative(proto_alternative.transcript(),
                                           confidence));
      alternatives.push_back(alternative);
    }

    bool final = proto_result.has_final() && proto_result.final();
    scoped_refptr<SpeechRecognitionResult> recognition_result(
        new SpeechRecognitionResult(alternatives, final));
    results.push_back(recognition_result);
  }
  return results;
}

// TODO: Feed error messages when creating SpeechRecognitionError.
void ProcessAndFireErrorEvent(
    proto::SpeechRecognitionEvent event,
    GoogleSpeechService::EventCallback event_callback) {
  // This method handles wrappables and should run on the MainWebModule thread.
  scoped_refptr<dom::Event> error_event;
  switch (event.status()) {
    case proto::SpeechRecognitionEvent::STATUS_SUCCESS:
      NOTREACHED();
      return;
    case proto::SpeechRecognitionEvent::STATUS_NO_SPEECH:
      error_event =
          new SpeechRecognitionError(kSpeechRecognitionErrorCodeNoSpeech, "");
      break;
    case proto::SpeechRecognitionEvent::STATUS_ABORTED:
      error_event =
          new SpeechRecognitionError(kSpeechRecognitionErrorCodeAborted, "");
      break;
    case proto::SpeechRecognitionEvent::STATUS_AUDIO_CAPTURE:
      error_event = new SpeechRecognitionError(
          kSpeechRecognitionErrorCodeAudioCapture, "");
      break;
    case proto::SpeechRecognitionEvent::STATUS_NETWORK:
      error_event =
          new SpeechRecognitionError(kSpeechRecognitionErrorCodeNetwork, "");
      break;
    case proto::SpeechRecognitionEvent::STATUS_NOT_ALLOWED:
      error_event =
          new SpeechRecognitionError(kSpeechRecognitionErrorCodeNotAllowed, "");
      break;
    case proto::SpeechRecognitionEvent::STATUS_SERVICE_NOT_ALLOWED:
      error_event = new SpeechRecognitionError(
          kSpeechRecognitionErrorCodeServiceNotAllowed, "");
      break;
    case proto::SpeechRecognitionEvent::STATUS_BAD_GRAMMAR:
      error_event =
          new SpeechRecognitionError(kSpeechRecognitionErrorCodeBadGrammar, "");
      break;
    case proto::SpeechRecognitionEvent::STATUS_LANGUAGE_NOT_SUPPORTED:
      error_event = new SpeechRecognitionError(
          kSpeechRecognitionErrorCodeLanguageNotSupported, "");
      break;
  }

  DCHECK(error_event);
  event_callback.Run(error_event);
}

void HandleNetworkResponseFailure(
    const GoogleSpeechService::EventCallback& event_callback) {
  event_callback.Run(new SpeechRecognitionError(
      kSpeechRecognitionErrorCodeNetwork, "Network response failure."));
}

bool IsResponseCodeSuccess(int response_code) {
  // NetFetcher only considers success to be if the network request
  // was successful *and* we get a 2xx response back.
  // TODO: 304s are unexpected since we don't enable the HTTP cache,
  // meaning we don't add the If-Modified-Since header to our request.
  // However, it's unclear what would happen if we did, so DCHECK.
  DCHECK_NE(response_code, 304) << "Unsupported status code";
  return response_code / 100 == 2;
}

}  // namespace

GoogleSpeechService::GoogleSpeechService(
    network::NetworkModule* network_module, const EventCallback& event_callback,
    const URLFetcherCreator& fetcher_creator)
    : network_module_(network_module),
      started_(false),
      event_callback_(event_callback),
      fetcher_creator_(fetcher_creator),
      thread_("SpeechRecognizr"),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          weak_this_(weak_ptr_factory_.GetWeakPtr())),
      wrappables_task_runner_(base::MessageLoop::current()->task_runner()) {
  thread_.StartWithOptions(
      base::Thread::Options(base::MessageLoop::TYPE_IO, 0));
}

GoogleSpeechService::~GoogleSpeechService() {
  Stop();
  // Stopping the thread here to ensure that StopInternal has completed before
  // we finish running the destructor.
  thread_.Stop();
}

void GoogleSpeechService::Start(const SpeechRecognitionConfig& config,
                                int sample_rate) {
  // Called by the speech recognition manager thread.
  thread_.message_loop()->task_runner()->PostTask(
      FROM_HERE, base::Bind(&GoogleSpeechService::StartInternal,
                            base::Unretained(this), config, sample_rate));
}

void GoogleSpeechService::Stop() {
  // Called by the speech recognition manager thread.
  thread_.message_loop()->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&GoogleSpeechService::StopInternal, base::Unretained(this)));
}

void GoogleSpeechService::RecognizeAudio(std::unique_ptr<AudioBus> audio_bus,
                                         bool is_last_chunk) {
  // Called by the speech recognition manager thread.
  thread_.message_loop()->task_runner()->PostTask(
      FROM_HERE, base::Bind(&GoogleSpeechService::UploadAudioDataInternal,
                            base::Unretained(this), base::Passed(&audio_bus),
                            is_last_chunk));
}

// TODO: Refactor OnURLFetchDownloadProgress() into a private function that is
//       called by OnURLFetchDownloadProgress() and OnURLFetchComplete(), to
//       explicitly remove the unreferenced parameters.
void GoogleSpeechService::OnURLFetchDownloadProgress(
    const net::URLFetcher* source, int64_t current, int64_t total,
    int64_t current_network_bytes) {
  DCHECK_EQ(thread_.message_loop(), base::MessageLoop::current());
  std::string data;
  download_data_writer_->GetAndResetData(&data);

  const net::URLRequestStatus& status = source->GetStatus();
  const int response_code = source->GetResponseCode();

  if (source == downstream_fetcher_.get()) {
    if (status.is_success() && IsResponseCodeSuccess(response_code)) {
      chunked_byte_buffer_.Append(data);
      while (chunked_byte_buffer_.HasChunks()) {
        std::unique_ptr<std::vector<uint8_t> > chunk =
            chunked_byte_buffer_.PopChunk();

        proto::SpeechRecognitionEvent event;
        if (!event.ParseFromString(std::string(chunk->begin(), chunk->end()))) {
          DLOG(WARNING) << "Parse proto string error.";
          return;
        }

        if (event.status() == proto::SpeechRecognitionEvent::STATUS_SUCCESS) {
          wrappables_task_runner_->PostTask(
              FROM_HERE,
              base::Bind(&GoogleSpeechService::ProcessAndFireSuccessEvent,
                         weak_this_, event));
        } else {
          wrappables_task_runner_->PostTask(
              FROM_HERE,
              base::Bind(&ProcessAndFireErrorEvent, event, event_callback_));
        }
      }
    } else {
      wrappables_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&HandleNetworkResponseFailure, event_callback_));
    }
  }
}

void GoogleSpeechService::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK_EQ(thread_.message_loop(), base::MessageLoop::current());
  if (download_data_writer_->HasData()) {
    // Explicitly pass '-1' for all sizes, as it is not used by
    // OnURLFetchDownloadProgress();
    OnURLFetchDownloadProgress(source, -1, -1, -1);
  }
}

// static
base::Optional<std::string> GoogleSpeechService::GetSpeechAPIKey() {
  const int kSpeechApiKeyLength = 100;
  char buffer[kSpeechApiKeyLength] = {0};
  bool result = SbSystemGetProperty(kSbSystemPropertySpeechApiKey, buffer,
                                    SB_ARRAY_SIZE_INT(buffer));
  if (result) {
    return std::string(buffer);
  } else {
    return base::nullopt;
  }
}

void GoogleSpeechService::StartInternal(const SpeechRecognitionConfig& config,
                                        int sample_rate) {
  DCHECK_EQ(thread_.message_loop(), base::MessageLoop::current());

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

  downstream_fetcher_ =
      fetcher_creator_.Run(down_url, net::URLFetcher::GET, this);
  download_data_writer_ = new loader::URLFetcherStringWriter();
  downstream_fetcher_->SaveResponseWithWriter(
      std::unique_ptr<loader::URLFetcherStringWriter>(download_data_writer_));
  downstream_fetcher_->SetRequestContext(
      network_module_->url_request_context_getter());
  downstream_fetcher_->Start();

  // Up stream.
  GURL up_url(kBaseStreamURL);
  up_url = AppendPath(up_url, kUp);
  up_url = AppendQueryParameter(up_url, "client", kClient);
  up_url = AppendQueryParameter(up_url, "pair", pair);
  up_url = AppendQueryParameter(up_url, "output", "pb");

  base::Optional<std::string> api_key = GetSpeechAPIKey();
  SB_DCHECK(api_key);

  up_url = AppendQueryParameter(up_url, "key", api_key.value_or(""));

  // Language is required. If no language is specified, use the system language.
  if (!config.lang.empty()) {
    up_url = AppendQueryParameter(up_url, "lang", config.lang);
  } else {
    up_url = AppendQueryParameter(up_url, "lang", base::GetSystemLanguage());
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

  upstream_fetcher_ = fetcher_creator_.Run(up_url, net::URLFetcher::POST, this);
  upstream_fetcher_->SetRequestContext(
      network_module_->url_request_context_getter());
  upstream_fetcher_->SetChunkedUpload(encoder_->GetMimeType());
  upstream_fetcher_->Start();
}

void GoogleSpeechService::StopInternal() {
  DCHECK_EQ(thread_.message_loop(), base::MessageLoop::current());

  if (!started_) {
    // Recognizer is not started.
    return;
  }
  started_ = false;

  upstream_fetcher_.reset();
  download_data_writer_ = nullptr;
  downstream_fetcher_.reset();
  encoder_.reset();

  wrappables_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&GoogleSpeechService::ClearFinalResults, weak_this_));
  // Clear any remaining audio data.
  chunked_byte_buffer_.Clear();
}

void GoogleSpeechService::ClearFinalResults() {
  // This method handles wrappables and should run on the MainWebModule thread.
  DCHECK(wrappables_task_runner_->BelongsToCurrentThread());
  final_results_.clear();
}

void GoogleSpeechService::UploadAudioDataInternal(
    std::unique_ptr<AudioBus> audio_bus, bool is_last_chunk) {
  DCHECK_EQ(thread_.message_loop(), base::MessageLoop::current());
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

void GoogleSpeechService::ProcessAndFireSuccessEvent(
    proto::SpeechRecognitionEvent event) {
  // This method handles wrappables and should run on the MainWebModule thread.
  DCHECK(wrappables_task_runner_->BelongsToCurrentThread());

  SpeechRecognitionResults new_results = ProcessProtoSuccessResults(event);

  SpeechRecognitionResults success_results;
  size_t total_size = final_results_.size() + new_results.size();
  success_results.reserve(total_size);
  success_results = final_results_;
  success_results.insert(success_results.end(), new_results.begin(),
                         new_results.end());

  size_t result_index = final_results_.size();
  // Update final results list.
  for (size_t i = 0; i < new_results.size(); ++i) {
    if (new_results[i]->is_final()) {
      final_results_.push_back(new_results[i]);
    }
  }

  scoped_refptr<SpeechRecognitionResultList> recognition_list(
      new SpeechRecognitionResultList(success_results));
  scoped_refptr<SpeechRecognitionEvent> recognition_event(
      new SpeechRecognitionEvent(SpeechRecognitionEvent::kResult,
                                 static_cast<uint32>(result_index),
                                 recognition_list));
  event_callback_.Run(recognition_event);
}

}  // namespace speech
}  // namespace cobalt
