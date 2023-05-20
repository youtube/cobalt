// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_NETWORK_CLIENT_HINT_HEADERS_H_
#define COBALT_NETWORK_CLIENT_HINT_HEADERS_H_

#include "cobalt/network/url_request_context.h"
#include "net/url_request/url_fetcher.h"

namespace cobalt {
namespace network {

// Adds Client Hint Headers in URL Context to a URLFetcher object.
inline void SetClientHintHeaders(
    const cobalt::network::URLRequestContext* const request_context,
    net::URLFetcher& url_fetcher) {
  if (request_context) {
    for (const auto& header : request_context->client_hint_headers()) {
      url_fetcher.AddExtraRequestHeader(header);
    }
  }
}

}  // namespace network
}  // namespace cobalt

#endif  // COBALT_NETWORK_CLIENT_HINT_HEADERS_H_
