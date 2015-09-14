/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/loader/loader.h"

#include "base/logging.h"

namespace cobalt {
namespace loader {

//////////////////////////////////////////////////////////////////
// Loader::FetcherToDecoderAdapter
//////////////////////////////////////////////////////////////////

// This class is responsible for passing chunks of data from fetcher to
// decoder and notifying fetching is done or aborted on error.
class Loader::FetcherToDecoderAdapter : public Fetcher::Handler {
 public:
  FetcherToDecoderAdapter(
      Decoder* decoder, base::Callback<void(const std::string&)> error_callback)
      : decoder_(decoder), error_callback_(error_callback) {}

  // From Fetcher::Handler.
  void OnReceived(const char* data, size_t size) OVERRIDE {
    decoder_->DecodeChunk(data, size);
  }
  void OnDone() OVERRIDE { decoder_->Finish(); }
  void OnError(const std::string& error) OVERRIDE {
    error_callback_.Run(error);
  }

 private:
  Decoder* decoder_;
  base::Callback<void(const std::string&)> error_callback_;
};

//////////////////////////////////////////////////////////////////
// Loader
//////////////////////////////////////////////////////////////////

Loader::Loader(
    base::Callback<scoped_ptr<Fetcher>(Fetcher::Handler*)> fetcher_creator,
    scoped_ptr<Decoder> decoder,
    base::Callback<void(const std::string&)> error_callback)
    : decoder_(decoder.Pass()),
      fetcher_to_decoder_adaptor_(
          new FetcherToDecoderAdapter(decoder_.get(), error_callback)),
      fetcher_(fetcher_creator.Run(fetcher_to_decoder_adaptor_.get())) {
  DCHECK(fetcher_);
  DCHECK(decoder_);
  DCHECK(!error_callback.is_null());
}

Loader::~Loader() {}

}  // namespace loader
}  // namespace cobalt
