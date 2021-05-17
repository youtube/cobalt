// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/loader/loader.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"

namespace cobalt {
namespace loader {

//////////////////////////////////////////////////////////////////
// Loader::FetcherHandlerToDecoderAdapter
//////////////////////////////////////////////////////////////////

// This class is responsible for passing chunks of data from fetcher to
// decoder and notifying fetching is done or aborted on error.
class Loader::FetcherHandlerToDecoderAdapter : public Fetcher::Handler {
 public:
  FetcherHandlerToDecoderAdapter(Decoder* decoder,
                                 OnCompleteFunction load_complete_callback)
      : decoder_(decoder), load_complete_callback_(load_complete_callback) {}

  // From Fetcher::Handler.
  LoadResponseType OnResponseStarted(
      Fetcher* fetcher,
      const scoped_refptr<net::HttpResponseHeaders>& headers) override {
    if (headers) {
      return decoder_->OnResponseStarted(fetcher, headers);
    } else {
      return kLoadResponseContinue;
    }
  }

  void OnReceived(Fetcher* fetcher, const char* data, size_t size) override {
    decoder_->DecodeChunk(data, size);
  }
  void OnReceivedPassed(Fetcher* fetcher,
                        std::unique_ptr<std::string> data) override {
    decoder_->DecodeChunkPassed(std::move(data));
  }
  void OnDone(Fetcher* fetcher) override {
    DCHECK(fetcher);
    decoder_->SetLastURLOrigin(fetcher->last_url_origin());
    decoder_->Finish();
  }
  void OnError(Fetcher* fetcher, const std::string& error) override {
    load_complete_callback_.Run(error);
  }

 private:
  Decoder* decoder_;
  OnCompleteFunction load_complete_callback_;
};

//////////////////////////////////////////////////////////////////
// Loader
//////////////////////////////////////////////////////////////////

Loader::Loader(const FetcherCreator& fetcher_creator,
               const DecoderCreator& decoder_creator,
               const OnCompleteFunction& on_load_complete,
               const OnDestructionFunction& on_destruction, bool is_suspended)
    : fetcher_creator_(fetcher_creator),
      decoder_creator_(decoder_creator),
      on_load_complete_(on_load_complete),
      on_destruction_(on_destruction),
      is_suspended_(is_suspended) {
  DCHECK(!fetcher_creator_.is_null());
  DCHECK(!decoder_creator_.is_null());
  DCHECK(!on_load_complete_.is_null());

  load_timing_info_ = net::LoadTimingInfo();

  std::unique_ptr<Decoder> decoder = decoder_creator_.Run(
      base::Bind(&Loader::LoadComplete, base::Unretained(this)));
  // We are doing this bizarre check because decoder_ used to be a scoped_ptr
  // and scoped_ptr checks equality of held raw pointer and only delete rhs if
  // not equal.
  if (decoder.get() == decoder_.get()) {
    decoder_.release();
  }
  decoder_ = std::move(decoder);

  if (!is_suspended_) {
    Start();
  }
}

Loader::~Loader() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!on_destruction_.is_null()) {
    on_destruction_.Run(this);
  }

  fetcher_creator_error_closure_.Cancel();
}

void Loader::Suspend() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!is_suspended_);

  is_suspended_ = true;

  bool suspendable = decoder_->Suspend();
  if (fetcher_) {
    fetcher_.reset();
  }

  fetcher_handler_to_decoder_adaptor_.reset();
  fetcher_creator_error_closure_.Cancel();

  if (!suspendable) {
    LoadComplete(std::string("Aborted."));
  }
}

void Loader::Resume(render_tree::ResourceProvider* resource_provider) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(is_suspended_);

  is_suspended_ = false;

  decoder_->Resume(resource_provider);
  if (!is_load_complete_) Start();
}

bool Loader::DidFailFromTransientError() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return fetcher_ && fetcher_->did_fail_from_transient_error();
}

void Loader::LoadComplete(const base::Optional<std::string>& error) {
  is_load_complete_ = true;
  on_load_complete_.Run(error);
}

void Loader::Start() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!is_suspended_);

  fetcher_handler_to_decoder_adaptor_.reset(new FetcherHandlerToDecoderAdapter(
      decoder_.get(),
      base::Bind(&Loader::LoadComplete, base::Unretained(this))));
  fetcher_ = fetcher_creator_.Run(fetcher_handler_to_decoder_adaptor_.get());

  if (fetcher_) {
    fetcher_->SetLoadTimingInfoCallback(base::Bind(&Loader::set_load_timing_info,
                                                   base::Unretained(this)));
  }

  // Post the error callback on the current message loop in case the loader is
  // destroyed in the callback.
  if (!fetcher_) {
    fetcher_creator_error_closure_.Reset(
        base::Bind(base::Bind(&Loader::LoadComplete, base::Unretained(this)),
                   std::string("Fetcher was not created.")));
    base::MessageLoop::current()->task_runner()->PostTask(
        FROM_HERE, fetcher_creator_error_closure_.callback());
  }
}

void Loader::set_load_timing_info(const net::LoadTimingInfo& timing_info) {
  load_timing_info_ = timing_info;
}

net::LoadTimingInfo Loader::get_load_timing_info() {
  return load_timing_info_;
}

}  // namespace loader
}  // namespace cobalt
