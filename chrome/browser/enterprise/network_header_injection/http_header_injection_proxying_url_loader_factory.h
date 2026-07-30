// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_NETWORK_HEADER_INJECTION_HTTP_HEADER_INJECTION_PROXYING_URL_LOADER_FACTORY_H_
#define CHROME_BROWSER_ENTERPRISE_NETWORK_HEADER_INJECTION_HTTP_HEADER_INJECTION_PROXYING_URL_LOADER_FACTORY_H_

#include "base/functional/callback.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/self_deleting_url_loader_factory.h"
#include "services/network/public/cpp/url_loader_factory_builder.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace content {
class BrowserContext;
}

namespace enterprise_custom_headers {

// Proxies URLLoaderFactory to force the use of TrustedHeaderClient for
// requests, enabling our HttpHeaderInjectionClient to inject headers after
// extensions.
// Note: Because this proxy is created conditionally, if the policy changes
// from empty to non-empty, existing URLLoaderFactory instances won't see the
// change since the `HttpHeaderInjectionProxyingURLLoaderFactory` proxy was
// never instantiated for them. The factory must be recreated for the change
// to take effect (e.g., by reloading the page or navigating to a new URL).
class HttpHeaderInjectionProxyingURLLoaderFactory
    : public network::SelfDeletingURLLoaderFactory {
 public:
  HttpHeaderInjectionProxyingURLLoaderFactory(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> loader_receiver,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          target_factory_remote,
      base::RepeatingCallback<bool()> has_rules_callback,
      base::SelfDeletingPassKey pass_key);
  HttpHeaderInjectionProxyingURLLoaderFactory(
      const HttpHeaderInjectionProxyingURLLoaderFactory&) = delete;
  HttpHeaderInjectionProxyingURLLoaderFactory& operator=(
      const HttpHeaderInjectionProxyingURLLoaderFactory&) = delete;

  static void MaybeProxyRequest(
      content::BrowserContext* browser_context,
      network::URLLoaderFactoryBuilder& factory_builder);

  // network::mojom::URLLoaderFactory:
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;

 private:
  ~HttpHeaderInjectionProxyingURLLoaderFactory() override;

  void OnTargetFactoryError();

  mojo::Remote<network::mojom::URLLoaderFactory> target_factory_;
  base::RepeatingCallback<bool()> has_rules_callback_;
};

}  // namespace enterprise_custom_headers

#endif  // CHROME_BROWSER_ENTERPRISE_NETWORK_HEADER_INJECTION_HTTP_HEADER_INJECTION_PROXYING_URL_LOADER_FACTORY_H_
