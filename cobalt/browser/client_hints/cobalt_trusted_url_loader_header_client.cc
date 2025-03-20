#include "cobalt/browser/client_hints/cobalt_trusted_url_loader_header_client.h"

namespace cobalt {
namespace browser {

CobaltTrustedURLLoaderHeaderClient::CobaltTrustedURLLoaderHeaderClient(
    mojo::PendingReceiver<network::mojom::TrustedURLLoaderHeaderClient>
        receiver)
    : receiver_(this, std::move(receiver)) {}

void CobaltTrustedURLLoaderHeaderClient::OnLoaderCreated(
    int32_t request_id,
    mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver) {
  auto cobalt_header_client =
      std::make_unique<browser::CobaltTrustedHeaderClient>();
  cobalt_header_client->BindReceiver(std::move(receiver));
  cobalt_header_clients_.push_back(std::move(cobalt_header_client));
}

void CobaltTrustedURLLoaderHeaderClient::OnLoaderForCorsPreflightCreated(
    const network::ResourceRequest& request,
    mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver) {
  auto cobalt_header_client =
      std::make_unique<browser::CobaltTrustedHeaderClient>();
  cobalt_header_client->BindReceiver(std::move(receiver));
  cobalt_header_clients_.push_back(std::move(cobalt_header_client));
}

}  // namespace browser
}  // namespace cobalt
