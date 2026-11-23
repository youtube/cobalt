// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <limits>
#include <optional>

#include "base/time/time.h"
#include "cobalt/browser/cobalt_browser_main_parts.h"
#include "starboard/extension/graphics.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace {

constexpr base::TimeDelta kRefreshInterval = base::Hertz(60);

CobaltExtensionGraphicsApi GraphicsExtension(
    float (*minimum_frame_interval)(),
    float (*maximum_frame_interval)() = nullptr) {
  CobaltExtensionGraphicsApi graphics_extension = {};
  graphics_extension.name = kCobaltExtensionGraphicsName;
  graphics_extension.version = 2;
  graphics_extension.GetMinimumFrameIntervalInMilliseconds =
      minimum_frame_interval;
  graphics_extension.GetMaximumFrameIntervalInMilliseconds =
      maximum_frame_interval;
  return graphics_extension;
}

// A platform that asks for at most 30 or 20 frames a second on a 60 Hz display
// gets a frame every second or third refresh.
TEST(FrameIntervalTest, MinimumFrameIntervalCapsFrameRate) {
  const CobaltExtensionGraphicsApi capped_at_30 =
      GraphicsExtension([] { return 33.0f; });
  EXPECT_EQ(GetThrottledFrameInterval(&capped_at_30, kRefreshInterval),
            kRefreshInterval * 2);

  const CobaltExtensionGraphicsApi capped_at_20 =
      GraphicsExtension([] { return 50.0f; });
  EXPECT_EQ(GetThrottledFrameInterval(&capped_at_20, kRefreshInterval),
            kRefreshInterval * 3);
}

// A platform whose minimum frame interval is shorter than a display refresh,
// such as 8 ms or 1 ms, is not throttled.
TEST(FrameIntervalTest, ShortMinimumFrameIntervalDoesNotCapFrameRate) {
  const CobaltExtensionGraphicsApi eight_ms =
      GraphicsExtension([] { return 8.0f; });
  EXPECT_EQ(GetThrottledFrameInterval(&eight_ms, kRefreshInterval),
            std::nullopt);

  const CobaltExtensionGraphicsApi one_ms =
      GraphicsExtension([] { return 1.0f; });
  EXPECT_EQ(GetThrottledFrameInterval(&one_ms, kRefreshInterval), std::nullopt);
}

// A platform that sets no minimum frame interval, or has no Graphics
// extension, is not throttled. Neither is any platform if the display's
// refresh interval is not positive.
TEST(FrameIntervalTest, NoMinimumFrameIntervalDoesNotCapFrameRate) {
  const CobaltExtensionGraphicsApi not_set =
      GraphicsExtension([] { return -1.0f; });
  EXPECT_EQ(GetThrottledFrameInterval(&not_set, kRefreshInterval),
            std::nullopt);

  EXPECT_EQ(GetThrottledFrameInterval(nullptr, kRefreshInterval), std::nullopt);

  const CobaltExtensionGraphicsApi capped_at_30 =
      GraphicsExtension([] { return 33.0f; });
  EXPECT_EQ(GetThrottledFrameInterval(&capped_at_30, base::TimeDelta()),
            std::nullopt);
  EXPECT_EQ(GetThrottledFrameInterval(&capped_at_30, -kRefreshInterval),
            std::nullopt);
}

// A platform implementing only version 1 of the Graphics extension (or leaving
// the minimum-interval callback unset) does not expose a frame rate cap and
// stays unthrottled during rendering.
TEST(FrameIntervalTest, VersionOneOrNullCallbackDoesNotCapFrameRate) {
  CobaltExtensionGraphicsApi v1_extension = {};
  v1_extension.name = kCobaltExtensionGraphicsName;
  v1_extension.version = 1;
  EXPECT_EQ(GetThrottledFrameInterval(&v1_extension, kRefreshInterval),
            std::nullopt);

  const CobaltExtensionGraphicsApi null_callback = GraphicsExtension(nullptr);
  EXPECT_EQ(GetThrottledFrameInterval(&null_callback, kRefreshInterval),
            std::nullopt);
}

