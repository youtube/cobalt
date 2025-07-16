#include "starboard/shared/starboard/media/memory_tracker.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard::shared::starboard::media {
namespace {

int64_t g_current_time = 0;

int64_t GetCurrentMockTimeUs() {
  return g_current_time;
}

TEST(MemoryTrackerTest, InitialState) {
  MemoryTracker tracker(&GetCurrentMockTimeUs);
  EXPECT_EQ(tracker.GetCurrentFrames(), 0);
}

TEST(MemoryTrackerTest, AddAndCountFrames) {
  MemoryTracker tracker(&GetCurrentMockTimeUs);
  EXPECT_TRUE(tracker.AddNewFrame());
  EXPECT_EQ(tracker.GetCurrentFrames(), 1);
  EXPECT_TRUE(tracker.AddNewFrame());
  EXPECT_EQ(tracker.GetCurrentFrames(), 2);
}

TEST(MemoryTrackerTest, AddTooManyFrames) {
  MemoryTracker tracker(&GetCurrentMockTimeUs);
  for (int i = 0; i < 6; ++i) {
    EXPECT_TRUE(tracker.AddNewFrame());
  }
  EXPECT_EQ(tracker.GetCurrentFrames(), 6);
  EXPECT_FALSE(tracker.AddNewFrame());
  EXPECT_EQ(tracker.GetCurrentFrames(), 6);
}

TEST(MemoryTrackerTest, SetFrameExpiration) {
  MemoryTracker tracker(&GetCurrentMockTimeUs);
  tracker.AddNewFrame();
  EXPECT_EQ(tracker.GetCurrentFrames(), 1);
  EXPECT_TRUE(tracker.SetFrameExpiration(g_current_time + 100));
  EXPECT_EQ(tracker.GetCurrentFrames(), 1);
}

TEST(MemoryTrackerTest, FrameExpiration) {
  MemoryTracker tracker(&GetCurrentMockTimeUs);
  tracker.AddNewFrame();
  tracker.SetFrameExpiration(g_current_time + 100);
  EXPECT_EQ(tracker.GetCurrentFrames(), 1);
  g_current_time += 101;
  EXPECT_EQ(tracker.GetCurrentFrames(), 0);
}

TEST(MemoryTrackerTest, AddAfterExpiration) {
  MemoryTracker tracker(&GetCurrentMockTimeUs);
  for (int i = 0; i < 6; ++i) {
    tracker.AddNewFrame();
  }
  EXPECT_FALSE(tracker.AddNewFrame());

  tracker.SetFrameExpiration(g_current_time + 100);
  g_current_time += 101;

  EXPECT_TRUE(tracker.AddNewFrame());
  EXPECT_EQ(tracker.GetCurrentFrames(), 6);
}

}  // namespace
}  // namespace starboard::shared::starboard::media
