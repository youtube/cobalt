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

#ifndef LOADER_NET_FETCHER_H_
#define LOADER_NET_FETCHER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop_proxy.h"
#include "base/threading/thread_checker.h"
#include "cobalt/loader/fetcher.h"
#include "cobalt/network/network_module.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"

namespace cobalt {
namespace loader {

// NetFetcher is for fetching data from the network.
class NetFetcher : public Fetcher, public net::URLFetcherDelegate {
 public:
  struct Options {
   public:
    Options() : request_method(net::URLFetcher::GET) {}
    net::URLFetcher::RequestType request_method;
    std::string request_headers;
    std::string request_body;
  };

  NetFetcher(const GURL& url, Handler* handler,
             const network::NetworkModule* network_module,
             const Options& options);
  ~NetFetcher() OVERRIDE;

  // net::URLFetcherDelegate interface
  void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  net::URLFetcher* url_fetcher() const { return url_fetcher_.get(); }

 private:
  // Thread checker ensures all calls to the NetFetcher are made from the same
  // thread that it is created in.
  base::ThreadChecker thread_checker_;
  scoped_ptr<net::URLFetcher> url_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(NetFetcher);
};

}  // namespace loader
}  // namespace cobalt

#endif  // LOADER_NET_FETCHER_H_
