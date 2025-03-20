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

#include "cobalt/browser/client_hint_headers/cobalt_trusted_url_loader_header_client.h"

namespace cobalt {
namespace browser {

CobaltTrustedURLLoaderHeaderClient::CobaltTrustedURLLoaderHeaderClient(
    mojo::PendingReceiver<network::mojom::TrustedURLLoaderHeaderClient>
        receiver)
    : receiver_(this, std::move(receiver)) {}

void CobaltTrustedURLLoaderHeaderClient::OnLoaderCreated(
    int32_t request_id,
    mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver) {
  CreateAndBindCobaltTrustedHeaderClient(std::move(receiver));
}

void CobaltTrustedURLLoaderHeaderClient::OnLoaderForCorsPreflightCreated(
    const network::ResourceRequest& request,
    mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver) {
  CreateAndBindCobaltTrustedHeaderClient(std::move(receiver));
}

void CobaltTrustedURLLoaderHeaderClient::CreateAndBindCobaltTrustedHeaderClient(
    mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver) {
  auto client =
      std::make_unique<browser::CobaltTrustedHeaderClient>(std::move(receiver));
  cobalt_header_clients_.push_back(std::move(client));
}

}  // namespace browser
}  // namespace cobalt
