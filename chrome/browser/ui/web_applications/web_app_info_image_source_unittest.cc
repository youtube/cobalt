// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_info_image_source.h"

#include <utility>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"

namespace web_app {
namespace {

// Verifies that when a UI view on a high-DPI screen requests a non-integer or
// scaled DIP resolution (e.g., 1.25x or 1.5x scale factor) that is not
// explicitly stored in the icon map, WebAppInfoImageSource dynamically Lanczos3
// resizes the closest available bitmap rather than serving an empty
// representation that triggers fatal UI CHECK crashes.
TEST(WebAppInfoImageSourceTest, FallbackResizesClosestBitmapDPI) {
  SkBitmap bitmap192;
  bitmap192.allocN32Pixels(192, 192);
  bitmap192.eraseColor(SK_ColorRED);

  UnorderedSizeToBitmap icons;
  icons[192] = bitmap192;

  gfx::ImageSkia image_skia(std::make_unique<WebAppInfoImageSource>(
                                /*dip_size=*/32, std::move(icons)),
                            gfx::Size(32, 32));

  gfx::ImageSkiaRep rep = image_skia.GetRepresentation(1.25f);
  EXPECT_FALSE(rep.is_null());
  EXPECT_EQ(rep.pixel_width(), 40);
  EXPECT_EQ(rep.pixel_height(), 40);
  EXPECT_FALSE(rep.GetBitmap().drawsNothing());
}

}  // namespace
}  // namespace web_app
