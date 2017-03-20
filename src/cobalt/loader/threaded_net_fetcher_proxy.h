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

#ifndef COBALT_LOADER_THREADED_NET_FETCHER_PROXY_H_
#define COBALT_LOADER_THREADED_NET_FETCHER_PROXY_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "cobalt/loader/fetcher.h"
#include "cobalt/loader/net_fetcher.h"
#include "net/url_request/url_fetcher.h"

namespace cobalt {
namespace loader {

// This class is similar to a NetFetcher, but has one critical difference:
// Callbacks to the |handler| can be done on a separate message loop:
// |fetch_message_loop|.
class ThreadedNetFetcherProxy
    : public Fetcher,
      public base::SupportsWeakPtr<ThreadedNetFetcherProxy> {
 public:
  struct ConstructorParams {
    ConstructorParams(const GURL& url,
                      const csp::SecurityCallback& security_callback,
                      Fetcher::Handler* handler,
                      const network::NetworkModule* network_module,
                      const NetFetcher::Options& options)
        : url_(url),
          security_callback_(security_callback),
          handler_(handler),
          network_module_(network_module),
          options_(options) {}

    const GURL url_;
    const csp::SecurityCallback security_callback_;
    Fetcher::Handler* const handler_;
    const network::NetworkModule* network_module_;
    const NetFetcher::Options options_;
  };

  // Note: |handler| will be called on the fetch_message_loop.
  // The other arguments mimic NetFetcher's arguments.
  ThreadedNetFetcherProxy(const GURL& url,
                          const csp::SecurityCallback& security_callback,
                          Fetcher::Handler* handler,
                          const network::NetworkModule* network_module,
                          const NetFetcher::Options& options,
                          MessageLoop* fetch_message_loop);
  virtual ~ThreadedNetFetcherProxy() OVERRIDE;

 private:
  Fetcher::Handler* handler_;
  scoped_ptr<NetFetcher> net_fetcher_;
  MessageLoop* fetch_message_loop_;

  void CreateNetFetcher(const ConstructorParams& params,
                        base::WaitableEvent* fetcher_created_event);

  DISALLOW_COPY_AND_ASSIGN(ThreadedNetFetcherProxy);
};

}  // namespace loader
}  // namespace cobalt

#endif  // COBALT_LOADER_THREADED_NET_FETCHER_PROXY_H_
