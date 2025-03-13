#ifndef COBALT_BROWSER_COBALT_TRUSTED_HEADER_CLIENT_H_
#define COBALT_BROWSER_COBALT_TRUSTED_HEADER_CLIENT_H_

#include "mojo/public/cpp/bindings/receiver.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_util.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace cobalt {
namespace browser {

class CobaltTrustedHeaderClient : public network::mojom::TrustedHeaderClient {
 public:
  CobaltTrustedHeaderClient();

  CobaltTrustedHeaderClient(const CobaltTrustedHeaderClient&) = delete;
  CobaltTrustedHeaderClient& operator=(const CobaltTrustedHeaderClient&) =
      delete;

  ~CobaltTrustedHeaderClient() override;

  void OnBeforeSendHeaders(net::HttpRequestHeaders headers,
                           OnBeforeSendHeadersCallback callback) override;

  void OnHeadersReceived(const std::string& headers,
                         const net::IPEndPoint& remote_endpoint,
                         OnHeadersReceivedCallback callback) override;

 private:
  mojo::Receiver<network::mojom::TrustedHeaderClient> receiver_;
};

}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_COBALT_TRUSTED_HEADER_CLIENT_H_
