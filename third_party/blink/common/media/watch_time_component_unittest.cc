// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/media/watch_time_component.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/media/display_type.h"

namespace blink {

class WatchTimeInterceptor : public media::mojom::WatchTimeRecorder {
 public:
  WatchTimeInterceptor() = default;
  ~WatchTimeInterceptor() override = default;

  // media::mojom::WatchTimeRecorder implementation:
  MOCK_METHOD2(RecordWatchTime, void(media::WatchTimeKey, base::TimeDelta));
  MOCK_METHOD1(FinalizeWatchTime,
               void(const std::vector<media::WatchTimeKey>&));
  MOCK_METHOD1(OnError, void(const media::PipelineStatus&));
  MOCK_METHOD1(SetAutoplayInitiated, void(bool));
  MOCK_METHOD1(OnDurationChanged, void(base::TimeDelta));
  MOCK_METHOD2(UpdateVideoDecodeStats, void(uint32_t, uint32_t));
  MOCK_METHOD1(UpdateUnderflowCount, void(int32_t));
  MOCK_METHOD2(UpdateUnderflowDuration, void(int32_t, base::TimeDelta));
  MOCK_METHOD1(UpdateSecondaryProperties,
               void(media::mojom::SecondaryPlaybackPropertiesPtr));
  MOCK_METHOD1(OnCurrentTimestampChanged, void(base::TimeDelta));
};

class WatchTimeComponentTest : public testing::Test {
 public:
  WatchTimeComponentTest() = default;
  WatchTimeComponentTest(const WatchTimeComponentTest&) = delete;
  WatchTimeComponentTest& operator=(const WatchTimeComponentTest&) = delete;
  ~WatchTimeComponentTest() override = default;

 protected:
  template <typename T>
  std::unique_ptr<WatchTimeComponent<T>> CreateComponent(
      T initial_value,
      std::vector<media::WatchTimeKey> keys_to_finalize,
      typename WatchTimeComponent<T>::ValueToKeyCB value_to_key_cb) {
    return std::make_unique<WatchTimeComponent<T>>(
        initial_value, std::move(keys_to_finalize), std::move(value_to_key_cb),
        base::BindRepeating(&WatchTimeComponentTest::GetMediaTime,
                            base::Unretained(this)),
        &recorder_);
  }

  MOCK_METHOD0(GetMediaTime, base::TimeDelta(void));

