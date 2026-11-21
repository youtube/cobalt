// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PROXY_CHECKING_HOST_RESOLVER_REQUEST_H_
#define SERVICES_NETWORK_PROXY_CHECKING_HOST_RESOLVER_REQUEST_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/network_anonymization_key.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/proxy_resolution/proxy_resolution_request.h"
#include "services/network/public/mojom/host_resolver.mojom.h"
#include "url/gurl.h"

namespace network {

class NetworkContext;

// A wrapper around a HostResolver request that first checks if a proxy is
// configured. If a proxy is configured, it resolves to an empty address list.
// Otherwise, it forwards the request to the underlying HostResolver.
class ProxyCheckingHostResolverRequest {
 public:
  ProxyCheckingHostResolverRequest(
      NetworkContext* network_context,
      mojom::HostResolverHostPtr host,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      mojom::ResolveHostParametersPtr parameters,
      mojo::PendingRemote<mojom::ResolveHostClient> response_client);

  ProxyCheckingHostResolverRequest(const ProxyCheckingHostResolverRequest&) =
      delete;
  ProxyCheckingHostResolverRequest& operator=(
      const ProxyCheckingHostResolverRequest&) = delete;

  ~ProxyCheckingHostResolverRequest();

  void Start(const GURL& url);

 private:
  void OnResolveComplete(int result);
  void DestroySelf();

  // `network_context_` must outlive `this`. This is guaranteed because
  // NetworkContext owns `this`.
  const raw_ptr<NetworkContext> network_context_;
  mojom::HostResolverHostPtr host_;
  net::NetworkAnonymizationKey network_anonymization_key_;
  mojom::ResolveHostParametersPtr parameters_;
  // Handles remote disconnection (when a request is pending) by calling
  // DestroySelf(), which notifies `network_context_` to delete `this`.
  mojo::Remote<mojom::ResolveHostClient> response_client_;
  net::ProxyInfo proxy_info_;
  std::unique_ptr<net::ProxyResolutionRequest> request_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_PROXY_CHECKING_HOST_RESOLVER_REQUEST_H_
