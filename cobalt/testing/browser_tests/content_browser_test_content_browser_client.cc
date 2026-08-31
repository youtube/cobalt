// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/testing/browser_tests/content_browser_test_content_browser_client.h"

#include <optional>
#include <string_view>

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "cobalt/browser/cobalt_browser_interface_binders.h"
#include "cobalt/browser/metrics/cobalt_page_load_metrics_embedder.h"
#include "components/page_load_metrics/browser/metrics_navigation_throttle.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "content/public/browser/navigation_throttle_registry.h"
#include "content/public/common/content_client.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"

namespace content {

ContentBrowserTestContentBrowserClient::
    ContentBrowserTestContentBrowserClient() {
  if (GetShellContentBrowserClientInstances().size() > 1) {
    ContentClient::SetBrowserClientAlwaysAllowForTesting(this);
  }
}

ContentBrowserTestContentBrowserClient::
    ~ContentBrowserTestContentBrowserClient() {
  // ShellContentBrowserClient is responsible for removing `this` from
  // GetShellContentBrowserClientInstances(). Only set ContentClient's
  // variable when there is at least one more
  // ContentBrowserTestContentBrowserClient. This is necessary as the
  // last instance is owned by ContentClient and this function is called
  // during ContentClient's destruction.
  const size_t client_count = GetShellContentBrowserClientInstances().size();
  if (client_count > 1) {
    ContentClient::SetBrowserClientAlwaysAllowForTesting(
        GetShellContentBrowserClientInstances()[client_count - 2]);
  }
}

void ContentBrowserTestContentBrowserClient::OnNetworkServiceCreated(
    network::mojom::NetworkService* network_service) {
  // Override ShellContentBrowserClient::OnNetworkServiceCreated() not to call
  // NetworkService::ConfigureStubHostResolver(), because some tests are flaky
  // when configuring the stub host resolver.
  // TODO(crbug.com/41494161): Remove this override once the flakiness is fixed.
}

void ContentBrowserTestContentBrowserClient::
    RegisterBrowserInterfaceBindersForFrame(
        RenderFrameHost* render_frame_host,
        mojo::BinderMapWithContext<RenderFrameHost*>* map) {
  cobalt::PopulateCobaltFrameBinders(std::nullopt, render_frame_host, map);
  ShellContentBrowserClient::RegisterBrowserInterfaceBindersForFrame(
      render_frame_host, map);
}

void ContentBrowserTestContentBrowserClient::OnWebContentsCreated(
    WebContents* web_contents) {
  if (!page_load_metrics::MetricsWebContentsObserver::FromWebContents(
          web_contents)) {
    page_load_metrics::MetricsWebContentsObserver::CreateForWebContents(
        web_contents,
        std::make_unique<cobalt::CobaltPageLoadMetricsEmbedder>(web_contents));
  }
}

void ContentBrowserTestContentBrowserClient::CreateThrottlesForNavigation(
    content::NavigationThrottleRegistry& registry) {
  ShellContentBrowserClient::CreateThrottlesForNavigation(registry);
  page_load_metrics::MetricsNavigationThrottle::CreateAndAdd(registry);
}

void ContentBrowserTestContentBrowserClient::
    RegisterAssociatedInterfaceBindersForRenderFrameHost(
        RenderFrameHost& render_frame_host,
        blink::AssociatedInterfaceRegistry& associated_registry) {
  associated_registry.AddInterface<page_load_metrics::mojom::PageLoadMetrics>(
      base::BindRepeating(
          [](content::RenderFrameHost* rfh,
             mojo::PendingAssociatedReceiver<
                 page_load_metrics::mojom::PageLoadMetrics> receiver) {
            page_load_metrics::MetricsWebContentsObserver::BindPageLoadMetrics(
                std::move(receiver), rfh);
          },
          base::Unretained(&render_frame_host)));
}

}  // namespace content
