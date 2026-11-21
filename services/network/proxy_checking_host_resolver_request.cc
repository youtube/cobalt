// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/proxy_checking_host_resolver_request.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "net/base/net_errors.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/url_request/url_request_context.h"
#include "services/network/network_context.h"

namespace network {

ProxyCheckingHostResolverRequest::ProxyCheckingHostResolverRequest(
    NetworkContext* network_context,
    mojom::HostResolverHostPtr host,
    const net::NetworkAnonymizationKey& network_anonymization_key,
    mojom::ResolveHostParametersPtr parameters,
    mojo::PendingRemote<mojom::ResolveHostClient> response_client)
    : network_context_(network_context),
      host_(std::move(host)),
      network_anonymization_key_(network_anonymization_key),
      parameters_(std::move(parameters)),
      response_client_(std::move(response_client)) {
  CHECK(parameters_);
  // direct_only false to prevent infinite recursion on ResolveHost.
  parameters_->direct_only = false;
}

ProxyCheckingHostResolverRequest::~ProxyCheckingHostResolverRequest() {
  if (request_) {
    int error = net::ERR_ABORTED;
    response_client_->OnComplete(error, net::ResolveErrorInfo(error),
                                 /*resolved_addresses=*/net::AddressList(),
                                 /*alternative_endpoints=*/{});
  }
}

void ProxyCheckingHostResolverRequest::Start(const GURL& url) {
  CHECK(response_client_.is_connected());
  response_client_.set_disconnect_handler(base::BindOnce(
      &ProxyCheckingHostResolverRequest::DestroySelf, base::Unretained(this)));

  int result =
      network_context_->url_request_context()
          ->proxy_resolution_service()
          ->ResolveProxy(
              url, std::string(), network_anonymization_key_,
              net::handles::kInvalidNetworkHandle, &proxy_info_,
              base::BindOnce(
                  &ProxyCheckingHostResolverRequest::OnResolveComplete,
                  base::Unretained(this)),
              &request_, net::NetLogWithSource(), net::DEFAULT_PRIORITY);
  if (result != net::ERR_IO_PENDING) {
    OnResolveComplete(result);
  }
}

void ProxyCheckingHostResolverRequest::OnResolveComplete(int result) {
  CHECK(response_client_.is_connected());
  response_client_.set_disconnect_handler(base::OnceClosure());
  if (result == net::OK && !proxy_info_.is_direct()) {
    int error = net::ERR_DNS_DIRECT_ONLY;
    response_client_->OnComplete(error, net::ResolveErrorInfo(error),
                                 /*resolved_addresses=*/net::AddressList(),
                                 /*alternative_endpoints=*/{});
  } else {
    // Note we fail open, resolving host if proxy resolution failed.
    network_context_->ResolveHost(std::move(host_), network_anonymization_key_,
                                  std::move(parameters_),
                                  response_client_.Unbind());
  }
  DestroySelf();
}

void ProxyCheckingHostResolverRequest::DestroySelf() {
  request_.reset();
  network_context_->OnProxyCheckingHostResolverRequestComplete(this);
}

}  // namespace network
