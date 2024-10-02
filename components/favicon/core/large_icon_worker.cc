// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/core/large_icon_worker.h"

#include <vector>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/task/task_runner.h"
#include "base/task/thread_pool.h"
#include "components/favicon/core/large_favicon_provider.h"
#include "components/favicon_base/fallback_icon_style.h"
#include "skia/ext/image_operations.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"

namespace favicon {

namespace {

bool IsDbResultAdequate(const favicon_base::FaviconRawBitmapResult& db_result,
                        int min_source_size) {
  return db_result.is_valid() &&
         db_result.pixel_size.width() == db_result.pixel_size.height() &&
         db_result.pixel_size.width() >= min_source_size;
}

// Wraps the PNG data in |db_result| in a gfx::Image. If |desired_size| is not
// 0, the image gets decoded and resized to |desired_size| (in px). Must run on
// a background thread in production.
gfx::Image ResizeLargeIconOnBackgroundThread(
    const favicon_base::FaviconRawBitmapResult& db_result,
    int desired_size) {
  gfx::Image image = gfx::Image::CreateFrom1xPNGBytes(
      db_result.bitmap_data->front(), db_result.bitmap_data->size());

  if (desired_size == 0 || db_result.pixel_size.width() == desired_size) {
    return image;
  }

  SkBitmap resized = skia::ImageOperations::Resize(
      image.AsBitmap(), skia::ImageOperations::RESIZE_LANCZOS3, desired_size,
      desired_size);
  return gfx::Image::CreateFrom1xBitmap(resized);
}

// Processes the |db_result| and writes the result into |raw_result| if
// |raw_result| is not nullptr or to |bitmap|, otherwise. If |db_result| is not
// valid or is smaller than |min_source_size|, the resulting fallback style is
// written into |fallback_icon_style|.
void ProcessIconOnBackgroundThread(
    const favicon_base::FaviconRawBitmapResult& db_result,
    int min_source_size,
    int desired_size,
    favicon_base::FaviconRawBitmapResult* raw_result,
    SkBitmap* bitmap,
    GURL* icon_url,
    favicon_base::FallbackIconStyle* fallback_icon_style) {
  if (IsDbResultAdequate(db_result, min_source_size)) {
    gfx::Image image;
    image = ResizeLargeIconOnBackgroundThread(db_result, desired_size);

    if (!image.IsEmpty()) {
      if (raw_result) {
        *raw_result = db_result;
        if (desired_size != 0)
          raw_result->pixel_size = gfx::Size(desired_size, desired_size);
        raw_result->bitmap_data = image.As1xPNGBytes();
      }
      if (bitmap) {
        *bitmap = image.AsBitmap();
      }
      if (icon_url) {
        *icon_url = db_result.icon_url;
      }
      return;
    }
  }

  if (!fallback_icon_style)
    return;

  *fallback_icon_style = favicon_base::FallbackIconStyle();
  int fallback_icon_size = 0;
  if (db_result.is_valid()) {
    favicon_base::SetDominantColorAsBackground(db_result.bitmap_data,
                                               fallback_icon_style);
    // The size must be positive, we cap to 128 to avoid the sparse histogram
    // to explode (having too many different values, server-side). Size 128
    // already indicates that there is a problem in the code, 128 px _should_ be
    // enough in all current UI surfaces.
    fallback_icon_size = db_result.pixel_size.width();
    DCHECK_GT(fallback_icon_size, 0);
    fallback_icon_size = std::min(fallback_icon_size, 128);
  }
  base::UmaHistogramSparse("Favicons.LargeIconService.FallbackSize",
                           fallback_icon_size);
}

}  // namespace

LargeIconWorker::LargeIconWorker(
    int min_source_size_in_pixel,
    int desired_size_in_pixel,
    favicon_base::LargeIconCallback raw_bitmap_callback,
    favicon_base::LargeIconImageCallback image_callback,
    base::CancelableTaskTracker* tracker)
    : min_source_size_in_pixel_(min_source_size_in_pixel),
      desired_size_in_pixel_(desired_size_in_pixel),
      raw_bitmap_callback_(std::move(raw_bitmap_callback)),
      image_callback_(std::move(image_callback)),
      background_task_runner_(base::ThreadPool::CreateTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      tracker_(tracker),
      fallback_icon_style_(
          std::make_unique<favicon_base::FallbackIconStyle>()) {}

LargeIconWorker::~LargeIconWorker() = default;

void LargeIconWorker::OnIconLookupComplete(
    const favicon_base::FaviconRawBitmapResult& db_result) {
  tracker_->PostTaskAndReply(
      background_task_runner_.get(), FROM_HERE,
      base::BindOnce(&ProcessIconOnBackgroundThread, db_result,
                     min_source_size_in_pixel_, desired_size_in_pixel_,
                     raw_bitmap_callback_ ? &raw_bitmap_result_ : nullptr,
                     image_callback_ ? &bitmap_result_ : nullptr,
                     image_callback_ ? &icon_url_ : nullptr,
                     fallback_icon_style_.get()),
      base::BindOnce(&LargeIconWorker::OnIconProcessingComplete, this));
}

// static
base::CancelableTaskTracker::TaskId LargeIconWorker::GetLargeIconRawBitmap(
    LargeFaviconProvider* provider,
    const GURL& page_url,
    int min_source_size_in_pixel,
    int desired_size_in_pixel,
    favicon_base::LargeIconCallback raw_bitmap_callback,
    favicon_base::LargeIconImageCallback image_callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK_LE(1, min_source_size_in_pixel);
  DCHECK_LE(0, desired_size_in_pixel);

  auto worker = base::MakeRefCounted<LargeIconWorker>(
      min_source_size_in_pixel, desired_size_in_pixel,
      std::move(raw_bitmap_callback), std::move(image_callback), tracker);

  int max_size_in_pixel =
      std::max(desired_size_in_pixel, min_source_size_in_pixel);

  static const base::NoDestructor<std::vector<favicon_base::IconTypeSet>>
      large_icon_types({{favicon_base::IconType::kWebManifestIcon},
                        {favicon_base::IconType::kFavicon},
                        {favicon_base::IconType::kTouchIcon},
                        {favicon_base::IconType::kTouchPrecomposedIcon}});

  // TODO(beaudoin): For now this is just a wrapper around
  //   GetLargestRawFaviconForPageURL. Add the logic required to select the
  //   best possible large icon. Also add logic to fetch-on-demand when the
  //   URL of a large icon is known but its bitmap is not available.
  return provider->GetLargestRawFaviconForPageURL(
      page_url, *large_icon_types, max_size_in_pixel,
      base::BindOnce(&LargeIconWorker::OnIconLookupComplete, worker), tracker);
}

void LargeIconWorker::OnIconProcessingComplete() {
  // If |raw_bitmap_callback_| is provided, return the raw result.
  if (raw_bitmap_callback_) {
    if (raw_bitmap_result_.is_valid()) {
      std::move(raw_bitmap_callback_)
          .Run(favicon_base::LargeIconResult(raw_bitmap_result_));
      return;
    }
    std::move(raw_bitmap_callback_)
        .Run(favicon_base::LargeIconResult(fallback_icon_style_.release()));
    return;
  }

  if (!bitmap_result_.isNull()) {
    std::move(image_callback_)
        .Run(favicon_base::LargeIconImageResult(
            gfx::Image::CreateFrom1xBitmap(bitmap_result_), icon_url_));
    return;
  }
  std::move(image_callback_)
      .Run(favicon_base::LargeIconImageResult(fallback_icon_style_.release()));
}

}  // namespace favicon
