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

#ifndef COBALT_BROWSER_METRICS_COBALT_PAGE_LOAD_METRICS_EMBEDDER_H_
#define COBALT_BROWSER_METRICS_COBALT_PAGE_LOAD_METRICS_EMBEDDER_H_

#include <string_view>

#include "components/page_load_metrics/browser/page_load_metrics_embedder_base.h"

namespace content {
class BrowserContext;
class NavigationHandle;
class WebContents;
}  // namespace content

namespace page_load_metrics {
class PageLoadMetricsMemoryTracker;
class PageLoadTracker;
}  // namespace page_load_metrics

namespace cobalt {

// Embedder interface implementation that attaches standard PageLoadMetrics
// observers (including UmaPageLoadMetricsObserver for FCP, LCP, CLS, INP) to
// Cobalt WebContents.
//
// Lifetime and Ownership:
// This class is instantiated on WebContents creation and its ownership is
// transferred to `MetricsWebContentsObserver` (a `WebContentsUserData`),
// meaning its lifetime is bound to the associated `WebContents`.
//
// Threading Model:
// This class is thread-affine and must be constructed and used exclusively
// on the Browser UI thread.
class CobaltPageLoadMetricsEmbedder
    : public page_load_metrics::PageLoadMetricsEmbedderBase {
 public:
  explicit CobaltPageLoadMetricsEmbedder(content::WebContents* web_contents);
  CobaltPageLoadMetricsEmbedder(const CobaltPageLoadMetricsEmbedder&) = delete;
  CobaltPageLoadMetricsEmbedder& operator=(
      const CobaltPageLoadMetricsEmbedder&) = delete;
  ~CobaltPageLoadMetricsEmbedder() override;

  // page_load_metrics::PageLoadMetricsEmbedderBase:
  bool IsNewTabPageUrl(const GURL& url) override;
  bool IsNoStatePrefetch(content::WebContents* web_contents) override;
  bool IsExtensionUrl(const GURL& url) override;
  bool IsNonTabWebUI(const GURL& url) override;
  page_load_metrics::PageLoadMetricsMemoryTracker*
  GetMemoryTrackerForBrowserContext(
      content::BrowserContext* browser_context) override;
  bool IsIncognito(content::WebContents* web_contents) override;
  bool ShouldObserveScheme(std::string_view scheme) override;

 protected:
  // page_load_metrics::PageLoadMetricsEmbedderBase:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker,
                         content::NavigationHandle* navigation_handle) override;
};

}  // namespace cobalt

#endif  // COBALT_BROWSER_METRICS_COBALT_PAGE_LOAD_METRICS_EMBEDDER_H_
