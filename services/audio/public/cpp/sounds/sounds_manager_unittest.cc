// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/public/cpp/sounds/sounds_manager.h"

#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/compiler_specific.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "media/audio/simple_sources.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/audio_codecs.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/audio/public/cpp/sounds/audio_stream_handler.h"
#include "services/audio/public/cpp/sounds/test_data.h"
#include "services/audio/test/fake_output_stream.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/mock_resource_bundle_delegate.h"
#include "ui/base/resource/resource_bundle.h"

namespace audio {
namespace {

using ::testing::_;
using ::testing::DoAll;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SetArgPointee;

constexpr int kTestResourceId = 1;

class SoundsManagerTest : public testing::Test {
 public:
  SoundsManagerTest() = default;
  ~SoundsManagerTest() override = default;

  void SetUp() override {
    sounds_manager_ = SoundsManager::Create(fake_output_stream_.GetBinder());
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    sounds_manager_.reset();
    base::RunLoop().RunUntilIdle();
  }

 protected:
  SoundsManager& sounds_manager() { return CHECK_DEREF(sounds_manager_); }

  FakeOutputStream& fake_output_stream() { return fake_output_stream_; }

  ui::MockResourceBundleDelegate& mock_resource_delegate() {
    return mock_resource_delegate_;
  }

 private:
  base::test::TaskEnvironment env_;

  FakeOutputStream fake_output_stream_;
  std::unique_ptr<SoundsManager> sounds_manager_;

  NiceMock<ui::MockResourceBundleDelegate> mock_resource_delegate_;
  ui::ResourceBundle resource_bundle_{&mock_resource_delegate_};
  ui::ResourceBundle::SharedInstanceSwapperForTesting resource_bundle_swapper_{
      &resource_bundle_};
};

TEST_F(SoundsManagerTest, Play) {
  EXPECT_CALL(mock_resource_delegate(),
              GetRawDataResource(kTestResourceId, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(std::string_view(
                          kTestAudioData, std::size(kTestAudioData))),
                      Return(true)));

  ASSERT_TRUE(sounds_manager().Initialize(
      kTestAudioKey, kTestResourceId, media::AudioCodec::kPCM, /*loop=*/false));
  ASSERT_EQ(20, sounds_manager().GetDuration(kTestAudioKey).InMicroseconds());
  ASSERT_TRUE(sounds_manager().Play(kTestAudioKey));

  fake_output_stream().ExpectPlay();
  EXPECT_EQ(1, fake_output_stream().play_count());
  int frames = fake_output_stream().ConsumeAllAudioFrames();
  EXPECT_GT(frames, 0);

  sounds_manager().Stop(kTestAudioKey);
  fake_output_stream().ExpectPause();
  EXPECT_EQ(1, fake_output_stream().pause_count());
  fake_output_stream().ExpectDisconnect();
}

TEST_F(SoundsManagerTest, Stop) {
  EXPECT_CALL(mock_resource_delegate(),
              GetRawDataResource(kTestResourceId, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(std::string_view(
                          kTestAudioData, std::size(kTestAudioData))),
                      Return(true)));

  ASSERT_TRUE(sounds_manager().Initialize(
      kTestAudioKey, kTestResourceId, media::AudioCodec::kPCM, /*loop=*/false));

  EXPECT_EQ(0, fake_output_stream().play_count());
  EXPECT_EQ(0, fake_output_stream().pause_count());

  ASSERT_TRUE(sounds_manager().Play(kTestAudioKey));
  fake_output_stream().ExpectPlay();
  EXPECT_EQ(1, fake_output_stream().play_count());

  ASSERT_TRUE(sounds_manager().Stop(kTestAudioKey));
  fake_output_stream().ExpectPause();
  EXPECT_EQ(1, fake_output_stream().pause_count());
  fake_output_stream().ExpectDisconnect();
}

TEST_F(SoundsManagerTest, Pause) {
  EXPECT_CALL(mock_resource_delegate(),
              GetRawDataResource(kTestResourceId, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(std::string_view(
                          kTestAudioData, std::size(kTestAudioData))),
                      Return(true)));

  ASSERT_TRUE(sounds_manager().Initialize(
      kTestAudioKey, kTestResourceId, media::AudioCodec::kPCM, /*loop=*/false));

  EXPECT_EQ(0, fake_output_stream().play_count());
  EXPECT_EQ(0, fake_output_stream().pause_count());

  ASSERT_TRUE(sounds_manager().Play(kTestAudioKey));
  fake_output_stream().ExpectPlay();
  EXPECT_EQ(1, fake_output_stream().play_count());

  ASSERT_TRUE(sounds_manager().Pause(kTestAudioKey));
  fake_output_stream().ExpectPause();
  EXPECT_EQ(1, fake_output_stream().pause_count());
  fake_output_stream().ExpectDisconnect();
}

TEST_F(SoundsManagerTest, Uninitialized) {
  ASSERT_FALSE(sounds_manager().Play(kTestAudioKey));
  ASSERT_FALSE(sounds_manager().Stop(kTestAudioKey));
  ASSERT_FALSE(sounds_manager().Pause(kTestAudioKey));
}

}  // namespace
}  // namespace audio
