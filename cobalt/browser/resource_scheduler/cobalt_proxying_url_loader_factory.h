// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef COBALT_BROWSER_RESOURCE_SCHEDULER_COBALT_PROXYING_URL_LOADER_FACTORY_H_
#define COBALT_BROWSER_RESOURCE_SCHEDULER_COBALT_PROXYING_URL_LOADER_FACTORY_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_factory_builder.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace cobalt {

// Proxies URLLoaderFactory creation for renderer subresources (all <img> tags,
// JavaScript fetch/XHR requests) in Chrobalt.
//
// When D-Pad navigation / scrolling is active, non-critical subresource
// requests are deferred in memory and only forwarded to Chrome::Net once the
// user interaction settles.
class CobaltProxyingURLLoaderFactory : public network::mojom::URLLoaderFactory {
 public:
  static void MaybeProxy(network::URLLoaderFactoryBuilder& factory_builder);

  CobaltProxyingURLLoaderFactory(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
      mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory);
  ~CobaltProxyingURLLoaderFactory() override;

  CobaltProxyingURLLoaderFactory(const CobaltProxyingURLLoaderFactory&) =
      delete;
  CobaltProxyingURLLoaderFactory& operator=(
      const CobaltProxyingURLLoaderFactory&) = delete;

  // network::mojom::URLLoaderFactory implementation:
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& url_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;

  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override;

  // Called by CobaltAdaptiveResourceScheduler to drain deferred requests.
  void ResumeDeferredRequests();

 private:
  struct DeferredRequest {
    DeferredRequest(
        mojo::PendingReceiver<network::mojom::URLLoader> receiver,
        int32_t request_id,
        uint32_t options,
        const network::ResourceRequest& url_request,
        mojo::PendingRemote<network::mojom::URLLoaderClient> client,
        const net::MutableNetworkTrafficAnnotationTag& traffic_annotation);
    ~DeferredRequest();
    DeferredRequest(DeferredRequest&&);
    DeferredRequest& operator=(DeferredRequest&&);

    mojo::PendingReceiver<network::mojom::URLLoader> receiver;
    int32_t request_id;
    uint32_t options;
    network::ResourceRequest url_request;
    mojo::PendingRemote<network::mojom::URLLoaderClient> client;
    net::MutableNetworkTrafficAnnotationTag traffic_annotation;
  };

  void OnTargetFactoryDisconnect();
  void OnReceiverDisconnect();

  mojo::ReceiverSet<network::mojom::URLLoaderFactory> proxy_receivers_;
  mojo::Remote<network::mojom::URLLoaderFactory> target_factory_;

  std::vector<DeferredRequest> deferred_requests_;

  base::WeakPtrFactory<CobaltProxyingURLLoaderFactory> weak_ptr_factory_{this};
};

}  // namespace cobalt

#endif  // COBALT_BROWSER_RESOURCE_SCHEDULER_COBALT_PROXYING_URL_LOADER_FACTORY_H_
