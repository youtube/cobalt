// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "cobalt/loader/threaded_net_fetcher_proxy.h"

#include <string>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop_proxy.h"
#include "base/synchronization/waitable_event.h"
#include "cobalt/loader/fetcher.h"
#include "googleurl/src/gurl.h"

namespace {

class EnsureNotUsedHandler : public cobalt::loader::Fetcher::Handler {
 public:
  void OnReceived(cobalt::loader::Fetcher*, const char*, size_t) OVERRIDE {
    NOTREACHED();
  }
  void OnDone(cobalt::loader::Fetcher*) OVERRIDE { NOTREACHED(); }
  void OnError(cobalt::loader::Fetcher*, const std::string&) OVERRIDE {
    NOTREACHED();
  }
};

EnsureNotUsedHandler not_used_handler;
}  // namespace

namespace cobalt {
namespace loader {

ThreadedNetFetcherProxy::ThreadedNetFetcherProxy(
    const GURL& url, const csp::SecurityCallback& security_callback,
    Fetcher::Handler* handler, const network::NetworkModule* network_module,
    const NetFetcher::Options& options, MessageLoop* fetch_message_loop)
    : Fetcher(&not_used_handler), fetch_message_loop_(fetch_message_loop) {
  DCHECK(fetch_message_loop);

  // Creating a NetFetcher on the fetch_message_loop will cause the completition
  // callbacks to be called on the fetch_message_loop. This is because
  // NetFetcher class creates a net::UrlFetcher object, and it uses the
  // message loop was active during construction for future callbacks.

  // The following few lines of code just make sure that a NetFetcher is
  // created on fetch_message_loop.
  ConstructorParams params(url, security_callback, handler, network_module,
                           options);

  base::WaitableEvent fetcher_created_event(true, false);

  base::Closure create_fetcher_closure(base::Bind(
      &ThreadedNetFetcherProxy::CreateNetFetcher, base::Unretained(this),
      params, base::Unretained(&fetcher_created_event)));
  fetch_message_loop_->message_loop_proxy()->PostTask(FROM_HERE,
                                                      create_fetcher_closure);

  fetcher_created_event.Wait();
}

void ThreadedNetFetcherProxy::CreateNetFetcher(
    const ConstructorParams& params,
    base::WaitableEvent* fetcher_created_event) {
  net_fetcher_.reset(new NetFetcher(params.url_, params.security_callback_,
                                    params.handler_, params.network_module_,
                                    params.options_));

  if (fetcher_created_event) {
    fetcher_created_event->Signal();
  }
}

// We're dying, but |net_fetcher_| might still be doing work on the load
// thread.  Because of this, we transfer ownership of it into the fetch message
// loop, where it will be deleted after any pending tasks involving it are
// done.  This case can easily happen e.g. if a we navigate to a different page.
ThreadedNetFetcherProxy::~ThreadedNetFetcherProxy() {
  if (net_fetcher_) {
    fetch_message_loop_->DeleteSoon(FROM_HERE, net_fetcher_.release());

    // Wait for all fetcher lifetime thread messages to be flushed before
    // returning.
    base::WaitableEvent messages_flushed(true, false);
    fetch_message_loop_->PostTask(
        FROM_HERE, base::Bind(&base::WaitableEvent::Signal,
                              base::Unretained(&messages_flushed)));
    messages_flushed.Wait();
  }
}

}  // namespace loader
}  // namespace cobalt
