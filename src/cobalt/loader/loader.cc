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

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop.h"

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
  LoadResponseType OnResponseStarted(
      Fetcher* fetcher,
      const scoped_refptr<net::HttpResponseHeaders>& headers) OVERRIDE {
    if (headers) {
      return decoder_->OnResponseStarted(fetcher, headers);
    } else {
      return kLoadResponseContinue;
    }
  }

  void OnReceived(Fetcher* fetcher, const char* data, size_t size) OVERRIDE {
    UNREFERENCED_PARAMETER(fetcher);
    decoder_->DecodeChunk(data, size);
  }
  void OnDone(Fetcher* fetcher) OVERRIDE {
    UNREFERENCED_PARAMETER(fetcher);
    decoder_->Finish();
  }
  void OnError(Fetcher* fetcher, const std::string& error) OVERRIDE {
    UNREFERENCED_PARAMETER(fetcher);
    error_callback_.Run(error);
  }

 private:
  Decoder* decoder_;
  base::Callback<void(const std::string&)> error_callback_;
};

//////////////////////////////////////////////////////////////////
// Loader
//////////////////////////////////////////////////////////////////

Loader::Loader(const FetcherCreator& fetcher_creator,
               scoped_ptr<Decoder> decoder,
               const base::Callback<void(const std::string&)>& error_callback,
               const OnDestructionFunction& on_destruction)
    : decoder_(decoder.Pass()),
      fetcher_to_decoder_adaptor_(
          new FetcherToDecoderAdapter(decoder_.get(), error_callback)),
      fetcher_(fetcher_creator.Run(fetcher_to_decoder_adaptor_.get())),
      on_destruction_(on_destruction) {
  DCHECK(decoder_);
  DCHECK(!error_callback.is_null());
  DCHECK(!fetcher_creator.is_null());

  // Post the error callback on the current message loop in case loader is
  // destroyed in the callback.
  if (!fetcher_) {
    fetcher_creator_error_closure_.Reset(
        base::Bind(error_callback, "Fetcher was not created."));
    MessageLoop::current()->PostTask(FROM_HERE,
                                     fetcher_creator_error_closure_.callback());
  }
}

Loader::~Loader() {
  if (!on_destruction_.is_null()) {
    on_destruction_.Run(this);
  }

  DCHECK(thread_checker_.CalledOnValidThread());
  fetcher_creator_error_closure_.Cancel();
}

void Loader::Abort() { decoder_->Abort(); }

}  // namespace loader
}  // namespace cobalt
