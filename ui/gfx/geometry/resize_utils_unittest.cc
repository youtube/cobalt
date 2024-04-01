// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/resize_utils.h"

#include <string>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {
namespace {

// Aspect ratio is defined by width / height.
constexpr float kAspectRatioSquare = 1.0f;
constexpr float kAspectRatioHorizontal = 2.0f;
constexpr float kAspectRatioVertical = 0.5f;

constexpr Size kMinSizeHorizontal(20, 10);
constexpr Size kMaxSizeHorizontal(50, 25);

constexpr Size kMinSizeVertical(10, 20);
constexpr Size kMaxSizeVertical(25, 50);

std::string HitTestToString(ResizeEdge resize_edge) {
  switch (resize_edge) {
    case ResizeEdge::kTop:
      return "top";
    case ResizeEdge::kTopRight:
      return "top-righ";
    case ResizeEdge::kRight:
      return "right";
    case ResizeEdge::kBottomRight:
      return "bottom-right";
    case ResizeEdge::kBottom:
      return "bottom";
    case ResizeEdge::kBottomLeft:
      return "bottom-left";
    case ResizeEdge::kLeft:
      return "left";
    case ResizeEdge::kTopLeft:
      return "top-left";
  }
}

}  // namespace

struct SizingParams {
  ResizeEdge resize_edge{};
  float aspect_ratio = 0.0f;
  Size min_size;
  Size max_size;
  Rect input_rect;
  Rect expected_output_rect;

  std::string ToString() const {
    return base::StrCat({HitTestToString(resize_edge),
                         " ratio=", base::NumberToString(aspect_ratio), " [",
                         min_size.ToString(), "..", max_size.ToString(), "] ",
                         input_rect.ToString(), " -> ",
                         expected_output_rect.ToString()});
  }
};

using ResizeUtilsTest = testing::TestWithParam<SizingParams>;

TEST_P(ResizeUtilsTest, SizeRectToAspectRatio) {
  Rect rect = GetParam().input_rect;
  SizeRectToAspectRatio(GetParam().resize_edge, GetParam().aspect_ratio,
                        GetParam().min_size, GetParam().max_size, &rect);
  EXPECT_EQ(rect, GetParam().expected_output_rect) << GetParam().ToString();
}

const SizingParams kSizeRectToSquareAspectRatioTestCases[] = {
    // Dragging the top resizer up.
    {ResizeEdge::kTop, kAspectRatioSquare, kMinSizeHorizontal,
     kMaxSizeHorizontal, Rect(100, 98, 22, 24), Rect(100, 98, 24, 24)},

    // Dragging the bottom resizer down.
    {ResizeEdge::kBottom, kAspectRatioSquare, kMinSizeHorizontal,
     kMaxSizeHorizontal, Rect(100, 100, 22, 24), Rect(100, 100, 24, 24)},

    // Dragging the left resizer right.
    {ResizeEdge::kLeft, kAspectRatioSquare, kMinSizeHorizontal,
     kMaxSizeHorizontal, Rect(102, 100, 22, 24), Rect(102, 102, 22, 22)},

    // Dragging the right resizer left.
    {ResizeEdge::kRight, kAspectRatioSquare, kMinSizeHorizontal,
     kMaxSizeHorizontal, Rect(100, 100, 22, 24), Rect(100, 100, 22, 22)},

    // Dragging the top-left resizer right.
    {ResizeEdge::kTopLeft, kAspectRatioSquare, kMinSizeHorizontal,
     kMaxSizeHorizontal, Rect(102, 100, 22, 24), Rect(102, 102, 22, 22)},

    // Dragging the top-right resizer down.
    {ResizeEdge::kTopRight, kAspectRatioSquare, kMinSizeHorizontal,
     kMaxSizeHorizontal, Rect(100, 102, 24, 22), Rect(100, 102, 22, 22)},

    // Dragging the bottom-left resizer right.
    {ResizeEdge::kBottomLeft, kAspectRatioSquare, kMinSizeHorizontal,
     kMaxSizeHorizontal, Rect(100, 102, 22, 24), Rect(100, 102, 22, 22)},

    // Dragging the bottom-right resizer up.
    {ResizeEdge::kBottomRight, kAspectRatioSquare, kMinSizeHorizontal,
     kMaxSizeHorizontal, Rect(100, 100, 24, 22), Rect(100, 100, 22, 22)},

    // Dragging the bottom-right resizer left.
    // Rect already as small as `kMinSizeHorizontal` allows.
    {ResizeEdge::kBottomRight, kAspectRatioSquare, kMinSizeHorizontal,
     kMaxSizeHorizontal,
     Rect(100, 100, kMinSizeHorizontal.width(), kMinSizeHorizontal.width()),
     Rect(100, 100, kMinSizeHorizontal.width(), kMinSizeHorizontal.width())},

    // Dragging the top-left resizer left.
    // Rect already as large as `kMaxSizeHorizontal` allows.
    {ResizeEdge::kTopLeft, kAspectRatioSquare, kMinSizeHorizontal,
     kMaxSizeHorizontal,
     Rect(100, 100, kMaxSizeHorizontal.height(), kMaxSizeHorizontal.height()),
     Rect(100, 100, kMaxSizeHorizontal.height(), kMaxSizeHorizontal.height())},
};

