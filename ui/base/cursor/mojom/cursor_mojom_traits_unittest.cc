// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cursor/mojom/cursor_mojom_traits.h"

#include "base/numerics/safe_conversions.h"
#include "skia/public/mojom/bitmap_skbitmap_mojom_traits.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor.mojom.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"
#include "ui/gfx/skia_util.h"

namespace ui {

namespace {

using mojom::CursorType;

bool EchoCursor(const Cursor& in, Cursor* out) {
  return mojom::Cursor::DeserializeFromMessage(
      mojom::Cursor::SerializeAsMessage(&in), out);
}

using CursorStructTraitsTest = testing::Test;

}  // namespace

// Test that basic cursor structs are passed correctly across the wire.
TEST_F(CursorStructTraitsTest, TestBuiltIn) {
  for (int i = base::checked_cast<int>(CursorType::kMinValue);
       i < base::checked_cast<int>(CursorType::kMaxValue); ++i) {
    CursorType type = static_cast<CursorType>(i);
    if (type == CursorType::kCustom) {
      continue;
    }

    const Cursor input(type);
    Cursor output;
    ASSERT_TRUE(EchoCursor(input, &output));
    EXPECT_EQ(type, output.type());
  }
}

// Test that cursor bitmaps and metadata are passed correctly across the wire.
TEST_F(CursorStructTraitsTest, TestBitmapCursor) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(10, 10);
  bitmap.eraseColor(SK_ColorRED);
  const gfx::Point kHotspot = gfx::Point(5, 2);
  const float kScale = 2.0f;

  const Cursor input = Cursor::NewCustom(std::move(bitmap), kHotspot, kScale);
  Cursor output;
  ASSERT_TRUE(EchoCursor(input, &output));
  EXPECT_EQ(input, output);

  EXPECT_EQ(CursorType::kCustom, output.type());
  EXPECT_EQ(kHotspot, output.custom_hotspot());
  EXPECT_EQ(kScale, output.image_scale_factor());

  // Even though the pixel data is the same, the bitmap generation ids differ.
  EXPECT_TRUE(
      gfx::BitmapsAreEqual(input.custom_bitmap(), output.custom_bitmap()));
  EXPECT_NE(input.custom_bitmap().getGenerationID(),
            output.custom_bitmap().getGenerationID());

  // Make a copy of output; the bitmap generation ids should be the same.
  const Cursor copy = output;
  EXPECT_EQ(output.custom_bitmap().getGenerationID(),
            copy.custom_bitmap().getGenerationID());
  EXPECT_EQ(copy, output);
}

// Test that empty bitmaps are passed correctly over the wire. This happens when
// renderers relay a custom cursor before the bitmap resource is loaded.
TEST_F(CursorStructTraitsTest, TestEmptyCursor) {
  gfx::Point hotspot = gfx::Point(5, 2);
  const float kScale = 2.0f;
  const Cursor input =
      Cursor::NewCustom(SkBitmap(), std::move(hotspot), kScale);
  Cursor output;

  ASSERT_TRUE(EchoCursor(input, &output));
  EXPECT_TRUE(output.custom_bitmap().empty());
}

// Test that various device scale factors are passed correctly over the wire.
TEST_F(CursorStructTraitsTest, TestDeviceScaleFactors) {
  Cursor output;
  for (auto scale : {0.525f, 0.75f, 0.9f, 1.0f, 2.1f, 2.5f, 3.0f, 10.0f}) {
    SCOPED_TRACE(testing::Message() << " scale: " << scale);
    const Cursor input = Cursor::NewCustom(SkBitmap(), gfx::Point(), scale);
    ASSERT_TRUE(EchoCursor(input, &output));
    EXPECT_EQ(scale, output.image_scale_factor());
  }
}

}  // namespace ui
