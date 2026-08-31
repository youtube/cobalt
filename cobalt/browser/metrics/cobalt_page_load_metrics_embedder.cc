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

#include "cobalt/browser/metrics/cobalt_page_load_metrics_embedder.h"

#include <memory>
#include <string_view>

#include "base/check_op.h"
#include "cobalt/shell/common/url_constants.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer_delegate.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace cobalt {

namespace {

// Lightweight observer that records standard Cumulative Layout Shift (CLS)
// normalized session window metrics to UMA on page complete or backgrounding.
class CobaltClsPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  CobaltClsPageLoadMetricsObserver() = default;
  ~CobaltClsPageLoadMetricsObserver() override = default;

  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override {
    return STOP_OBSERVING;
  }

  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override {
    return STOP_OBSERVING;
  }

  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override {
    RecordNormalizedCls();
  }

  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing) override {
    if (GetDelegate().DidCommit()) {
      RecordNormalizedCls();
    }
    return STOP_OBSERVING;
  }

 private:
  void RecordNormalizedCls() {
    if (!GetDelegate().DidCommit()) {
      return;
    }
    if (!GetDelegate().StartedInForeground() &&
        !GetDelegate().GetTimeToFirstForeground()) {
      return;
    }
    const page_load_metrics::NormalizedCLSData& normalized_cls_data =
        GetDelegate().GetNormalizedCLSData(
            page_load_metrics::PageLoadMetricsObserverDelegate::
                BfcacheStrategy::ACCUMULATE);
    if (!normalized_cls_data.data_tainted) {
      page_load_metrics::UmaMaxCumulativeShiftScoreHistogram10000x(
          "PageLoad.LayoutInstability.MaxCumulativeShiftScore.SessionWindow."
          "Gap1000ms.Max5000ms2",
          normalized_cls_data);
    }
  }
};

}  // namespace

CobaltPageLoadMetricsEmbedder::CobaltPageLoadMetricsEmbedder(
    content::WebContents* web_contents)
    : PageLoadMetricsEmbedderBase(web_contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

CobaltPageLoadMetricsEmbedder::~CobaltPageLoadMetricsEmbedder() = default;

void CobaltPageLoadMetricsEmbedder::RegisterObservers(
    page_load_metrics::PageLoadTracker* tracker,
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  RegisterCommonObservers(tracker);
  tracker->AddObserver(std::make_unique<CobaltClsPageLoadMetricsObserver>());
}

bool CobaltPageLoadMetricsEmbedder::IsNewTabPageUrl(const GURL& url) {
  return false;
}

bool CobaltPageLoadMetricsEmbedder::IsNoStatePrefetch(
    content::WebContents* web_contents) {
  return false;
}

bool CobaltPageLoadMetricsEmbedder::IsExtensionUrl(const GURL& url) {
  return false;
}

bool CobaltPageLoadMetricsEmbedder::IsNonTabWebUI(const GURL& url) {
  return false;
}

page_load_metrics::PageLoadMetricsMemoryTracker*
CobaltPageLoadMetricsEmbedder::GetMemoryTrackerForBrowserContext(
    content::BrowserContext* browser_context) {
  return nullptr;
}

bool CobaltPageLoadMetricsEmbedder::IsIncognito(
    content::WebContents* web_contents) {
  return web_contents && web_contents->GetBrowserContext() &&
         web_contents->GetBrowserContext()->IsOffTheRecord();
}

bool CobaltPageLoadMetricsEmbedder::ShouldObserveScheme(
    std::string_view scheme) {
  return scheme == content::kH5vccEmbeddedScheme ||
         scheme == url::kFileScheme || scheme == url::kHttpScheme ||
         scheme == url::kHttpsScheme;
}

}  // namespace cobalt
