// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/simple_host_resolver.h"

#include "base/functional/bind.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/network_anonymization_key.h"
#include "services/network/public/cpp/resolve_host_client_base.h"
#include "services/network/public/mojom/host_resolver.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace network {

class SimpleHostResolverImpl : public SimpleHostResolver,
                               public ResolveHostClientBase {
 public:
  explicit SimpleHostResolverImpl(mojom::NetworkContext* network_context)
      : network_context_(network_context) {
    receivers_.set_disconnect_handler(
        base::BindRepeating(&SimpleHostResolverImpl::OnReceiverDisconnected,
                            base::Unretained(this)));
  }

  void ResolveHost(
      mojom::HostResolverHostPtr host,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      mojom::ResolveHostParametersPtr optional_parameters,
      ResolveHostCallback callback) override {
    mojo::PendingReceiver<mojom::ResolveHostClient> receiver;
    network_context_->ResolveHost(std::move(host), network_anonymization_key,
                                  std::move(optional_parameters),
                                  receiver.InitWithNewPipeAndPassRemote());
    receivers_.Add(this, std::move(receiver), std::move(callback));
  }

  uint32_t GetNumOutstandingRequestsForTesting() const override {
    return receivers_.size();
  }

 private:
  // network::ResolveHostClientBase:
  void OnComplete(int result,
                  const net::ResolveErrorInfo& resolve_error_info,
                  const absl::optional<net::AddressList>& resolved_addresses,
                  const absl::optional<net::HostResolverEndpointResults>&
                      endpoint_results_with_metadata) override {
    auto callback = std::move(receivers_.current_context());
    receivers_.Remove(receivers_.current_receiver());
    std::move(callback).Run(result, resolve_error_info, resolved_addresses,
                            endpoint_results_with_metadata);
  }

  void OnReceiverDisconnected() {
    std::move(receivers_.current_context())
        .Run(net::ERR_FAILED, net::ResolveErrorInfo(net::ERR_FAILED),
             /*resolved_addresses=*/absl::nullopt,
             /*endpoint_results_with_metadata=*/absl::nullopt);
  }

  const raw_ptr<mojom::NetworkContext> network_context_;
  mojo::ReceiverSet<mojom::ResolveHostClient,
                    SimpleHostResolver::ResolveHostCallback>
      receivers_;
};

// static
std::unique_ptr<SimpleHostResolver> SimpleHostResolver::Create(
    network::mojom::NetworkContext* network_context) {
  return std::make_unique<SimpleHostResolverImpl>(network_context);
}

}  // namespace network
