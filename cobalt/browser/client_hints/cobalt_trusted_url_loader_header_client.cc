#include "cobalt/browser/client_hints/cobalt_trusted_url_loader_header_client.h"

namespace cobalt {
namespace browser {

CobaltTrustedURLLoaderHeaderClient::CobaltTrustedURLLoaderHeaderClient() =
    default;

CobaltTrustedURLLoaderHeaderClient::~CobaltTrustedURLLoaderHeaderClient() =
    default;

void CobaltTrustedURLLoaderHeaderClient::OnLoaderCreated(
    int32 request_id,
    mojo::PendingReceiver<network::mojom::TrustedHeaderClient> header_client) {
  auto my_header_client = std::make_unique<CobaltTrustedHeaderClient>();
  mojo::Receiver<network::mojom::TrustedHeaderClient> receiver(
      my_header_client.get());
  receiver.Bind(std::move(header_client));
  my_header_client.release();
}

void CobaltTrustedURLLoaderHeaderClient::OnLoaderForCorsPreflightCreated(
    const network::ResourceRequest& request,
    mojo::PendingReceiver<network::mojom::TrustedHeaderClient> header_client) {
  OnLoaderCreated(0, std::move(header_client));
}

}  // namespace browser
}  // namespace cobalt
