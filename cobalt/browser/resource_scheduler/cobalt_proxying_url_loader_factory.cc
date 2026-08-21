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

#include "cobalt/browser/resource_scheduler/cobalt_proxying_url_loader_factory.h"

#include <utility>

#include "base/logging.h"
#include "cobalt/browser/resource_scheduler/cobalt_adaptive_resource_scheduler.h"
#include "net/base/request_priority.h"

namespace cobalt {

namespace {

bool IsRequestDeferrable(const network::ResourceRequest& request) {
  // Never defer scripts, stylesheets, or main documents.
  if (request.destination == network::mojom::RequestDestination::kScript ||
      request.destination == network::mojom::RequestDestination::kStyle ||
      request.destination == network::mojom::RequestDestination::kDocument) {
    return false;
  }

  // Never defer high-priority or media chunk requests.
  if (request.priority >= net::RequestPriority::MEDIUM) {
    return false;
  }

  // Images, fonts, and low-priority fetches are candidates for deferral.
  return true;
}

}  // namespace

// static
void CobaltProxyingURLLoaderFactory::MaybeProxy(
    network::URLLoaderFactoryBuilder& factory_builder) {
  mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver;
  mojo::PendingRemote<network::mojom::URLLoaderFactory> remote;
  std::tie(receiver, remote) = factory_builder.Append();

  // Self-managed lifecycle tied to mojo connection lifetime.
  new CobaltProxyingURLLoaderFactory(std::move(receiver), std::move(remote));
}

CobaltProxyingURLLoaderFactory::DeferredRequest::DeferredRequest(
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& url_request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
    : receiver(std::move(receiver)),
      request_id(request_id),
      options(options),
      url_request(url_request),
      client(std::move(client)),
      traffic_annotation(traffic_annotation) {}

CobaltProxyingURLLoaderFactory::DeferredRequest::~DeferredRequest() = default;
CobaltProxyingURLLoaderFactory::DeferredRequest::DeferredRequest(
    DeferredRequest&&) = default;
CobaltProxyingURLLoaderFactory::DeferredRequest&
CobaltProxyingURLLoaderFactory::DeferredRequest::operator=(DeferredRequest&&) =
    default;

CobaltProxyingURLLoaderFactory::CobaltProxyingURLLoaderFactory(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
    mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory)
    : target_factory_(std::move(target_factory)) {
  proxy_receivers_.Add(this, std::move(receiver));
  proxy_receivers_.set_disconnect_handler(
      base::BindRepeating(&CobaltProxyingURLLoaderFactory::OnReceiverDisconnect,
                          base::Unretained(this)));
  target_factory_.set_disconnect_handler(
      base::BindOnce(&CobaltProxyingURLLoaderFactory::OnTargetFactoryDisconnect,
                     base::Unretained(this)));

  CobaltAdaptiveResourceScheduler::GetInstance()->RegisterProxyFactory(this);
}

CobaltProxyingURLLoaderFactory::~CobaltProxyingURLLoaderFactory() {
  CobaltAdaptiveResourceScheduler::GetInstance()->UnregisterProxyFactory(this);
}

void CobaltProxyingURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& url_request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  auto* scheduler = CobaltAdaptiveResourceScheduler::GetInstance();
  if (scheduler && scheduler->IsScrolling() &&
      IsRequestDeferrable(url_request)) {
    LOG(INFO) << "CobaltProxyingURLLoaderFactory: Deferring renderer "
                 "subresource during scroll: "
              << url_request.url.spec();
    deferred_requests_.emplace_back(std::move(receiver), request_id, options,
                                    url_request, std::move(client),
                                    traffic_annotation);
    return;
  }

  target_factory_->CreateLoaderAndStart(std::move(receiver), request_id,
                                        options, url_request, std::move(client),
                                        traffic_annotation);
}

void CobaltProxyingURLLoaderFactory::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver) {
  proxy_receivers_.Add(this, std::move(receiver));
}

void CobaltProxyingURLLoaderFactory::ResumeDeferredRequests() {
  if (deferred_requests_.empty()) {
    return;
  }

  LOG(INFO) << "CobaltProxyingURLLoaderFactory: Resuming "
            << deferred_requests_.size() << " deferred subresource requests.";

  auto requests_to_resume = std::move(deferred_requests_);
  deferred_requests_.clear();

  for (auto& req : requests_to_resume) {
    if (target_factory_.is_bound()) {
      target_factory_->CreateLoaderAndStart(
          std::move(req.receiver), req.request_id, req.options, req.url_request,
          std::move(req.client), req.traffic_annotation);
    }
  }
}

void CobaltProxyingURLLoaderFactory::OnTargetFactoryDisconnect() {
  delete this;
}

void CobaltProxyingURLLoaderFactory::OnReceiverDisconnect() {
  if (proxy_receivers_.empty()) {
    delete this;
  }
}

}  // namespace cobalt
