// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_PROTOCOL_ADS_HANDLER_H_
#define CHROME_BROWSER_DEVTOOLS_PROTOCOL_ADS_HANDLER_H_

#include "base/containers/flat_map.h"
#include "base/unguessable_token.h"
#include "chrome/browser/devtools/protocol/ads.h"
#include "components/page_load_metrics/browser/observers/ad_metrics/ads_page_load_metrics_observer.h"
#include "content/public/browser/web_contents_observer.h"

// Implements the "Ads" DevTools protocol domain.
//
// This class bridges the DevTools frontend with the backend
// `AdsPageLoadMetricsObserver`. It handles requests from the frontend to
// retrieve page-level ad statistics.
class AdsHandler : public protocol::Ads::Backend,
                   public content::WebContentsObserver {
 public:
  AdsHandler(content::WebContents* web_contents,
             protocol::UberDispatcher* dispatcher,
             bool is_trusted);
  ~AdsHandler() override;
  AdsHandler(const AdsHandler&) = delete;
  AdsHandler& operator=(const AdsHandler&) = delete;

 private:
  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;

  // protocol::Ads::Backend:
  protocol::Response GetAdMetrics(
      std::unique_ptr<protocol::Ads::AdMetrics>* out_metrics) override;

  page_load_metrics::AdsPageLoadMetricsObserver*
  GetAdsPageLoadMetricsObserver();

  // Maps the DevTools frame token to the last sent AdFrameLiveStats.
  base::flat_map<
      base::UnguessableToken,
      page_load_metrics::AdsPageLoadMetricsObserver::AdFrameLiveStats>
      previous_ad_frame_data_;
};

#endif  // CHROME_BROWSER_DEVTOOLS_PROTOCOL_ADS_HANDLER_H_
