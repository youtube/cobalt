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

#include "cobalt/browser/client_hint_headers/cobalt_trusted_header_client.h"

#include <optional>

#include "cobalt/browser/client_hint_headers/cobalt_header_value_provider.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"

namespace cobalt {
namespace browser {

CobaltTrustedHeaderClient::CobaltTrustedHeaderClient() = default;

void CobaltTrustedHeaderClient::OnBeforeSendHeaders(
    const ::net::HttpRequestHeaders& headers,
    OnBeforeSendHeadersCallback callback) {
  ::net::HttpRequestHeaders mutable_headers(headers);
  auto header_value_provider =
      cobalt::browser::CobaltHeaderValueProvider::GetInstance();
  for (const auto& pair : header_value_provider->GetHeaderValues()) {
    mutable_headers.SetHeader(pair.first, pair.second);
  }
  std::move(callback).Run(net::OK, mutable_headers);
}

void CobaltTrustedHeaderClient::OnHeadersReceived(
    const std::string& headers,
    const net::IPEndPoint& remote_endpoint,
    OnHeadersReceivedCallback callback) {
  // Cobalt does not currently need to act on response headers, so this is a
  // no-op.
  std::move(callback).Run(net::OK, std::nullopt, std::nullopt);
}

}  // namespace browser
}  // namespace cobalt
