// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/network_header_injection/http_header_injection_proxying_url_loader_factory.h"

#include <memory>

#include "base/functional/bind.h"
#include "chrome/browser/enterprise/network_header_injection/http_header_injection_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/enterprise/network_header_injection/core/features.h"
#include "components/enterprise/network_header_injection/core/http_header_injection_service.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace enterprise_custom_headers {

HttpHeaderInjectionProxyingURLLoaderFactory::
    HttpHeaderInjectionProxyingURLLoaderFactory(
        mojo::PendingReceiver<network::mojom::URLLoaderFactory> loader_receiver,
        mojo::PendingRemote<network::mojom::URLLoaderFactory>
            target_factory_remote,
        base::RepeatingCallback<bool()> has_rules_callback,
        base::SelfDeletingPassKey pass_key)
    : network::SelfDeletingURLLoaderFactory(std::move(loader_receiver),
                                            pass_key),
      has_rules_callback_(std::move(has_rules_callback)) {
  target_factory_.Bind(std::move(target_factory_remote));
  target_factory_.set_disconnect_handler(base::BindOnce(
      &HttpHeaderInjectionProxyingURLLoaderFactory::OnTargetFactoryError,
      base::Unretained(this)));
}

HttpHeaderInjectionProxyingURLLoaderFactory::
    ~HttpHeaderInjectionProxyingURLLoaderFactory() = default;

// static
void HttpHeaderInjectionProxyingURLLoaderFactory::MaybeProxyRequest(
    content::BrowserContext* browser_context,
    network::URLLoaderFactoryBuilder& factory_builder) {
  if (!IsHttpHeaderInjectionEnabled()) {
    return;
  }

  Profile* profile = Profile::FromBrowserContext(browser_context);
  auto* service = HttpHeaderInjectionServiceFactory::GetForProfile(profile);

  if (!service || !service->HasRules()) {
    return;
  }

  auto has_rules_callback = base::BindRepeating(
      [](base::WeakPtr<HttpHeaderInjectionService> weak_service) {
        return weak_service ? weak_service->HasRules() : false;
      },
      service->GetWeakPtr());

  auto [receiver, remote] = factory_builder.Append();
  // The factory manages its own lifetime, typically bound to the Mojo
  // connection.
  base::MakeSelfDeleting<HttpHeaderInjectionProxyingURLLoaderFactory>(
      std::move(receiver), std::move(remote), std::move(has_rules_callback));
}

void HttpHeaderInjectionProxyingURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  uint32_t new_options = options;

  // Only force the use of header client if there are rules to inject.
  if (has_rules_callback_ && has_rules_callback_.Run()) {
    new_options |= network::mojom::kURLLoadOptionUseHeaderClient;
  }

  target_factory_->CreateLoaderAndStart(std::move(loader_receiver), request_id,
                                        new_options, request, std::move(client),
                                        traffic_annotation);
}

void HttpHeaderInjectionProxyingURLLoaderFactory::OnTargetFactoryError() {
  DisconnectReceiversAndDestroy();
}

}  // namespace enterprise_custom_headers
