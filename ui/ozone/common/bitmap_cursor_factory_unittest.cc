// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/common/bitmap_cursor_factory.h"

#include "build/chromeos_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/cursor/platform_cursor.h"
#include "ui/ozone/common/bitmap_cursor.h"

namespace ui {

using mojom::CursorType;

TEST(BitmapCursorFactoryTest, InvisibleCursor) {
  BitmapCursorFactory cursor_factory;

  auto cursor = cursor_factory.GetDefaultCursor(CursorType::kNone);
  // The invisible cursor should be a `BitmapCursor` of type kNone, not nullptr.
  ASSERT_NE(cursor, nullptr);
  EXPECT_EQ(BitmapCursor::FromPlatformCursor(cursor)->type(),
            CursorType::kNone);
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
TEST(BitmapCursorFactoryTest, LacrosUsesDefaultCursorsForCommonTypes) {
  BitmapCursorFactory factory;

  // Verify some common cursor types.
  auto cursor = factory.GetDefaultCursor(CursorType::kPointer);
  EXPECT_NE(cursor, nullptr);
  EXPECT_EQ(BitmapCursor::FromPlatformCursor(cursor)->type(),
            CursorType::kPointer);

  cursor = factory.GetDefaultCursor(CursorType::kHand);
  EXPECT_NE(cursor, nullptr);
  EXPECT_EQ(BitmapCursor::FromPlatformCursor(cursor)->type(),
            CursorType::kHand);

  cursor = factory.GetDefaultCursor(CursorType::kIBeam);
  EXPECT_NE(cursor, nullptr);
  EXPECT_EQ(BitmapCursor::FromPlatformCursor(cursor)->type(),
            CursorType::kIBeam);
}

TEST(BitmapCursorFactoryTest, LacrosCustomCursor) {
  BitmapCursorFactory factory;
  auto cursor = factory.GetDefaultCursor(CursorType::kCustom);
  // Custom cursors don't have a default platform cursor.
  EXPECT_EQ(cursor, nullptr);
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace ui
