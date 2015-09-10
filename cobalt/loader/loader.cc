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

namespace cobalt {
namespace loader {

//////////////////////////////////////////////////////////////////
// Loader::DecoderToFetcherAdapter
//////////////////////////////////////////////////////////////////

// This class is responsible for passing chunks of data from fetcher to
// decoder and notifying fetching is done or aborted on error.
class Loader::DecoderToFetcherAdapter : public Fetcher::Handler {
 public:
  DecoderToFetcherAdapter(Decoder* decoder, ErrorCallback error_callback)
      : decoder_(decoder), error_callback_(error_callback) {
    DCHECK(decoder);
    DCHECK(!error_callback_.is_null());
  }
  void OnReceived(const char* data, size_t size) {
    decoder_->DecodeChunk(data, size);
  }
  void OnDone() { decoder_->Finish(); }
  void OnError(const std::string& error) { error_callback_.Run(error); }

 private:
  Decoder* decoder_;
  ErrorCallback error_callback_;
};

//////////////////////////////////////////////////////////////////
// Loader
//////////////////////////////////////////////////////////////////

Loader::Loader(
    base::Callback<scoped_ptr<Fetcher>(Fetcher::Handler*)> fetcher_creator,
    scoped_ptr<Decoder> decoder, ErrorCallback error_callback)
    : decoder_(decoder.Pass()),
      decoder_to_fetcher_adaptor_(
          new DecoderToFetcherAdapter(decoder_.get(), error_callback)),
      fetcher_(fetcher_creator.Run(decoder_to_fetcher_adaptor_.get()).Pass()) {
  static const char kLoaderNotCreated[] = "Fetcher or Decoder is not created.";
  if (!fetcher_ || !decoder_) {
    error_callback.Run(kLoaderNotCreated);
  }
}

Loader::~Loader() {}

}  // namespace loader
}  // namespace cobalt
