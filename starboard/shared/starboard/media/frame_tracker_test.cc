#include "starboard/shared/starboard/media/frame_tracker.h"

#include <sstream>
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard::shared::starboard::media {
namespace {

constexpr int kMaxFrames = 6;

TEST(FrameTrackerTest, InitialState) {
  FrameTracker frame_tracker(kMaxFrames);

  FrameTracker::State status = frame_tracker.GetCurrentState();

  EXPECT_EQ(status.decoding_frames, 0);
  EXPECT_EQ(status.decoded_frames, 0);
  EXPECT_EQ(status.decoding_frames_high_water_mark, 0);
  EXPECT_EQ(status.decoded_frames_high_water_mark, 0);
  EXPECT_EQ(status.total_frames_high_water_mark, 0);
}

TEST(FrameTrackerTest, AddFrame) {
  FrameTracker frame_tracker(kMaxFrames);

  frame_tracker.AddFrame();
  FrameTracker::State status = frame_tracker.GetCurrentState();

  EXPECT_EQ(status.decoding_frames, 1);
  EXPECT_EQ(status.decoded_frames, 0);
  EXPECT_EQ(status.decoding_frames_high_water_mark, 1);
  EXPECT_EQ(status.decoded_frames_high_water_mark, 0);
  EXPECT_EQ(status.total_frames_high_water_mark, 1);
}

TEST(FrameTrackerTest, SetFrameDecoded) {
  FrameTracker frame_tracker(kMaxFrames);
  frame_tracker.AddFrame();

  frame_tracker.SetFrameDecoded();
  FrameTracker::State status = frame_tracker.GetCurrentState();

  EXPECT_EQ(status.decoding_frames, 0);
  EXPECT_EQ(status.decoded_frames, 1);
  EXPECT_EQ(status.decoding_frames_high_water_mark, 1);
  EXPECT_EQ(status.decoded_frames_high_water_mark, 1);
  EXPECT_EQ(status.total_frames_high_water_mark, 1);
}

TEST(FrameTrackerTest, ReleaseFrame) {
  FrameTracker frame_tracker(kMaxFrames);
  frame_tracker.AddFrame();
  frame_tracker.SetFrameDecoded();

  frame_tracker.ReleaseFrame();
  FrameTracker::State status = frame_tracker.GetCurrentState();

  EXPECT_EQ(status.decoding_frames, 0);
  EXPECT_EQ(status.decoded_frames, 0);
  EXPECT_EQ(status.decoding_frames_high_water_mark, 1);
  EXPECT_EQ(status.decoded_frames_high_water_mark, 1);
  EXPECT_EQ(status.total_frames_high_water_mark, 1);
}

TEST(FrameTrackerTest, HighWaterMark) {
  FrameTracker frame_tracker(kMaxFrames);
  frame_tracker.AddFrame();
  frame_tracker.AddFrame();
  frame_tracker.SetFrameDecoded();
  frame_tracker.SetFrameDecoded();
  frame_tracker.ReleaseFrame();

  FrameTracker::State status = frame_tracker.GetCurrentState();

  EXPECT_EQ(status.decoding_frames, 0);
  EXPECT_EQ(status.decoded_frames, 1);
  EXPECT_EQ(status.decoding_frames_high_water_mark, 2);
  EXPECT_EQ(status.decoded_frames_high_water_mark, 2);
  EXPECT_EQ(status.total_frames_high_water_mark, 2);
}

TEST(FrameTrackerTest, Reset) {
  FrameTracker frame_tracker(kMaxFrames);
  frame_tracker.AddFrame();
  frame_tracker.SetFrameDecoded();
  frame_tracker.ReleaseFrame();

  frame_tracker.Reset();
  FrameTracker::State status = frame_tracker.GetCurrentState();

  EXPECT_EQ(status.decoding_frames, 0);
  EXPECT_EQ(status.decoded_frames, 0);
  EXPECT_EQ(status.decoding_frames_high_water_mark, 0);
  EXPECT_EQ(status.decoded_frames_high_water_mark, 0);
  EXPECT_EQ(status.total_frames_high_water_mark, 0);
}

TEST(FrameTrackerTest, GetDeferredInputBuffer) {
  FrameTracker frame_tracker(kMaxFrames);
  frame_tracker.DeferInputBuffer(1);
  frame_tracker.DeferInputBuffer(2);

  std::optional<int> buffer1 = frame_tracker.GetDeferredInputBuffer();
  std::optional<int> buffer2 = frame_tracker.GetDeferredInputBuffer();
  std::optional<int> buffer3 = frame_tracker.GetDeferredInputBuffer();

  EXPECT_TRUE(buffer1);
  EXPECT_EQ(*buffer1, 1);
  EXPECT_TRUE(buffer2);
  EXPECT_EQ(*buffer2, 2);
  EXPECT_FALSE(buffer3);
}

TEST(FrameTrackerTest, StreamInsertionOperator) {
  FrameTracker::State status;
  status.decoding_frames = 1;
  status.decoded_frames = 2;
  status.decoding_frames_high_water_mark = 3;
  status.decoded_frames_high_water_mark = 4;
  status.total_frames_high_water_mark = 5;

  std::stringstream ss;
  ss << status;

  EXPECT_EQ(ss.str(),
            "{decoding: 1, decoded: 2, decoding (hw): 3, decoded (hw): 4, "
            "total (hw): 5}");
}

}  // namespace
}  // namespace starboard::shared::starboard::media
