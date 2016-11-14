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
               scoped_ptr<Decoder> decoder, const OnErrorFunction& on_error,
               const OnDestructionFunction& on_destruction)
    : fetcher_creator_(fetcher_creator),
      decoder_(decoder.Pass()),
      on_error_(on_error),
      on_destruction_(on_destruction),
      suspended_(false) {
  DCHECK(decoder_);
  DCHECK(!on_error.is_null());

  Start();

  // Post the error callback on the current message loop in case loader is
  // destroyed in the callback.
  if (!fetcher_) {
    fetcher_creator_error_closure_.Reset(
        base::Bind(on_error, "Fetcher was not created."));
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

void Loader::Suspend() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (suspended_) {
    return;
  }

  bool suspendable = decoder_->Suspend();
  if (fetcher_) {
    fetcher_.reset();
  }

  fetcher_to_decoder_adaptor_.reset();
  fetcher_creator_error_closure_.Cancel();
  suspended_ = true;

  if (!suspendable) {
    on_error_.Run("Aborted.");
  }
}

void Loader::Resume(render_tree::ResourceProvider* resource_provider) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!suspended_) {
    return;
  }
  suspended_ = false;
  decoder_->Resume(resource_provider);
  Start();
}

void Loader::Start() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(decoder_);
  DCHECK(!fetcher_creator_.is_null());
  fetcher_to_decoder_adaptor_.reset(
      new FetcherToDecoderAdapter(decoder_.get(), on_error_));
  fetcher_ = fetcher_creator_.Run(fetcher_to_decoder_adaptor_.get());
}

}  // namespace loader
}  // namespace cobalt
