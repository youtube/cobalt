// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/device_posture/device_posture_platform_provider_win.h"

#include "base/functional/callback_helpers.h"
#include "base/test/values_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

class DevicePosturePlatformProviderWinTest : public testing::Test {
 protected:
  DevicePosturePlatformProviderWinTest() = default;

  ~DevicePosturePlatformProviderWinTest() override = default;

  static absl::optional<std::vector<gfx::Rect>> ParseViewportSegments(
      const base::Value::List& viewport_segments) {
    return DevicePosturePlatformProviderWin::ParseViewportSegments(
        viewport_segments);
  }

  static absl::optional<mojom::DevicePostureType> ParsePosture(
      base::StringPiece posture_state) {
    return DevicePosturePlatformProviderWin::ParsePosture(posture_state);
  }
};

TEST_F(DevicePosturePlatformProviderWinTest, InvalidPostureData) {
  EXPECT_FALSE(ParsePosture(""));
  EXPECT_FALSE(ParsePosture("test"));
  EXPECT_FALSE(ParsePosture(" LAPTOP"));
}

TEST_F(DevicePosturePlatformProviderWinTest, ValidPostureData) {
  EXPECT_EQ(ParsePosture("MODE_HANDHELD"), mojom::DevicePostureType::kFolded);
  EXPECT_EQ(ParsePosture("MODE_DUAL_ANGLE"), mojom::DevicePostureType::kFolded);
  EXPECT_EQ(ParsePosture("MODE_LAYFLAT_LANDSCAPE"),
            mojom::DevicePostureType::kContinuous);
  EXPECT_EQ(ParsePosture("MODE_LAYFLAT_PORTRAIT"),
            mojom::DevicePostureType::kContinuous);
  EXPECT_EQ(ParsePosture("MODE_TABLETOP"),
            mojom::DevicePostureType::kContinuous);
  EXPECT_EQ(ParsePosture("MODE_LAPTOP_KB"),
            mojom::DevicePostureType::kContinuous);
}

TEST_F(DevicePosturePlatformProviderWinTest, InvalidViewportSegmentsData) {
  base::Value::List list;
  absl::optional<std::vector<gfx::Rect>> result_viewport_segments =
      ParseViewportSegments(list);
  EXPECT_FALSE(result_viewport_segments);

  list = base::test::ParseJsonList(R"([123])");
  result_viewport_segments = ParseViewportSegments(list);
  EXPECT_FALSE(result_viewport_segments);

  list = base::test::ParseJsonList(R"([true])");
  result_viewport_segments = ParseViewportSegments(list);
  EXPECT_FALSE(result_viewport_segments);

  // Segment is not valid data.
  list = base::test::ParseJsonList(R"([
    "A132,232, 123, 22"
  ])");
  result_viewport_segments = ParseViewportSegments(list);
  EXPECT_FALSE(result_viewport_segments);

  // Segment is not a rectangle.
  list = base::test::ParseJsonList(R"([
    "132"
  ])");
  result_viewport_segments = ParseViewportSegments(list);
  EXPECT_FALSE(result_viewport_segments);

  // Segment is not a rectangle.
  list = base::test::ParseJsonList(R"([
    "132, 232"
  ])");
  result_viewport_segments = ParseViewportSegments(list);
  EXPECT_FALSE(result_viewport_segments);

  // Segment is not a rectangle.
  list = base::test::ParseJsonList(R"([
    "132,232, 123"
  ])");
  result_viewport_segments = ParseViewportSegments(list);
  EXPECT_FALSE(result_viewport_segments);

  // Segment is not a rectangle.
  list = base::test::ParseJsonList(R"([
    "132, 232, 123, 123, 123"
  ])");
  result_viewport_segments = ParseViewportSegments(list);
  EXPECT_FALSE(result_viewport_segments);

  // Two segments is invalid data lay out.
  // Either 1 or a multiple of 3.
  list = base::test::ParseJsonList(R"([
    "132, 232, 123, 123, 123",
    "132,232, 123, 123, 123"
  ])");
  result_viewport_segments = ParseViewportSegments(list);
  EXPECT_FALSE(result_viewport_segments);

  // 3 segments but the above are invalid.
  list = base::test::ParseJsonList(R"([
    "132, 232, 123, 123, 123",
    "132,232, 123, 123, 123",
    "12, 12, 12, 12, 12"
  ])");
  result_viewport_segments = ParseViewportSegments(list);
  EXPECT_FALSE(result_viewport_segments);

  // Invalid rectangle.
  list = base::test::ParseJsonList(R"([
    "132, ABC, 123, 123"
  ])");
  result_viewport_segments = ParseViewportSegments(list);
  EXPECT_FALSE(result_viewport_segments);

  // Invalid data. Invalid separators.
  list = base::test::ParseJsonList(R"([
    "132; 123; 123; 123"
  ])");
  result_viewport_segments = ParseViewportSegments(list);
  EXPECT_FALSE(result_viewport_segments);

  // One segment is invalid.
  list = base::test::ParseJsonList(R"([
    "11, 11, 11, 11",
    "22, 22, 22, 22",
    "22, 22, 22, AA"
  ])");
  EXPECT_FALSE(ParseViewportSegments(list));

  // One segment is invalid.
  list = base::test::ParseJsonList(R"([
    "11, 11, 11, 11",
    "22, 22, 22, 22",
    "22, 22, 22A, 22A"
  ])");
  EXPECT_FALSE(ParseViewportSegments(list));
}

TEST_F(DevicePosturePlatformProviderWinTest, ValidViewportSegmentsData) {
  absl::optional<std::vector<gfx::Rect>> result_viewport_segments;
  base::Value::List list = base::test::ParseJsonList(R"([
    "132, 123, 123, 123"
  ])");
  result_viewport_segments = ParseViewportSegments(list);
  EXPECT_TRUE(result_viewport_segments);
  EXPECT_THAT(*result_viewport_segments,
              std::vector<gfx::Rect>{gfx::Rect(132, 123, 123, 123)});

  list = base::test::ParseJsonList(R"([
    "132, 123, 123, 123",
    "12, 34, 56, 78",
    "22, 22,  22, 22  "
  ])");
  result_viewport_segments = ParseViewportSegments(list);
  EXPECT_TRUE(result_viewport_segments);
  EXPECT_THAT(*result_viewport_segments,
              (std::vector<gfx::Rect>{gfx::Rect(132, 123, 123, 123),
                                      gfx::Rect(12, 34, 56, 78),
                                      gfx::Rect(22, 22, 22, 22)}));

  list = base::test::ParseJsonList(R"([
    "132, 123, 123, 123",
    "12, 34, 56, 78",
    "22, 22,  22, 22  ",
    "  333, 333, 333, 333",
    "44, 44, 44    , 44",
    "55, 55, 55, 55  "
  ])");
  result_viewport_segments = ParseViewportSegments(list);
  EXPECT_TRUE(result_viewport_segments);
  EXPECT_THAT(*result_viewport_segments,
              (std::vector<gfx::Rect>{
                  gfx::Rect(132, 123, 123, 123), gfx::Rect(12, 34, 56, 78),
                  gfx::Rect(22, 22, 22, 22), gfx::Rect(333, 333, 333, 333),
                  gfx::Rect(44, 44, 44, 44), gfx::Rect(55, 55, 55, 55)}));
}

}  // namespace device
