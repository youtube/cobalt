// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_info_image_source.h"

#include "skia/ext/image_operations.h"
#include "ui/gfx/image/image_skia_rep.h"

WebAppInfoImageSource::WebAppInfoImageSource(
    int dip_size,
    web_app::UnorderedSizeToBitmap icons)
    : dip_size_(dip_size), icons_(std::move(icons)) {}

WebAppInfoImageSource::~WebAppInfoImageSource() = default;

gfx::ImageSkiaRep WebAppInfoImageSource::GetImageForScale(float scale) {
  if (icons_.empty()) {
    return gfx::ImageSkiaRep();
  }

  int expected_size = base::saturated_cast<int>(dip_size_ * scale);
  auto icon = icons_.find(expected_size);
  if (icon != icons_.end() && !icon->second.drawsNothing()) {
    return gfx::ImageSkiaRep(icon->second, scale);
  }

  // Fallback: find the best matching non-empty bitmap and Lanczos3 resize it.
  // Prefer finding the smallest icon >= target size (to minimize downscaling
  // distortion). If no icon exists >= target size, pick the largest icon
  // available (to maximize upscaled quality).
  const SkBitmap* best_bitmap = nullptr;
  int best_size = 0;
  for (const auto& [icon_size, bitmap] : icons_) {
    // Skip uninitialized or empty SkBitmaps.
    if (bitmap.drawsNothing()) {
      continue;
    }
    // Initialize our candidate with the very first valid bitmap encountered.
    if (!best_bitmap) {
      best_bitmap = &bitmap;
      best_size = icon_size;
      continue;
    }
    // If our current best candidate is smaller than the target size, prefer any
    // larger icon available (getting us closer to or above target resolution).
    if (best_size < expected_size && icon_size > best_size) {
      best_bitmap = &bitmap;
      best_size = icon_size;
    } else if (icon_size >= expected_size && icon_size < best_size) {
      best_bitmap = &bitmap;
      best_size = icon_size;
    }
  }

  if (!best_bitmap) {
    return gfx::ImageSkiaRep();
  }

  // TODO(crbug.com/525510633): Move this synchronous image resizing operation
  // to a background threadpool task asynchronously.
  SkBitmap resized = skia::ImageOperations::Resize(
      *best_bitmap, skia::ImageOperations::RESIZE_LANCZOS3, expected_size,
      expected_size);

  // If `best_bitmap` exists, `resized` is guaranteed to not be an empty bitmap.
  CHECK(!resized.drawsNothing());
  return gfx::ImageSkiaRep(resized, scale);
}
