// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/protocol/ads_handler.h"

#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/page_load_metrics/browser/observers/ad_metrics/ads_page_load_metrics_observer.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

AdsHandler::AdsHandler(content::WebContents* web_contents,
                       protocol::UberDispatcher* dispatcher,
                       bool is_trusted)
    : content::WebContentsObserver(web_contents) {
  protocol::Ads::Dispatcher::wire(dispatcher, this);
}

AdsHandler::~AdsHandler() = default;

void AdsHandler::PrimaryPageChanged(content::Page& page) {
  previous_ad_frame_data_.clear();
}

protocol::Response AdsHandler::GetAdMetrics(
    std::unique_ptr<protocol::Ads::AdMetrics>* out_metrics) {
  auto* ads_observer = GetAdsPageLoadMetricsObserver();

  base::flat_map<
      base::UnguessableToken,
      page_load_metrics::AdsPageLoadMetricsObserver::AdFrameLiveStats>
      new_ad_frame_data;

  double viewport_ad_density_by_area = 0;
  double average_viewport_ad_density_by_area = 0;
  int viewport_ad_count = 0;
  double average_viewport_ad_count = 0;
  double total_ad_cpu_time = 0;
  double total_ad_network_bytes = 0;

  if (ads_observer) {
    auto density_stats = ads_observer->GetAdDensityLiveStats();
    viewport_ad_density_by_area = density_stats.viewport_ad_density_by_area;
    average_viewport_ad_density_by_area =
        density_stats.average_viewport_ad_density_by_area;
    viewport_ad_count = density_stats.viewport_ad_count;
    average_viewport_ad_count = density_stats.average_viewport_ad_count;
    total_ad_cpu_time = ads_observer->GetTotalAdCpuTime().InMillisecondsF();
    total_ad_network_bytes = ads_observer->GetTotalAdNetworkBytes();
    new_ad_frame_data = ads_observer->GetAdFrameLiveStats();
  }

  // Identify ad frames that have been added, updated, or removed since the
  // previous metric collection.
  auto update_ad_frames =
      std::make_unique<protocol::Array<protocol::Ads::AdFrameData>>();
  auto remove_ad_frames = std::make_unique<protocol::Array<protocol::String>>();

  for (const auto& [frame_id, frame] : new_ad_frame_data) {
    auto it = previous_ad_frame_data_.find(frame_id);
    if (it == previous_ad_frame_data_.end() ||
        it->second.initial_origin != frame.initial_origin ||
        it->second.network_bytes != frame.network_bytes ||
        it->second.cpu_time != frame.cpu_time) {
      auto protocol_frame = protocol::Ads::AdFrameData::Create()
                                .SetFrameId(frame_id.ToString())
                                .SetNetworkBytes(frame.network_bytes)
                                .SetCpuTime(frame.cpu_time.InMillisecondsF())
                                .Build();

      // To minimize protocol payload size, `initial_origin` is only sent when
      // it changes from the previous message for a given frame. Typically, this
      // is sent only once per frame. We retain this check to handle potential
      // edge cases (e.g., AdsPLMO might stop tracking a frame that subsequently
      // navigates cross-origin before being re-added to the tracker).
      if (it == previous_ad_frame_data_.end() ||
          it->second.initial_origin != frame.initial_origin) {
        protocol_frame->SetInitialOrigin(frame.initial_origin.Serialize());
      }

      update_ad_frames->emplace_back(std::move(protocol_frame));
    }
  }

  for (const auto& [frame_id, frame] : previous_ad_frame_data_) {
    if (!new_ad_frame_data.contains(frame_id)) {
      remove_ad_frames->emplace_back(frame_id.ToString());
    }
  }

  previous_ad_frame_data_ = std::move(new_ad_frame_data);

  *out_metrics = protocol::Ads::AdMetrics::Create()
                     .SetViewportAdDensityByArea(viewport_ad_density_by_area)
                     .SetAverageViewportAdDensityByArea(
                         average_viewport_ad_density_by_area)
                     .SetViewportAdCount(viewport_ad_count)
                     .SetAverageViewportAdCount(average_viewport_ad_count)
                     .SetTotalAdCpuTime(total_ad_cpu_time)
                     .SetTotalAdNetworkBytes(total_ad_network_bytes)
                     .SetUpdateAdFrames(std::move(update_ad_frames))
                     .SetRemoveAdFrames(std::move(remove_ad_frames))
                     .Build();

  return protocol::Response::Success();
}

page_load_metrics::AdsPageLoadMetricsObserver*
AdsHandler::GetAdsPageLoadMetricsObserver() {
  if (!web_contents()) {
    return nullptr;
  }

  content::RenderFrameHost* main_frame = web_contents()->GetPrimaryMainFrame();
  if (!main_frame) {
    return nullptr;
  }

  auto* metrics_web_contents_observer =
      page_load_metrics::MetricsWebContentsObserver::FromWebContents(
          web_contents());
  if (!metrics_web_contents_observer) {
    return nullptr;
  }

  base::WeakPtr<page_load_metrics::PageLoadMetricsObserverInterface>
      ads_observer = metrics_web_contents_observer->GetMetricsObserver(
          main_frame,
          page_load_metrics::AdsPageLoadMetricsObserver::kObserverName);
  if (ads_observer) {
    return static_cast<page_load_metrics::AdsPageLoadMetricsObserver*>(
        ads_observer.get());
  }
  return nullptr;
}
