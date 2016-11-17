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

#include "cobalt/speech/sandbox/speech_sandbox.h"

#include "cobalt/speech/sandbox/wav_decoder.h"

namespace cobalt {
namespace speech {
namespace sandbox {

namespace {
// The maximum number of element depth in the DOM tree. Elements at a level
// deeper than this could be discarded, and will not be rendered.
const int kDOMMaxElementDepth = 32;
}  // namespace

SpeechSandbox::SpeechSandbox(const GURL& url, const FilePath& trace_log_path)
    : audio_data_size_(0) {
  trace_to_file_.reset(new trace_event::ScopedTraceToFile(trace_log_path));
  network::NetworkModule::Options network_options;
  network_options.require_https = false;

  network_module_.reset(new network::NetworkModule(network_options));
  fetcher_factory_.reset(new loader::FetcherFactory(network_module_.get()));

  loader_ = make_scoped_ptr(new loader::Loader(
      base::Bind(&loader::FetcherFactory::CreateSecureFetcher,
                 base::Unretained(fetcher_factory_.get()), url,
                 csp::SecurityCallback()),
      scoped_ptr<loader::Decoder>(new WAVDecoder(
          base::Bind(&SpeechSandbox::OnLoadingDone, base::Unretained(this)))),
      base::Bind(&SpeechSandbox::OnLoadingError, base::Unretained(this))));
}

SpeechSandbox::~SpeechSandbox() {
  if (speech_recognition_) {
    speech_recognition_->Stop();
  }
}

void SpeechSandbox::OnLoadingDone(scoped_array<uint8> data, int size) {
  audio_data_ = data.Pass();
  audio_data_size_ = size;

  dom::DOMSettings::Options dom_settings_options;
  dom_settings_options.microphone_options.enable_fake_microphone = true;
  dom_settings_options.microphone_options.external_audio_data =
      audio_data_.get();
  dom_settings_options.microphone_options.audio_data_size = audio_data_size_;

  environment_settings_.reset(new dom::DOMSettings(
      kDOMMaxElementDepth, fetcher_factory_.get(), network_module_.get(), NULL,
      NULL, NULL, NULL, NULL, NULL, dom_settings_options));
  DCHECK(environment_settings_);

  speech_recognition_ = new SpeechRecognition(environment_settings_.get());
  speech_recognition_->set_continuous(true);
  speech_recognition_->set_interim_results(true);
  speech_recognition_->Start(NULL);
}

void SpeechSandbox::OnLoadingError(const std::string& error) {
  DLOG(WARNING) << "OnLoadingError with error message: " << error;
}

}  // namespace sandbox
}  // namespace speech
}  // namespace cobalt
