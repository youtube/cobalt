#include "cobalt/browser/client_hints/cobalt_trusted_header_client.h"
#include "net/http/http_response_headers.h"
#include "url/gurl.h"

namespace cobalt {
namespace browser {

void CobaltTrustedHeaderClient::OnBeforeSendHeaders(
    const ::net::HttpRequestHeaders& headers,
    OnBeforeSendHeadersCallback callback) {
  ::net::HttpRequestHeaders mutable_headers(headers);
  // TODO(b/400459822): Cobalt - implement client hint headers
  mutable_headers.SetHeader("Sec-CH-UA-Co-Android-OS-Experience",
                            "Value");  // Modify the copy
  std::move(callback).Run(net::OK, mutable_headers);
}

void CobaltTrustedHeaderClient::OnHeadersReceived(
    const std::string& headers,
    const net::IPEndPoint& remote_endpoint,
    OnHeadersReceivedCallback callback) {
  // Do nothing for response header
  std::move(callback).Run(net::OK, absl::nullopt, absl::nullopt);
}

void CobaltTrustedHeaderClient::BindReceiver(
    mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver) {
  receiver_.Bind(std::move(receiver));
}

}  // namespace browser
}  // namespace cobalt
