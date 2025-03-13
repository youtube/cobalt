#ifndef COBALT_BROWSER_COBALT_TRUSTED_URL_LOADER_HEADER_CLIENT_H_
#define COBALT_BROWSER_COBALT_TRUSTED_URL_LOADER_HEADER_CLIENT_H_

#include "mojo/public/cpp/bindings/receiver.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_util.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace cobalt {
namespace browser {

class CobaltTrustedURLLoaderHeaderClient
    : public network::mojom::TrustedURLLoaderHeaderClient {
 public:
  CobaltTrustedURLLoaderHeaderClient();

  CobaltTrustedURLLoaderHeaderClient(
      const CobaltTrustedURLLoaderHeaderClient&) = delete;
  CobaltTrustedURLLoaderHeaderClient& operator=(
      const CobaltTrustedURLLoaderHeaderClient&) = delete;

  ~CobaltTrustedURLLoaderHeaderClient() override;

  void OnLoaderCreated(
      int32 request_id,
      mojo::PendingReceiver<network::mojom::TrustedHeaderClient> header_client)
      override;

  void OnLoaderForCorsPreflightCreated(
      const network::ResourceRequest& request,
      mojo::PendingReceiver<network::mojom::TrustedHeaderClient> header_client)
      override;

 private:
  mojo::Receiver<network::mojom::TrustedURLLoaderHeaderClient> receiver_;
};

}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_COBALT_TRUSTED_URL_LOADER_HEADER_CLIENT_H_
