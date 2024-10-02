// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_NETWORK_CONTEXT_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_NETWORK_CONTEXT_H_

#include "base/memory/scoped_refptr.h"
#include "content/browser/preloading/prefetch/prefetch_type.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/isolation_info.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/loader/referrer.mojom.h"

namespace content {

class PrefetchService;

// An isolated network context used for prefetches. The purpose of using a
// separate network context is to set a custom proxy configuration, and separate
// any cookies.
class CONTENT_EXPORT PrefetchNetworkContext {
 public:
  PrefetchNetworkContext(
      PrefetchService* prefetch_service,
      const PrefetchType& prefetch_type,
      const blink::mojom::Referrer& referring_origin,
      const GlobalRenderFrameHostId& referring_render_frame_host_id);
  ~PrefetchNetworkContext();

  PrefetchNetworkContext(const PrefetchNetworkContext&) = delete;
  const PrefetchNetworkContext operator=(const PrefetchNetworkContext&) =
      delete;

  // Get a reference to |network_context_|.
  network::mojom::NetworkContext* GetNetworkContext() const;

  // Get a reference to |url_loader_factory_|. If it is null, then
  // |network_context_| is bound and configured, and a new
  // |SharedURLLoaderFactory| is created.
  network::mojom::URLLoaderFactory* GetURLLoaderFactory();

  // Get a reference to |cookie_manager_|. If it is null, then it is bound to
  // the cookie manager of |network_context_|.
  network::mojom::CookieManager* GetCookieManager();

  // Close any idle connections with |network_context_|.
  void CloseIdleConnections();

 private:
  // Binds |pending_receiver| to a URL loader factory associated with
  // the given |network_context|.
  void CreateNewURLLoaderFactory(
      network::mojom::NetworkContext* network_context,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> pending_receiver,
      absl::optional<net::IsolationInfo> isolation_info);

  // Bind |network_context_| to a new network context and configure it to use
  // the prefetch proxy. Also set up |url_loader_factory_| as a new URL loader
  // factory for |network_context_|.
  void CreateIsolatedURLLoaderFactory();

  raw_ptr<PrefetchService> prefetch_service_;

  // Determines whether or not an isolated network context or the default
  // network context should be used. If an isolated network context is required,
  // also determines if it should be configured to use the Prefetch Proxy or
  // not.
  const PrefetchType prefetch_type_;

  // These parameters are used when considering to proxy |url_loader_factory_|
  // by calling WillCreateURLLoaderFactory.
  const blink::mojom::Referrer referrer_;
  const GlobalRenderFrameHostId referring_render_frame_host_id_;

  // The network context and URL loader factory to use when making prefetches.
  mojo::Remote<network::mojom::NetworkContext> network_context_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // The cookie manager for the isolated |network_context_|. This is used when
  // copying cookies from the isolated prefetch network context to the default
  // network context after a prefetch is committed.
  mojo::Remote<network::mojom::CookieManager> cookie_manager_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_NETWORK_CONTEXT_H_
