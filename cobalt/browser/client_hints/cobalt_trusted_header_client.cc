#include "cobalt/browser/client_hints/cobalt_trusted_header_client.h"

namespace cobalt {
namespace browser {

CobaltTrustedHeaderClient::CobaltTrustedHeaderClient() = default;

CobaltTrustedHeaderClient::~CobaltTrustedHeaderClient() = default;

void CobaltTrustedHeaderClient::OnBeforeSendHeaders(
    net::HttpRequestHeaders headers,
    OnBeforeSendHeadersCallback callback) {
  headers.SetHeader("Sec-CH-UA-Co-Android-OS-Experience",
                    "Amati");  // Hardcoded value
  std::move(callback).Run(net::OK, headers);
}

void CobaltTrustedHeaderClient::OnHeadersReceived(
    const std::string& headers,
    const net::IPEndPoint& remote_endpoint,
    OnHeadersReceivedCallback callback) {
  net::HttpResponseHeaders parsed_headers(
      net::HttpUtil::AssembleRawHeaders(headers));
  std::move(callback).Run(net::OK, parsed_headers.raw_headers(), nullptr);
}

}  // namespace browser
}  // namespace cobalt
