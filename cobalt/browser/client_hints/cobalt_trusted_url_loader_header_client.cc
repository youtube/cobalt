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
  // Bind the header_client to the receiver
  cobalt_header_client_->BindReceiver(std::move(receiver));
}

void CobaltTrustedURLLoaderHeaderClient::OnLoaderForCorsPreflightCreated(
    const network::ResourceRequest& request,
    mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver) {}

}  // namespace browser
}  // namespace cobalt
