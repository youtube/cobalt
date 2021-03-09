// Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

#include <string>

#include "starboard/android/shared/video_frame_tracker.h"
#include "starboard/time.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace android {
namespace shared {
namespace {

TEST(VideoFrameTrackerTest, DroppedFrameCountIsCumulative) {
  VideoFrameTracker video_frame_tracker(100);

  video_frame_tracker.OnInputBuffer(10000);
  video_frame_tracker.OnInputBuffer(20000);
  video_frame_tracker.OnInputBuffer(30000);
  video_frame_tracker.OnInputBuffer(40000);
  video_frame_tracker.OnInputBuffer(50000);
  video_frame_tracker.OnInputBuffer(60000);
  video_frame_tracker.OnInputBuffer(70000);
  video_frame_tracker.OnInputBuffer(80000);

  video_frame_tracker.OnFrameRendered(10000);
  video_frame_tracker.OnFrameRendered(20000);
  // Frame with timestamp 30000 was dropped
  video_frame_tracker.OnFrameRendered(40000);

  EXPECT_EQ(video_frame_tracker.UpdateAndGetDroppedFrames(), 1);
  video_frame_tracker.OnFrameRendered(50000);
  // Frame with timestamp 60000 was dropped
  video_frame_tracker.OnFrameRendered(70000);
  // Frame with timestamp 80000 is not rendered and not dropped.

  EXPECT_EQ(video_frame_tracker.UpdateAndGetDroppedFrames(), 2);
}

TEST(VideoFrameTrackerTest, OnlySkewOverMaxAllowedShouldResultInDroppedFrame) {
  VideoFrameTracker video_frame_tracker(100);

  video_frame_tracker.OnInputBuffer(100000);
  video_frame_tracker.OnInputBuffer(200000);
  video_frame_tracker.OnInputBuffer(300000);
  video_frame_tracker.OnInputBuffer(400000);
  video_frame_tracker.OnInputBuffer(500000);
  video_frame_tracker.OnInputBuffer(600000);

  video_frame_tracker.OnFrameRendered(104999);  // 4999us late -> not dropped
  video_frame_tracker.OnFrameRendered(195001);  // 4999us early -> not dropped
  video_frame_tracker.OnFrameRendered(300000);

  EXPECT_EQ(video_frame_tracker.UpdateAndGetDroppedFrames(), 0);

  video_frame_tracker.OnFrameRendered(394999);  // 5001us early -> dropped
  video_frame_tracker.OnFrameRendered(505001);  // 5001us late -> dropped
  video_frame_tracker.OnFrameRendered(600000);

  EXPECT_EQ(video_frame_tracker.UpdateAndGetDroppedFrames(), 2);
}

TEST(VideoFrameTrackerTest, MultipleFramesDroppedAreReportedCorrectly) {
  VideoFrameTracker video_frame_tracker(100);

  video_frame_tracker.OnInputBuffer(10000);
  video_frame_tracker.OnInputBuffer(20000);
  video_frame_tracker.OnInputBuffer(30000);
  video_frame_tracker.OnInputBuffer(40000);
  video_frame_tracker.OnInputBuffer(50000);
  video_frame_tracker.OnInputBuffer(60000);
  video_frame_tracker.OnInputBuffer(70000);
  video_frame_tracker.OnInputBuffer(80000);
  video_frame_tracker.OnInputBuffer(90000);

  video_frame_tracker.OnFrameRendered(10000);
  video_frame_tracker.OnFrameRendered(20000);
  // Frame with timestamp 30000 was dropped.
  video_frame_tracker.OnFrameRendered(40000);
  // Frames with timestamp 50000, 60000, 70000, 80000 were dropped.
  video_frame_tracker.OnFrameRendered(90000);

  EXPECT_EQ(video_frame_tracker.UpdateAndGetDroppedFrames(), 5);
}

TEST(VideoFrameTrackerTest, RenderQueueIsClearedOnSeek) {
  VideoFrameTracker video_frame_tracker(100);

  video_frame_tracker.OnInputBuffer(10000);
  video_frame_tracker.OnInputBuffer(20000);
  video_frame_tracker.OnInputBuffer(30000);

  // These frames are not rendered but due to the seek below should not count as
  // dropped.
  video_frame_tracker.OnInputBuffer(40000);
  video_frame_tracker.OnInputBuffer(50000);
  video_frame_tracker.OnInputBuffer(60000);

  video_frame_tracker.OnFrameRendered(10000);
  // Frame with timestamp 20000 was dropped and should be registered.
  video_frame_tracker.OnFrameRendered(30000);
  // Frames 40000, 50000, 60000 were never rendered.

  const SbTime kSeekToTime = 90000;
  video_frame_tracker.Seek(kSeekToTime);
  ASSERT_EQ(kSeekToTime, video_frame_tracker.seek_to_time());

  video_frame_tracker.OnInputBuffer(90000);
  video_frame_tracker.OnInputBuffer(100000);
  video_frame_tracker.OnInputBuffer(110000);
  video_frame_tracker.OnInputBuffer(120000);

  video_frame_tracker.OnFrameRendered(90000);
  video_frame_tracker.OnFrameRendered(100000);

  EXPECT_EQ(video_frame_tracker.UpdateAndGetDroppedFrames(), 1);
}

TEST(VideoFrameTrackerTest, UnorderedInputFramesAreHandled) {
  VideoFrameTracker video_frame_tracker(100);

  // Order of input frames should not matter as long as they are before the
  // render timestamp.
  video_frame_tracker.OnInputBuffer(20000);
  video_frame_tracker.OnInputBuffer(30000);
  video_frame_tracker.OnInputBuffer(10000);
  video_frame_tracker.OnInputBuffer(40000);
  video_frame_tracker.OnInputBuffer(60000);
  video_frame_tracker.OnInputBuffer(80000);

  video_frame_tracker.OnFrameRendered(10000);
  video_frame_tracker.OnFrameRendered(20000);
  video_frame_tracker.OnFrameRendered(30000);
  video_frame_tracker.OnFrameRendered(40000);

  // Add more input frames after the first frames are rendered.
  video_frame_tracker.OnInputBuffer(50000);
  video_frame_tracker.OnInputBuffer(70000);
  video_frame_tracker.OnInputBuffer(90000);

  EXPECT_EQ(video_frame_tracker.UpdateAndGetDroppedFrames(), 0);

  video_frame_tracker.OnFrameRendered(50000);
  video_frame_tracker.OnFrameRendered(60000);
  video_frame_tracker.OnFrameRendered(70000);
  video_frame_tracker.OnFrameRendered(80000);

  EXPECT_EQ(video_frame_tracker.UpdateAndGetDroppedFrames(), 0);
}

}  // namespace
}  // namespace shared
}  // namespace android
}  // namespace starboard
