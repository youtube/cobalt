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

#ifndef COBALT_BROWSER_CLIENT_HINT_HEADERS_COBALT_TRUSTED_URL_LOADER_HEADER_CLIENT_H_
#define COBALT_BROWSER_CLIENT_HINT_HEADERS_COBALT_TRUSTED_URL_LOADER_HEADER_CLIENT_H_

#include "cobalt/browser/client_hint_headers/cobalt_trusted_header_client.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace cobalt {
namespace browser {

// This class acts as a factory for creating CobaltTrustedHeaderClient instances
// for each new URL loader. It is responsible for binding the Mojo receiver
// for the TrustedHeaderClient interface.
class CobaltTrustedURLLoaderHeaderClient
    : public network::mojom::TrustedURLLoaderHeaderClient {
 public:
  CobaltTrustedURLLoaderHeaderClient();

  CobaltTrustedURLLoaderHeaderClient(
      const CobaltTrustedURLLoaderHeaderClient&) = delete;
  CobaltTrustedURLLoaderHeaderClient& operator=(
      const CobaltTrustedURLLoaderHeaderClient&) = delete;

  ~CobaltTrustedURLLoaderHeaderClient() override = default;

  // network::mojom::TrustedURLLoaderHeaderClient:
  void OnLoaderCreated(
      int32_t request_id,
      mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver)
      override;
  void OnLoaderForCorsPreflightCreated(
      const network::ResourceRequest& request,
      mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver)
      override;

 private:
  void CreateAndBindCobaltTrustedHeaderClient(
      mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver);
};

}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_CLIENT_HINT_HEADERS_COBALT_TRUSTED_URL_LOADER_HEADER_CLIENT_H_
