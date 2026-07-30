// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_url_loader_factory_utils.h"

#include "content/browser/loader/url_loader_factory_utils.h"
#include "content/browser/preloading/prefetch/prefetch_request.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "net/base/isolation_info.h"
#include "services/network/public/cpp/constants.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace content {

namespace {
static network::SharedURLLoaderFactory* g_url_loader_factory_for_testing =
    nullptr;
}  // namespace

network::mojom::URLLoaderFactoryParamsPtr CreatePrefetchURLLoaderFactoryParams(
    const base::UnguessableToken& network_restrictions_id) {
  auto factory_params = network::mojom::URLLoaderFactoryParams::New();
  factory_params->process_id = network::OriginatingProcessId::browser();
  factory_params->is_trusted = true;
  factory_params->is_orb_enabled = false;
  factory_params->network_restrictions_id = network_restrictions_id;
  return factory_params;
}

void SetTerminalPrefetchURLLoaderFactoryForTesting(  // IN-TEST
    network::SharedURLLoaderFactory* url_loader_factory) {
  g_url_loader_factory_for_testing = url_loader_factory;
}

scoped_refptr<network::SharedURLLoaderFactory> CreatePrefetchURLLoaderFactory(
    network::mojom::NetworkContext* network_context,
    const PrefetchRequest& prefetch_request,
    scoped_refptr<network::SharedURLLoaderFactory>
        pre_prefetch_url_loader_factory) {
  CHECK(network_context);

  RenderFrameHost* referring_render_frame_host;
  int referring_render_process_id;
  ukm::SourceIdObj ukm_source_id;
  if (auto* renderer_initiator_info =
          prefetch_request.GetRendererInitiatorInfo()) {
    referring_render_frame_host = renderer_initiator_info->GetRenderFrameHost();
    CHECK(referring_render_frame_host);
    referring_render_process_id =
        referring_render_frame_host->GetProcess()->GetDeprecatedID();
    ukm_source_id =
        ukm::SourceIdObj::FromInt64(renderer_initiator_info->ukm_source_id());
  } else {
    referring_render_frame_host = nullptr;
    referring_render_process_id = content::ChildProcessHost::kInvalidUniqueID;
    ukm_source_id = ukm::kInvalidSourceIdObj;
  }

  base::UnguessableToken network_restrictions_id =
      network::GetNoOpNetworkRestrictionsId();
  if (referring_render_frame_host) {
    auto* rfh_impl =
        static_cast<RenderFrameHostImpl*>(referring_render_frame_host);
    network_restrictions_id = rfh_impl->GetNetworkRestrictionsID();
  }

  bool bypass_redirect_checks = false;

  url_loader_factory::TerminalParams terminal_params = [&]() {
    // If this is for PrePrefetch-promoted request, serve from the PrePrefetched
    // result.
    if (pre_prefetch_url_loader_factory) {
      return url_loader_factory::TerminalParams::ForNonNetwork(
          std::move(pre_prefetch_url_loader_factory),
          network::mojom::kBrowserProcessId);
    }

    // Intercept the request for testing, if any (but not for
    // PrePrefetch-promoted cases, see the method comment in the header).
    if (g_url_loader_factory_for_testing) {
      return url_loader_factory::TerminalParams::ForNonNetwork(
          base::WrapRefCounted(g_url_loader_factory_for_testing),
          network::mojom::kBrowserProcessId);
    }

    // Otherwise, send the request to the network.
    return url_loader_factory::TerminalParams::ForNetworkContext(
        network_context,
        CreatePrefetchURLLoaderFactoryParams(network_restrictions_id),
        url_loader_factory::HeaderClientOption::kAllow);
  }();

  return url_loader_factory::Create(
      ContentBrowserClient::URLLoaderFactoryType::kPrefetch,
      std::move(terminal_params),
      url_loader_factory::ContentClientParams(
          prefetch_request.browser_context(), referring_render_frame_host,
          referring_render_process_id,
          prefetch_request.referring_origin().value_or(url::Origin()),
          net::IsolationInfo(), ukm_source_id, &bypass_redirect_checks));
}

}  // namespace content
