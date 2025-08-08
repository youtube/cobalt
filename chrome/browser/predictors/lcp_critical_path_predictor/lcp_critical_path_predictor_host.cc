// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/lcp_critical_path_predictor/lcp_critical_path_predictor_host.h"

#include "base/feature_list.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/lcp_critical_path_predictor_util.h"

namespace {

size_t GetLCPPFontURLPredictorMaxUrlLength() {
  return blink::features::kLCPPFontURLPredictorMaxUrlLength.Get();
}

}  // namespace

namespace predictors {

LCPCriticalPathPredictorHost::LCPCriticalPathPredictorHost(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<blink::mojom::LCPCriticalPathPredictorHost> receiver)
    : content::DocumentService<blink::mojom::LCPCriticalPathPredictorHost>(
          render_frame_host,
          std::move(receiver)) {}

void LCPCriticalPathPredictorHost::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::LCPCriticalPathPredictorHost>
        receiver) {
  // The object is bound to the lifetime of the |render_frame_host| and the mojo
  // connection. See DocumentService for details.
  new LCPCriticalPathPredictorHost(*render_frame_host, std::move(receiver));
}

LCPCriticalPathPredictorHost::~LCPCriticalPathPredictorHost() = default;

LcpCriticalPathPredictorPageLoadMetricsObserver* LCPCriticalPathPredictorHost::
    GetLcpCriticalPathPredictorPageLoadMetricsObserver() const {
  // Due to an unresolved bug (crbug.com/1335845), GetForPage can return
  // nullptr.
  if (auto* page_data =
          LcpCriticalPathPredictorPageLoadMetricsObserver::PageData::GetForPage(
              render_frame_host().GetPage())) {
    // `LcpCriticalPathPredictorPageLoadMetricsObserver::OnCommit()` stores
    // `LcpCriticalPathPredictorPageLoadMetricsObserver` in `PageData` as a weak
    // pointer. This weak pointer can be deleted at any time.
    return page_data->GetLcpCriticalPathPredictorPageLoadMetricsObserver();
  }
  return nullptr;
}

void LCPCriticalPathPredictorHost::SetLcpElementLocator(
    const std::string& lcp_element_locator,
    std::optional<uint32_t> predicted_lcp_index) {
  if (auto* plmo = GetLcpCriticalPathPredictorPageLoadMetricsObserver()) {
    plmo->SetLcpElementLocator(lcp_element_locator, predicted_lcp_index);
  }
}

void LCPCriticalPathPredictorHost::SetLcpInfluencerScriptUrls(
    const std::vector<GURL>& lcp_influencer_scripts) {
  if (!blink::LcppScriptObserverEnabled()) {
    return;
  }
  if (auto* plmo = GetLcpCriticalPathPredictorPageLoadMetricsObserver()) {
    plmo->SetLcpInfluencerScriptUrls(lcp_influencer_scripts);
  }
}

void LCPCriticalPathPredictorHost::SetPreconnectOrigins(
    const std::vector<GURL>& origins) {
  if (!base::FeatureList::IsEnabled(
          blink::features::kLCPPAutoPreconnectLcpOrigin)) {
    return;
  }
  if (auto* plmo = GetLcpCriticalPathPredictorPageLoadMetricsObserver()) {
    plmo->SetPreconnectOrigins(origins);
  }
}

void LCPCriticalPathPredictorHost::SetUnusedPreloads(
    const std::vector<GURL>& unused_preloads) {
  if (!base::FeatureList::IsEnabled(blink::features::kLCPPDeferUnusedPreload)) {
    return;
  }
  if (auto* plmo = GetLcpCriticalPathPredictorPageLoadMetricsObserver()) {
    plmo->SetUnusedPreloads(unused_preloads);
  }
}

void LCPCriticalPathPredictorHost::NotifyFetchedFont(const GURL& font_url,
                                                     bool hit) {
  if (!base::FeatureList::IsEnabled(blink::features::kLCPPFontURLPredictor)) {
    ReportBadMessageAndDeleteThis(
        "NotifyFetchedFont can be called only if kLCPPFontURLPredictor is "
        "enabled.");
    return;
  }
  if (!font_url.SchemeIsHTTPOrHTTPS()) {
    ReportBadMessageAndDeleteThis("url format must be checked in the caller.");
    return;
  }
  if (font_url.spec().length() > GetLCPPFontURLPredictorMaxUrlLength()) {
    // The size can be different between KURL and GURL, not reporting
    // bad message.
    return;
  }
  if (auto* plmo = GetLcpCriticalPathPredictorPageLoadMetricsObserver()) {
    plmo->AppendFetchedFontUrl(font_url, hit);
  }
}

void LCPCriticalPathPredictorHost::NotifyFetchedSubresource(
    const GURL& subresource_url,
    base::TimeDelta subresource_load_start,
    network::mojom::RequestDestination request_destination) {
  if (!base::FeatureList::IsEnabled(
          blink::features::kHttpDiskCachePrewarming) &&
      !base::FeatureList::IsEnabled(
          blink::features::kLCPPPrefetchSubresource)) {
    ReportBadMessageAndDeleteThis(
        "NotifyFetchedSubresource can be called "
        "only if kHttpDiskCachePrewarming or kLCPPPrefetchSubresource are "
        "enabled.");
    return;
  }
  if (!subresource_url.SchemeIsHTTPOrHTTPS()) {
    ReportBadMessageAndDeleteThis("url scheme must be HTTP or HTTPS.");
    return;
  }
  if (subresource_load_start.is_negative()) {
    ReportBadMessageAndDeleteThis(
        "subresource load start must not be negative value.");
    return;
  }
  if (subresource_url.spec().length() >
      blink::features::kHttpDiskCachePrewarmingMaxUrlLength.Get()) {
    // The size can be different between KURL and GURL, not reporting
    // bad message.
    return;
  }
  if (auto* plmo = GetLcpCriticalPathPredictorPageLoadMetricsObserver()) {
    plmo->AppendFetchedSubresourceUrl(subresource_url, subresource_load_start,
                                      request_destination);
  }
}

}  // namespace predictors
