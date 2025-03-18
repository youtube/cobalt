#ifndef COBALT_BROWSER_COBALT_TRUSTED_URL_LOADER_HEADER_CLIENT_H_
#define COBALT_BROWSER_COBALT_TRUSTED_URL_LOADER_HEADER_CLIENT_H_

#include "cobalt/browser/client_hints/cobalt_trusted_header_client.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace cobalt {
namespace browser {

class CobaltTrustedURLLoaderHeaderClient
    : public network::mojom::TrustedURLLoaderHeaderClient {
 public:
  CobaltTrustedURLLoaderHeaderClient() = default;

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

  mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
  GetPendingRemote();

 private:
  mojo::Receiver<network::mojom::TrustedURLLoaderHeaderClient> receiver_{this};
  std::unique_ptr<CobaltTrustedHeaderClient> cobalt_header_client_;
};

}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_COBALT_TRUSTED_URL_LOADER_HEADER_CLIENT_H_
