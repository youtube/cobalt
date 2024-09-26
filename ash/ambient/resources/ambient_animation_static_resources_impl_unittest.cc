// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/resources/ambient_animation_static_resources.h"

#include "ash/ambient/resources/ambient_animation_resource_constants.h"
#include "ash/constants/ambient_theme.h"
#include "base/json/json_reader.h"
#include "cc/paint/skottie_wrapper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::Not;
using ::testing::NotNull;

// AmbientAnimationStaticResources actually has very little application logic
// and is more a class to house static data. Thus, an animation theme is picked
// as an example and the basics are tested with it. A test case does not need to
// exist for every possible animation theme.

TEST(AmbientAnimationStaticResourcesTest, LoadsLottieData) {
  auto resources = AmbientAnimationStaticResources::Create(
      AmbientTheme::kFeelTheBreeze, /*serializable=*/false);
  ASSERT_THAT(resources->GetSkottieWrapper(), NotNull());
  EXPECT_TRUE(resources->GetSkottieWrapper()->is_valid());
}

TEST(AmbientAnimationStaticResourcesTest, LoadsStaticAssets) {
  auto resources = AmbientAnimationStaticResources::Create(
      AmbientTheme::kFeelTheBreeze, /*serializable=*/false);
  ASSERT_THAT(resources, NotNull());
  for (base::StringPiece asset_id :
       ambient::resources::kAllFeelTheBreezeStaticAssets) {
    gfx::ImageSkia image_original = resources->GetStaticImageAsset(asset_id);
    ASSERT_FALSE(image_original.isNull());
    gfx::ImageSkia image_reloaded = resources->GetStaticImageAsset(asset_id);
    ASSERT_FALSE(image_reloaded.isNull());
    EXPECT_TRUE(image_reloaded.BackedBySameObjectAs(image_original));
  }
}

TEST(AmbientAnimationStaticResourcesTest, FailsForSlideshowTheme) {
  EXPECT_THAT(AmbientAnimationStaticResources::Create(AmbientTheme::kSlideshow,
                                                      /*serializable=*/false),
              IsNull());
}

TEST(AmbientAnimationStaticResourcesTest, FailsForUnknownAssetId) {
  auto resources = AmbientAnimationStaticResources::Create(
      AmbientTheme::kFeelTheBreeze, /*serializable=*/false);
  ASSERT_THAT(resources, NotNull());
  gfx::ImageSkia image = resources->GetStaticImageAsset("unknown_asset_id");
  EXPECT_TRUE(image.isNull());
}

}  // namespace ash
