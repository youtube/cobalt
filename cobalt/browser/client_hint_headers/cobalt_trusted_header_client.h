// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_BROWSER_CLIENT_HINT_HEADERS_COBALT_TRUSTED_HEADER_CLIENT_H_
#define COBALT_BROWSER_CLIENT_HINT_HEADERS_COBALT_TRUSTED_HEADER_CLIENT_H_

#include "mojo/public/cpp/bindings/receiver.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace cobalt {
namespace browser {

// Responsible for handling trusted headers for individual network requests.
// This class is used to add Cobalt-specific client hint headers before a
// request is sent.
class CobaltTrustedHeaderClient : public network::mojom::TrustedHeaderClient {
 public:
  CobaltTrustedHeaderClient();

  CobaltTrustedHeaderClient(const CobaltTrustedHeaderClient&) = delete;
  CobaltTrustedHeaderClient& operator=(const CobaltTrustedHeaderClient&) =
      delete;

  ~CobaltTrustedHeaderClient() override = default;

  // network::mojom::TrustedHeaderClient:
  void OnBeforeSendHeaders(const net::HttpRequestHeaders& headers,
                           OnBeforeSendHeadersCallback callback) override;
  void OnHeadersReceived(const std::string& headers,
                         const net::IPEndPoint& endpoint,
                         OnHeadersReceivedCallback callback) override;
};

}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_CLIENT_HINT_HEADERS_COBALT_TRUSTED_HEADER_CLIENT_H_
