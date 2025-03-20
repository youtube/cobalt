#ifndef COBALT_BROWSER_COBALT_TRUSTED_HEADER_CLIENT_H_
#define COBALT_BROWSER_COBALT_TRUSTED_HEADER_CLIENT_H_

#include "mojo/public/cpp/bindings/receiver.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace cobalt {
namespace browser {

class CobaltTrustedHeaderClient : public network::mojom::TrustedHeaderClient {
 public:
  CobaltTrustedHeaderClient(
      mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver);

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

 private:
  mojo::Receiver<network::mojom::TrustedHeaderClient> receiver_;
};

}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_COBALT_TRUSTED_HEADER_CLIENT_H_
