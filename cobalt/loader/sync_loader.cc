// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/loader/sync_loader.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/debug/trace_event.h"
#include "base/synchronization/waitable_event.h"

namespace cobalt {
namespace loader {
namespace {

//////////////////////////////////////////////////////////////////
// Helpers
//////////////////////////////////////////////////////////////////

class FetcherToDecoderAdapter;

// This class is responsible for loading resource using the given fetcher and
// decoder creators.
class LoaderOnThread {
 public:
  LoaderOnThread() : waitable_event_(false, false) {}

  // Start() and End() should be called on the same thread, the sychronous load
  // thread, so the member objects are created, execute, and are destroyed,
  // on that same thread.
  void Start(
      base::Callback<scoped_ptr<Fetcher>(Fetcher::Handler*)> fetcher_creator,
      base::Callback<scoped_ptr<Decoder>()> decoder_creator,
      base::Callback<void(const std::string&)> error_callback);
  void End();

  void Signal() { waitable_event_.Signal(); }
  void Wait() { waitable_event_.Wait(); }

 private:
  scoped_ptr<Decoder> decoder_;
  scoped_ptr<FetcherToDecoderAdapter> fetcher_to_decoder_adaptor_;
  scoped_ptr<Fetcher> fetcher_;

  base::WaitableEvent waitable_event_;
};

// This class is responsible for passing chunks of data from fetcher to decoder
// and notifying fetching is done or aborted on error.
class FetcherToDecoderAdapter : public Fetcher::Handler {
 public:
  FetcherToDecoderAdapter(
      LoaderOnThread* loader_on_thread, Decoder* decoder,
      base::Callback<void(const std::string&)> error_callback)
      : loader_on_thread_(loader_on_thread),
        decoder_(decoder),
        error_callback_(error_callback) {}

  // From Fetcher::Handler.
  void OnReceived(Fetcher* fetcher, const char* data, size_t size) override {
    UNREFERENCED_PARAMETER(fetcher);
    decoder_->DecodeChunk(data, size);
  }
  void OnDone(Fetcher* fetcher) override {
    DCHECK(fetcher);
    decoder_->SetLastURLOrigin(fetcher->last_url_origin());
    decoder_->Finish();
    loader_on_thread_->Signal();
  }
  void OnError(Fetcher* fetcher, const std::string& error) override {
    UNREFERENCED_PARAMETER(fetcher);
    error_callback_.Run(error);
    loader_on_thread_->Signal();
  }

 private:
  LoaderOnThread* loader_on_thread_;
  Decoder* decoder_;
  base::Callback<void(const std::string&)> error_callback_;
};

void LoaderOnThread::Start(
    base::Callback<scoped_ptr<Fetcher>(Fetcher::Handler*)> fetcher_creator,
    base::Callback<scoped_ptr<Decoder>()> decoder_creator,
    base::Callback<void(const std::string&)> error_callback) {
  decoder_ = decoder_creator.Run();
  fetcher_to_decoder_adaptor_.reset(
      new FetcherToDecoderAdapter(this, decoder_.get(), error_callback));
  fetcher_ = fetcher_creator.Run(fetcher_to_decoder_adaptor_.get());
}

void LoaderOnThread::End() {
  fetcher_.reset();
  fetcher_to_decoder_adaptor_.reset();
  decoder_.reset();
  Signal();
}

}  // namespace

//////////////////////////////////////////////////////////////////
// LoadSynchronously
//////////////////////////////////////////////////////////////////

void LoadSynchronously(
    MessageLoop* message_loop,
    base::Callback<scoped_ptr<Fetcher>(Fetcher::Handler*)> fetcher_creator,
    base::Callback<scoped_ptr<Decoder>()> decoder_creator,
    base::Callback<void(const std::string&)> error_callback) {
  TRACE_EVENT0("cobalt::loader", "LoadSynchronously()");
  DCHECK(message_loop);
  DCHECK(!error_callback.is_null());

  LoaderOnThread loader_on_thread;

  message_loop->PostTask(
      FROM_HERE,
      base::Bind(&LoaderOnThread::Start, base::Unretained(&loader_on_thread),
                 fetcher_creator, decoder_creator, error_callback));
  loader_on_thread.Wait();

  message_loop->PostTask(
      FROM_HERE,
      base::Bind(&LoaderOnThread::End, base::Unretained(&loader_on_thread)));
  loader_on_thread.Wait();
}

}  // namespace loader
}  // namespace cobalt