  // Usage of StrictMock is intentional here. This ensures all mock method calls
  // are accounted for in tests.
  testing::StrictMock<WatchTimeInterceptor> recorder_;
};

// Components should be key agnostic so just choose an arbitrary key for running
// most of the tests.
constexpr media::WatchTimeKey kTestKey = media::WatchTimeKey::kAudioAll;

// This is a test of the standard flow for most components. Most components will
// be created, be enabled, start reporting, record watch time, be disabled,
// report a finalize, and then record watch time again.
TEST_F(WatchTimeComponentTest, BasicFlow) {
  auto test_component = CreateComponent<bool>(
      false, {kTestKey}, WatchTimeComponent<bool>::ValueToKeyCB());
  EXPECT_FALSE(test_component->current_value_for_testing());
  EXPECT_FALSE(test_component->NeedsFinalize());
  EXPECT_EQ(test_component->end_timestamp(), media::kNoTimestamp);

  // Simulate flag enabled after construction, but before timer is running; this
  // should set the current value immediately.
  test_component->SetCurrentValue(true);
  EXPECT_TRUE(test_component->current_value_for_testing());
  EXPECT_FALSE(test_component->NeedsFinalize());
  EXPECT_EQ(test_component->end_timestamp(), media::kNoTimestamp);

  // Notify the start of reporting to set the starting timestamp.
  const base::TimeDelta kStartTime = base::Seconds(1);
  test_component->OnReportingStarted(kStartTime);
  EXPECT_TRUE(test_component->current_value_for_testing());
  EXPECT_FALSE(test_component->NeedsFinalize());
  EXPECT_EQ(test_component->end_timestamp(), media::kNoTimestamp);

  // Simulate a single recording tick.
  const base::TimeDelta kWatchTime = base::Seconds(2);
  EXPECT_CALL(recorder_, RecordWatchTime(kTestKey, kWatchTime - kStartTime));
  test_component->RecordWatchTime(kWatchTime);
  EXPECT_TRUE(test_component->current_value_for_testing());
  EXPECT_FALSE(test_component->NeedsFinalize());
  EXPECT_EQ(test_component->end_timestamp(), media::kNoTimestamp);

  // Simulate the flag being flipped to false while the timer is running; which
  // should trigger a finalize, but not yet set the current value.
  const base::TimeDelta kFinalWatchTime = base::Seconds(3);
  EXPECT_CALL(*this, GetMediaTime()).WillOnce(testing::Return(kFinalWatchTime));
  test_component->SetPendingValue(false);
  EXPECT_TRUE(test_component->current_value_for_testing());
  EXPECT_TRUE(test_component->NeedsFinalize());
  EXPECT_EQ(test_component->end_timestamp(), kFinalWatchTime);

  // If record is called again it should use the finalize timestamp instead of
  // whatever timestamp we provide.
  EXPECT_CALL(recorder_,
              RecordWatchTime(kTestKey, kFinalWatchTime - kStartTime));
  test_component->RecordWatchTime(base::Seconds(1234));
  EXPECT_TRUE(test_component->current_value_for_testing());
  EXPECT_TRUE(test_component->NeedsFinalize());
  EXPECT_EQ(test_component->end_timestamp(), kFinalWatchTime);

  // Calling it twice or more should not change anything; nor even generate a
  // report since that time has already been recorded.
  for (int i = 0; i < 2; ++i) {
    test_component->RecordWatchTime(base::Seconds(1234 + i));
    EXPECT_TRUE(test_component->current_value_for_testing());
    EXPECT_TRUE(test_component->NeedsFinalize());
    EXPECT_EQ(test_component->end_timestamp(), kFinalWatchTime);
  }

  // Trigger finalize which should transition the pending value to the current
  // value as well as clear the finalize.
  std::vector<media::WatchTimeKey> finalize_keys;
  test_component->Finalize(&finalize_keys);
  EXPECT_FALSE(test_component->current_value_for_testing());
  EXPECT_FALSE(test_component->NeedsFinalize());
  EXPECT_EQ(test_component->end_timestamp(), media::kNoTimestamp);
  ASSERT_EQ(finalize_keys.size(), 1u);
  EXPECT_EQ(finalize_keys[0], kTestKey);

  // The start timestamps should be equal to the previous end timestamp now, so
  // if we call RecordWatchTime again, the value should be relative.
  const base::TimeDelta kNewWatchTime = base::Seconds(4);
  EXPECT_CALL(recorder_,
              RecordWatchTime(kTestKey, kNewWatchTime - kFinalWatchTime));
  test_component->RecordWatchTime(kNewWatchTime);
  EXPECT_FALSE(test_component->current_value_for_testing());
  EXPECT_FALSE(test_component->NeedsFinalize());
  EXPECT_EQ(test_component->end_timestamp(), media::kNoTimestamp);
}

TEST_F(WatchTimeComponentTest, SetCurrentValue) {
  auto test_component = CreateComponent<bool>(
      true, {kTestKey}, WatchTimeComponent<bool>::ValueToKeyCB());
  EXPECT_TRUE(test_component->current_value_for_testing());
  EXPECT_FALSE(test_component->NeedsFinalize());
  EXPECT_EQ(test_component->end_timestamp(), media::kNoTimestamp);

  // An update when the timer isn't running should take effect immediately.
  test_component->SetCurrentValue(false);
  EXPECT_FALSE(test_component->current_value_for_testing());
  EXPECT_FALSE(test_component->NeedsFinalize());
  EXPECT_EQ(test_component->end_timestamp(), media::kNoTimestamp);

  test_component->SetCurrentValue(true);
  EXPECT_TRUE(test_component->current_value_for_testing());
  EXPECT_FALSE(test_component->NeedsFinalize());
  EXPECT_EQ(test_component->end_timestamp(), media::kNoTimestamp);
}

TEST_F(WatchTimeComponentTest, RecordDuringFinalizeRespectsCurrentTime) {
  auto test_component = CreateComponent<bool>(
      true, {kTestKey}, WatchTimeComponent<bool>::ValueToKeyCB());
  EXPECT_TRUE(test_component->current_value_for_testing());
  EXPECT_FALSE(test_component->NeedsFinalize());
  EXPECT_EQ(test_component->end_timestamp(), media::kNoTimestamp);

  // Simulate the flag being flipped to false while the timer is running; which
  // should trigger a finalize, but not yet set the current value.
  const base::TimeDelta kWatchTime1 = base::Seconds(3);
  EXPECT_CALL(*this, GetMediaTime()).WillOnce(testing::Return(kWatchTime1));
  test_component->SetPendingValue(false);
  EXPECT_TRUE(test_component->current_value_for_testing());
  EXPECT_TRUE(test_component->NeedsFinalize());
  EXPECT_EQ(test_component->end_timestamp(), kWatchTime1);

  // Now issue a RecordWatchTime() call with a media time before the finalize
  // time. This can happen when the TimeDelta provided to RecordWatchTime has
  // been clamped for some reason (e.g., a superseding finalize).
  const base::TimeDelta kWatchTime2 = base::Seconds(2);
  EXPECT_CALL(recorder_, RecordWatchTime(kTestKey, kWatchTime2));
  test_component->RecordWatchTime(kWatchTime2);
}

TEST_F(WatchTimeComponentTest, SetPendingValue) {
  auto test_component = CreateComponent<bool>(
      true, {kTestKey}, WatchTimeComponent<bool>::ValueToKeyCB());
  EXPECT_TRUE(test_component->current_value_for_testing());
  EXPECT_FALSE(test_component->NeedsFinalize());
  EXPECT_EQ(test_component->end_timestamp(), media::kNoTimestamp);

  // A change when running should trigger a finalize.
  const base::TimeDelta kFinalWatchTime = base::Seconds(1);
  EXPECT_CALL(*this, GetMediaTime()).WillOnce(testing::Return(kFinalWatchTime));
  test_component->SetPendingValue(false);
  EXPECT_TRUE(test_component->current_value_for_testing());
  EXPECT_TRUE(test_component->NeedsFinalize());
  EXPECT_EQ(test_component->end_timestamp(), kFinalWatchTime);

  // Issuing the same property change again should do nothing since there's a
  // pending finalize already.
  test_component->SetPendingValue(false);
  EXPECT_TRUE(test_component->current_value_for_testing());
  EXPECT_TRUE(test_component->NeedsFinalize());
  EXPECT_EQ(test_component->end_timestamp(), kFinalWatchTime);

  // Changing the value back, should cancel the finalize.
  test_component->SetPendingValue(true);
  EXPECT_TRUE(test_component->current_value_for_testing());
  EXPECT_FALSE(test_component->NeedsFinalize());
  EXPECT_EQ(test_component->end_timestamp(), media::kNoTimestamp);
}

// Tests RecordWatchTime() behavior when a ValueToKeyCB is provided.
TEST_F(WatchTimeComponentTest, WithValueToKeyCB) {
  const std::vector<media::WatchTimeKey> finalize_keys = {
      media::WatchTimeKey::kAudioVideoDisplayInline,
      media::WatchTimeKey::kAudioVideoDisplayFullscreen,
      media::WatchTimeKey::kAudioVideoDisplayPictureInPicture};
  auto test_component = CreateComponent<DisplayType>(
      DisplayType::kFullscreen, finalize_keys,
      base::BindRepeating([](DisplayType display_type) {
        switch (display_type) {
          case DisplayType::kInline:
            return media::WatchTimeKey::kAudioVideoDisplayInline;
          case DisplayType::kFullscreen:
            return media::WatchTimeKey::kAudioVideoDisplayFullscreen;
          case DisplayType::kPictureInPicture:
            return media::WatchTimeKey::kAudioVideoDisplayPictureInPicture;
        }
      }));
  EXPECT_EQ(test_component->current_value_for_testing(),
            DisplayType::kFullscreen);
  EXPECT_FALSE(test_component->NeedsFinalize());
  EXPECT_EQ(test_component->end_timestamp(), media::kNoTimestamp);

  // Notify the start of reporting to set the starting timestamp.
  const base::TimeDelta kStartTime = base::Seconds(1);
  test_component->OnReportingStarted(kStartTime);
  EXPECT_EQ(test_component->current_value_for_testing(),
            DisplayType::kFullscreen);
  EXPECT_FALSE(test_component->NeedsFinalize());
  EXPECT_EQ(test_component->end_timestamp(), media::kNoTimestamp);

  // Record and verify the key recorded too matches the callback provided.
  const base::TimeDelta kWatchTime1 = base::Seconds(2);
  EXPECT_CALL(recorder_,
              RecordWatchTime(media::WatchTimeKey::kAudioVideoDisplayFullscreen,
                              kWatchTime1 - kStartTime));
  test_component->RecordWatchTime(kWatchTime1);
  EXPECT_EQ(test_component->current_value_for_testing(),
            DisplayType::kFullscreen);
  EXPECT_FALSE(test_component->NeedsFinalize());
  EXPECT_EQ(test_component->end_timestamp(), media::kNoTimestamp);

  // Change property while saying the timer isn't running to avoid finalize.
  const base::TimeDelta kWatchTime2 = base::Seconds(3);
  test_component->SetCurrentValue(DisplayType::kInline);
  EXPECT_CALL(recorder_,
              RecordWatchTime(media::WatchTimeKey::kAudioVideoDisplayInline,
                              kWatchTime2 - kStartTime));
  test_component->RecordWatchTime(kWatchTime2);
  EXPECT_EQ(test_component->current_value_for_testing(), DisplayType::kInline);
  EXPECT_FALSE(test_component->NeedsFinalize());
  EXPECT_EQ(test_component->end_timestamp(), media::kNoTimestamp);

  // Cycle through all three properties...
  const base::TimeDelta kWatchTime3 = base::Seconds(4);
  test_component->SetCurrentValue(DisplayType::kPictureInPicture);
  EXPECT_CALL(
      recorder_,
      RecordWatchTime(media::WatchTimeKey::kAudioVideoDisplayPictureInPicture,
                      kWatchTime3 - kStartTime));
  test_component->RecordWatchTime(kWatchTime3);
  EXPECT_EQ(test_component->current_value_for_testing(),
            DisplayType::kPictureInPicture);
  EXPECT_FALSE(test_component->NeedsFinalize());
  EXPECT_EQ(test_component->end_timestamp(), media::kNoTimestamp);

  // Verify finalize sends all three keys.
  std::vector<media::WatchTimeKey> actual_finalize_keys;
  const base::TimeDelta kFinalWatchTime = base::Seconds(5);
  EXPECT_CALL(*this, GetMediaTime()).WillOnce(testing::Return(kFinalWatchTime));
  test_component->SetPendingValue(DisplayType::kFullscreen);
  test_component->Finalize(&actual_finalize_keys);
  ASSERT_EQ(actual_finalize_keys.size(), finalize_keys.size());
  for (size_t i = 0; i < finalize_keys.size(); ++i)
    EXPECT_EQ(actual_finalize_keys[i], finalize_keys[i]);
}

// Unlike WatchTimeReporter, WatchTimeComponents have no automatic finalization
// so creating and destroying one without calls, should do nothing.
TEST_F(WatchTimeComponentTest, NoAutomaticFinalize) {
  auto test_component = CreateComponent<bool>(
      false, {kTestKey}, WatchTimeComponent<bool>::ValueToKeyCB());
}

}  // namespace blink
