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

#ifndef COBALT_NETWORK_NET_POSTER_H_
#define COBALT_NETWORK_NET_POSTER_H_

#include <string>

#include "base/memory/scoped_vector.h"
#include "base/threading/thread_checker.h"
#include "cobalt/network_bridge/net_poster.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"

namespace cobalt {
namespace network {
class NetworkModule;

// Simple class to manage some fire-and-forget POST requests.
class NetPoster : public network_bridge::NetPoster, net::URLFetcherDelegate {
 public:
  explicit NetPoster(NetworkModule* network_module);
  ~NetPoster();

  // POST the given data to the URL. No notification will be given if this
  // succeeds or fails.
  // content_type should reflect the mime type of data, e.g. "application/json".
  // data is the data to upload. May be empty.
  void Send(const GURL& url, const std::string& content_type,
            const std::string& data) override;

 private:
  // From net::URLFetcherDelegate
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  NetworkModule* network_module_;
  ScopedVector<net::URLFetcher> fetchers_;
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(NetPoster);
};

}  // namespace network
}  // namespace cobalt

#endif  // COBALT_NETWORK_NET_POSTER_H_