// Non-finite, zero, or default ~60 Hz intervals leave the app rendering at full
// display refresh rate rather than throttling.
TEST(FrameIntervalTest, InvalidAndDefaultIntervalsDoNotCapFrameRate) {
  const CobaltExtensionGraphicsApi zero =
      GraphicsExtension([] { return 0.0f; });
  EXPECT_EQ(GetThrottledFrameInterval(&zero, kRefreshInterval), std::nullopt);

  const CobaltExtensionGraphicsApi nan =
      GraphicsExtension([] { return std::numeric_limits<float>::quiet_NaN(); });
  EXPECT_EQ(GetThrottledFrameInterval(&nan, kRefreshInterval), std::nullopt);

  const CobaltExtensionGraphicsApi inf =
      GraphicsExtension([] { return std::numeric_limits<float>::infinity(); });
  EXPECT_EQ(GetThrottledFrameInterval(&inf, kRefreshInterval), std::nullopt);

  const CobaltExtensionGraphicsApi sixteen_ms =
      GraphicsExtension([] { return 16.0f; });
  EXPECT_EQ(GetThrottledFrameInterval(&sixteen_ms, kRefreshInterval),
            std::nullopt);
}

// A platform that needs a frame at least every 100 ms gets one that often
// while nothing on screen changes. A maximum frame interval of 0 asks for a
// frame on every display refresh.
TEST(FrameIntervalTest, MaximumFrameIntervalSetsMaxTimeBetweenFrames) {
  const CobaltExtensionGraphicsApi every_100_ms =
      GraphicsExtension([] { return -1.0f; }, [] { return 100.0f; });
  EXPECT_EQ(GetMaxTimeBetweenFrames(&every_100_ms, kRefreshInterval),
            base::Milliseconds(100));

  const CobaltExtensionGraphicsApi every_refresh =
      GraphicsExtension([] { return -1.0f; }, [] { return 0.0f; });
  EXPECT_EQ(GetMaxTimeBetweenFrames(&every_refresh, kRefreshInterval),
            kRefreshInterval);
}

// The minimum frame interval takes precedence: a platform capped at 30 frames
// a second is not presented frames more often than that, whatever its maximum
// frame interval is.
TEST(FrameIntervalTest, MinimumFrameIntervalLimitsMaxTimeBetweenFrames) {
  const CobaltExtensionGraphicsApi capped_at_30_every_refresh =
      GraphicsExtension([] { return 33.0f; }, [] { return 0.0f; });
  EXPECT_EQ(
      GetMaxTimeBetweenFrames(&capped_at_30_every_refresh, kRefreshInterval),
      kRefreshInterval * 2);

  const CobaltExtensionGraphicsApi capped_at_30_every_100_ms =
      GraphicsExtension([] { return 33.0f; }, [] { return 100.0f; });
  EXPECT_EQ(
      GetMaxTimeBetweenFrames(&capped_at_30_every_100_ms, kRefreshInterval),
      base::Milliseconds(100));
}

// A platform that sets no maximum frame interval, has no Graphics extension,
// leaves the callback unset or returns a value that is not a number is only
// presented frames when something on screen changes.
TEST(FrameIntervalTest, NoMaximumFrameIntervalPresentsOnlyOnChange) {
  const CobaltExtensionGraphicsApi not_set =
      GraphicsExtension([] { return -1.0f; }, [] { return -1.0f; });
  EXPECT_EQ(GetMaxTimeBetweenFrames(&not_set, kRefreshInterval), std::nullopt);

  EXPECT_EQ(GetMaxTimeBetweenFrames(nullptr, kRefreshInterval), std::nullopt);

  const CobaltExtensionGraphicsApi null_callback =
      GraphicsExtension([] { return -1.0f; });
  EXPECT_EQ(GetMaxTimeBetweenFrames(&null_callback, kRefreshInterval),
            std::nullopt);

  const CobaltExtensionGraphicsApi nan =
      GraphicsExtension([] { return -1.0f; },
                        [] { return std::numeric_limits<float>::quiet_NaN(); });
  EXPECT_EQ(GetMaxTimeBetweenFrames(&nan, kRefreshInterval), std::nullopt);

  const CobaltExtensionGraphicsApi every_100_ms =
      GraphicsExtension([] { return -1.0f; }, [] { return 100.0f; });
  EXPECT_EQ(GetMaxTimeBetweenFrames(&every_100_ms, base::TimeDelta()),
            std::nullopt);
}

}  // namespace
}  // namespace cobalt