const SizingParams kSizeRectToHorizontalAspectRatioTestCases[] = {
    // Dragging the top resizer down.
    {ResizeEdge::kTop, kAspectRatioHorizontal, kMinSizeHorizontal,
     kMaxSizeHorizontal, Rect(100, 102, 48, 22), Rect(100, 102, 44, 22)},

    // Dragging the left resizer left.
    {ResizeEdge::kLeft, kAspectRatioHorizontal, kMinSizeHorizontal,
     kMaxSizeHorizontal, Rect(96, 100, 48, 22), Rect(96, 98, 48, 24)},

    // Rect already as small as `kMinSizeHorizontal` allows.
    {ResizeEdge::kTop, kAspectRatioHorizontal, kMinSizeHorizontal,
     kMaxSizeHorizontal,
     Rect(100, 100, kMinSizeHorizontal.width(), kMinSizeHorizontal.height()),
     Rect(100, 100, kMinSizeHorizontal.width(), kMinSizeHorizontal.height())},

    // Rect already as large as `kMaxSizeHorizontal` allows.
    {ResizeEdge::kTop, kAspectRatioHorizontal, kMinSizeHorizontal,
     kMaxSizeHorizontal,
     Rect(100, 100, kMaxSizeHorizontal.width(), kMaxSizeHorizontal.height()),
     Rect(100, 100, kMaxSizeHorizontal.width(), kMaxSizeHorizontal.height())},
};

const SizingParams kSizeRectToVerticalAspectRatioTestCases[] = {
    // Dragging the bottom resizer up.
    {ResizeEdge::kBottom, kAspectRatioVertical, kMinSizeVertical,
     kMaxSizeVertical, Rect(100, 100, 24, 44), Rect(100, 100, 22, 44)},

    // Dragging the right resizer right.
    {ResizeEdge::kRight, kAspectRatioVertical, kMinSizeVertical,
     kMaxSizeVertical, Rect(100, 100, 24, 44), Rect(100, 100, 24, 48)},

    // Rect already as small as `kMinSizeVertical` allows.
    {ResizeEdge::kTop, kAspectRatioVertical, kMinSizeVertical, kMaxSizeVertical,
     Rect(100, 100, kMinSizeVertical.width(), kMinSizeVertical.height()),
     Rect(100, 100, kMinSizeVertical.width(), kMinSizeVertical.height())},

    // Rect already as large as `kMaxSizeVertical` allows.
    {ResizeEdge::kTop, kAspectRatioVertical, kMinSizeVertical, kMaxSizeVertical,
     Rect(100, 100, kMaxSizeVertical.width(), kMaxSizeVertical.height()),
     Rect(100, 100, kMaxSizeVertical.width(), kMaxSizeVertical.height())},
};

INSTANTIATE_TEST_SUITE_P(
    Square,
    ResizeUtilsTest,
    testing::ValuesIn(kSizeRectToSquareAspectRatioTestCases));
INSTANTIATE_TEST_SUITE_P(
    Horizontal,
    ResizeUtilsTest,
    testing::ValuesIn(kSizeRectToHorizontalAspectRatioTestCases));
INSTANTIATE_TEST_SUITE_P(
    Vertical,
    ResizeUtilsTest,
    testing::ValuesIn(kSizeRectToVerticalAspectRatioTestCases));

}  // namespace gfx
