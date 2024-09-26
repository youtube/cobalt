// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_CORE_LARGE_ICON_SERVICE_IMPL_H_
#define COMPONENTS_FAVICON_CORE_LARGE_ICON_SERVICE_IMPL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/favicon/core/large_icon_service.h"
#include "components/favicon_base/favicon_callback.h"
#include "components/favicon_base/favicon_types.h"
#include "components/image_fetcher/core/image_fetcher.h"

class GURL;

namespace net {
struct NetworkTrafficAnnotationTag;
}

namespace favicon {

class FaviconService;

// Implementation class for LargeIconService.
class LargeIconServiceImpl : public LargeIconService {
 public:
  LargeIconServiceImpl(
      FaviconService* favicon_service,
      std::unique_ptr<image_fetcher::ImageFetcher> image_fetcher,
      int desired_size_in_dip_for_server_requests,
      favicon_base::IconType icon_type_for_server_requests,
      const std::string& google_server_client_param);

  LargeIconServiceImpl(const LargeIconServiceImpl&) = delete;
  LargeIconServiceImpl& operator=(const LargeIconServiceImpl&) = delete;

  ~LargeIconServiceImpl() override;

  // LargeIconService Implementation.
  base::CancelableTaskTracker::TaskId
  GetLargeIconRawBitmapOrFallbackStyleForPageUrl(
      const GURL& page_url,
      int min_source_size_in_pixel,
      int desired_size_in_pixel,
      favicon_base::LargeIconCallback callback,
      base::CancelableTaskTracker* tracker) override;
  base::CancelableTaskTracker::TaskId
  GetLargeIconImageOrFallbackStyleForPageUrl(
      const GURL& page_url,
      int min_source_size_in_pixel,
      int desired_size_in_pixel,
      favicon_base::LargeIconImageCallback callback,
      base::CancelableTaskTracker* tracker) override;
  base::CancelableTaskTracker::TaskId
  GetLargeIconRawBitmapOrFallbackStyleForIconUrl(
      const GURL& icon_url,
      int min_source_size_in_pixel,
      int desired_size_in_pixel,
      favicon_base::LargeIconCallback callback,
      base::CancelableTaskTracker* tracker) override;
  base::CancelableTaskTracker::TaskId GetIconRawBitmapOrFallbackStyleForPageUrl(
      const GURL& page_url,
      int desired_size_in_pixel,
      favicon_base::LargeIconCallback callback,
      base::CancelableTaskTracker* tracker) override;
  void GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
      const GURL& page_url,
      bool may_page_url_be_private,
      bool should_trim_page_url_path,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      favicon_base::GoogleFaviconServerCallback callback) override;
  void TouchIconFromGoogleServer(const GURL& icon_url) override;

  // Overrides the URL of the Google favicon server to send requests to for
  // testing.
  void SetServerUrlForTesting(const GURL& server_url_for_testing);

 private:
  base::CancelableTaskTracker::TaskId GetLargeIconOrFallbackStyleImpl(
      const GURL& page_url,
      int min_source_size_in_pixel,
      int desired_size_in_pixel,
      favicon_base::LargeIconCallback raw_bitmap_callback,
      favicon_base::LargeIconImageCallback image_callback,
      base::CancelableTaskTracker* tracker);

  void OnCanSetOnDemandFaviconComplete(
      const GURL& server_request_url,
      const GURL& page_url,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      favicon_base::GoogleFaviconServerCallback callback,
      bool can_set_on_demand_favicon);

  const raw_ptr<FaviconService> favicon_service_;

  const std::unique_ptr<image_fetcher::ImageFetcher> image_fetcher_;

  const int desired_size_in_pixel_for_server_requests_;

  const favicon_base::IconType icon_type_for_server_requests_;

  const std::string google_server_client_param_;

  // URL of the Google favicon server (overridable by tests).
  GURL server_url_;

  base::WeakPtrFactory<LargeIconServiceImpl> weak_ptr_factory_{this};
};

}  // namespace favicon

#endif  // COMPONENTS_FAVICON_CORE_LARGE_ICON_SERVICE_IMPL_H_
