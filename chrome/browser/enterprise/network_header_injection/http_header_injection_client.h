// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_NETWORK_HEADER_INJECTION_HTTP_HEADER_INJECTION_CLIENT_H_
#define CHROME_BROWSER_ENTERPRISE_NETWORK_HEADER_INJECTION_HTTP_HEADER_INJECTION_CLIENT_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace enterprise_custom_headers {
class HttpHeaderInjectionService;

// Implements TrustedHeaderClient to inject custom HTTP headers based on policy.
class HttpHeaderInjectionClient : public network::mojom::TrustedHeaderClient {
 public:
  // Creates an instance and binds it to `receiver`. If `target_client` is
  // valid, this instance acts as a proxy, forwarding calls to it after
  // performing enterprise header injection.
  static void Create(
      base::WeakPtr<HttpHeaderInjectionService> service,
      mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver,
      mojo::PendingRemote<network::mojom::TrustedHeaderClient> target_client);

  HttpHeaderInjectionClient(const HttpHeaderInjectionClient&) = delete;
  HttpHeaderInjectionClient& operator=(const HttpHeaderInjectionClient&) =
      delete;
  ~HttpHeaderInjectionClient() override;

  // network::mojom::TrustedHeaderClient:
  void OnBeforeSendHeaders(const GURL& request_url,
                           const net::HttpRequestHeaders& headers,
                           OnBeforeSendHeadersCallback callback) override;
  void OnHeadersReceived(const std::string& headers,
                         const net::IPEndPoint& remote_endpoint,
                         const std::optional<net::SSLInfo>& ssl_info,
                         OnHeadersReceivedCallback callback) override;

 private:
  HttpHeaderInjectionClient(
      base::WeakPtr<HttpHeaderInjectionService> service,
      mojo::PendingRemote<network::mojom::TrustedHeaderClient> target_client);

  void OnTargetDisconnect();

  void OnTargetBeforeSendHeadersComplete(
      OnBeforeSendHeadersCallback callback,
      const GURL& request_url,
      const net::HttpRequestHeaders& original_headers,
      int32_t result,
      const std::optional<net::HttpRequestHeaders>& headers,
      std::optional<base::DictValue> extended_net_log_events);

  base::WeakPtr<HttpHeaderInjectionService> service_;
  mojo::Remote<network::mojom::TrustedHeaderClient> target_client_;

  base::WeakPtrFactory<HttpHeaderInjectionClient> weak_ptr_factory_{this};
};

}  // namespace enterprise_custom_headers

#endif  // CHROME_BROWSER_ENTERPRISE_NETWORK_HEADER_INJECTION_HTTP_HEADER_INJECTION_CLIENT_H_
