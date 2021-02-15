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

#include "cobalt/loader/sync_loader.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/synchronization/waitable_event.h"
#include "base/trace_event/trace_event.h"

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
  LoaderOnThread(base::Callback<void(const base::Optional<std::string>&)>
                     load_complete_callback)
      : load_complete_callback_(load_complete_callback),
        load_complete_waitable_event_(
            base::WaitableEvent::ResetPolicy::MANUAL,
            base::WaitableEvent::InitialState::NOT_SIGNALED),
        end_waitable_event_(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED) {}

  // Start() and End() should be called on the same thread, the synchronous load
  // thread, so the member objects are created, execute, and are destroyed,
  // on that same thread.
  void Start(base::Callback<std::unique_ptr<Fetcher>(Fetcher::Handler*)>
                 fetcher_creator,
             base::Callback<std::unique_ptr<Decoder>()> decoder_creator);
  void End();

  void SignalLoadComplete(const base::Optional<std::string>& error) {
    load_complete_callback_.Run(error);
    load_complete_waitable_event_.Signal();
  }
  void SignalEndDone() { end_waitable_event_.Signal(); }

  // Returns true if start completed normally, returns false if the wait was
  // interrupted by |interrupt_event|.
  void WaitForLoad(base::WaitableEvent* interrupt_event) {
    base::WaitableEvent* event_array[] = {&load_complete_waitable_event_,
                                          interrupt_event};
    size_t effective_size = arraysize(event_array);
    if (!interrupt_event) {
      --effective_size;
    }
    base::WaitableEvent::WaitMany(event_array, effective_size);
  }

  void WaitForEnd() { end_waitable_event_.Wait(); }

 private:
  base::Callback<void(const base::Optional<std::string>&)>
      load_complete_callback_;

  std::unique_ptr<Decoder> decoder_;
  std::unique_ptr<FetcherToDecoderAdapter> fetcher_to_decoder_adaptor_;
  std::unique_ptr<Fetcher> fetcher_;

  base::WaitableEvent load_complete_waitable_event_;
  base::WaitableEvent end_waitable_event_;
};

// This class is responsible for passing chunks of data from fetcher to decoder
// and notifying fetching is done or aborted on error.
class FetcherToDecoderAdapter : public Fetcher::Handler {
 public:
  FetcherToDecoderAdapter(
      LoaderOnThread* loader_on_thread, Decoder* decoder,
      base::Callback<void(const base::Optional<std::string>&)>
          load_complete_callback)
      : loader_on_thread_(loader_on_thread), decoder_(decoder) {}

  // From Fetcher::Handler.
  void OnReceived(Fetcher* fetcher, const char* data, size_t size) override {
    decoder_->DecodeChunk(data, size);
  }
  void OnDone(Fetcher* fetcher) override {
    DCHECK(fetcher);
    decoder_->SetLastURLOrigin(fetcher->last_url_origin());
    decoder_->Finish();
    loader_on_thread_->SignalLoadComplete(base::nullopt);
  }
  void OnError(Fetcher* fetcher, const std::string& error) override {
    loader_on_thread_->SignalLoadComplete(error);
  }

 private:
  LoaderOnThread* loader_on_thread_;
  Decoder* decoder_;
};

void LoaderOnThread::Start(
    base::Callback<std::unique_ptr<Fetcher>(Fetcher::Handler*)> fetcher_creator,
    base::Callback<std::unique_ptr<Decoder>()> decoder_creator) {
  decoder_ = decoder_creator.Run();
  fetcher_to_decoder_adaptor_.reset(new FetcherToDecoderAdapter(
      this, decoder_.get(), load_complete_callback_));
  fetcher_ = fetcher_creator.Run(fetcher_to_decoder_adaptor_.get());
}

void LoaderOnThread::End() {
  fetcher_.reset();
  fetcher_to_decoder_adaptor_.reset();
  decoder_.reset();

  if (!load_complete_waitable_event_.IsSignaled()) {
    // We were interrupted, and we did not complete.  Call the
    // |load_complete_callback| "manually" to indicate this.
    load_complete_callback_.Run(
        "Synchronous load was interrupted (probably due to the process "
        "suspending).  This probably implies that there is a suspend/resume "
        "bug (are you using inlined <script> tags?  If so, switch to loading "
        "scripts via JavaScript e.g. by using "
        "document.createElement('script')).");
  }

  SignalEndDone();
}

}  // namespace

//////////////////////////////////////////////////////////////////
// LoadSynchronously
//////////////////////////////////////////////////////////////////

void LoadSynchronously(
    base::MessageLoop* message_loop, base::WaitableEvent* interrupt_trigger,
    base::Callback<std::unique_ptr<Fetcher>(Fetcher::Handler*)> fetcher_creator,
    base::Callback<std::unique_ptr<Decoder>()> decoder_creator,
    base::Callback<void(const base::Optional<std::string>&)>
        load_complete_callback) {
  TRACE_EVENT0("cobalt::loader", "LoadSynchronously()");
  DCHECK(message_loop);
  DCHECK(!load_complete_callback.is_null());

  LoaderOnThread loader_on_thread(load_complete_callback);

  message_loop->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&LoaderOnThread::Start, base::Unretained(&loader_on_thread),
                 fetcher_creator, decoder_creator));
  loader_on_thread.WaitForLoad(interrupt_trigger);

  message_loop->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&LoaderOnThread::End, base::Unretained(&loader_on_thread)));

  // Wait for a different event here, since it is possible that the first
  // wait was interrupted, and the fetcher completion can still |Signal()|
  // the start event.
  loader_on_thread.WaitForEnd();
}

}  // namespace loader
}  // namespace cobalt
