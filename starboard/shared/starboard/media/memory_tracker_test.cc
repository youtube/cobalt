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
  MemoryTracker tracker(&GetCurrentMockTimeUs, 3);
  EXPECT_TRUE(tracker.AddNewFrame());
  EXPECT_TRUE(tracker.AddNewFrame());
  EXPECT_TRUE(tracker.AddNewFrame());
  EXPECT_EQ(tracker.GetCurrentFrames(), 3);
  EXPECT_FALSE(tracker.AddNewFrame());
  EXPECT_EQ(tracker.GetCurrentFrames(), 3);
}

TEST(MemoryTrackerTest, SetFrameExpiration) {
  MemoryTracker tracker(&GetCurrentMockTimeUs);
  tracker.AddNewFrame();
  EXPECT_EQ(tracker.GetCurrentFrames(), 1);
  EXPECT_TRUE(tracker.SetFrameExpiration(g_current_time + 100));
  EXPECT_EQ(tracker.GetCurrentFrames(), 1);
}

TEST(MemoryTrackerTest, SetFrameExpirationNow) {
  MemoryTracker tracker(&GetCurrentMockTimeUs);
  tracker.AddNewFrame();
  EXPECT_EQ(tracker.GetCurrentFrames(), 1);
  g_current_time++;
  EXPECT_TRUE(tracker.SetFrameExpirationNow());
  EXPECT_EQ(tracker.GetCurrentFrames(), 0);
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
  MemoryTracker tracker(&GetCurrentMockTimeUs, 2);
  tracker.AddNewFrame();
  tracker.AddNewFrame();
  EXPECT_FALSE(tracker.AddNewFrame());

  tracker.SetFrameExpiration(g_current_time + 100);
  g_current_time += 101;

  EXPECT_TRUE(tracker.AddNewFrame());
  EXPECT_EQ(tracker.GetCurrentFrames(), 2);
}

TEST(MemoryTrackerTest, Reset) {
  MemoryTracker tracker(&GetCurrentMockTimeUs, 5);
  tracker.AddNewFrame();
  tracker.AddNewFrame();
  tracker.AddNewFrame();
  EXPECT_EQ(tracker.GetCurrentFrames(), 3);
  tracker.Reset();
  EXPECT_EQ(tracker.GetCurrentFrames(), 0);
  EXPECT_TRUE(tracker.AddNewFrame());
  EXPECT_EQ(tracker.GetCurrentFrames(), 1);
}

}  // namespace
}  // namespace starboard::shared::starboard::media
