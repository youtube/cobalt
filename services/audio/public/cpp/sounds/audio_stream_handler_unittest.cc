// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/public/cpp/sounds/audio_stream_handler.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "media/audio/audio_io.h"
#include "media/audio/simple_sources.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/audio_codecs.h"
#include "media/base/channel_layout.h"
#include "media/base/test_data_util.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/audio/public/cpp/output_device.h"
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
using ::testing::TestParamInfo;
using ::testing::ValuesIn;

constexpr std::string_view kTestBadWavAudioData =
    "RIFF1234WAVEjunkjunkjunkjunk";

constexpr int kTestResourceId = 1;

std::string ReadTestMediaFile(std::string_view file_name) {
  const base::FilePath file_path = media::GetTestDataFilePath(file_name);
  std::string data;
  CHECK(base::ReadFileToString(file_path, &data));
  return data;
}

struct TestParams {
  base::RepeatingCallback<std::string()> data_factory;
  media::AudioCodec codec = media::AudioCodec::kUnknown;
  std::string test_suffix;
};

std::vector<TestParams> GetTestParams() {
  return {
      {.data_factory = base::BindRepeating(&ReadTestMediaFile, "bear_pcm.wav"),
       .codec = media::AudioCodec::kPCM,
       .test_suffix = "Wav"},
      {.data_factory = base::BindRepeating(&ReadTestMediaFile, "bear.flac"),
       .codec = media::AudioCodec::kFLAC,
       .test_suffix = "Flac"},
  };
}

class AudioStreamHandlerTest : public testing::Test {
 public:
  ~AudioStreamHandlerTest() override = default;

 protected:
  ui::MockResourceBundleDelegate& mock_resource_delegate() {
    return mock_resource_delegate_;
  }

 private:
  NiceMock<ui::MockResourceBundleDelegate> mock_resource_delegate_;
  ui::ResourceBundle resource_bundle_{&mock_resource_delegate_};
  ui::ResourceBundle::SharedInstanceSwapperForTesting resource_bundle_swapper_{
      &resource_bundle_};

  base::test::TaskEnvironment task_environment_;
};

TEST_F(AudioStreamHandlerTest, BadDataDoesNotInitialize) {
  EXPECT_CALL(mock_resource_delegate(),
              GetRawDataResource(kTestResourceId, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(kTestBadWavAudioData), Return(true)));

  AudioStreamHandler handler(/*stream_factory_binder=*/base::DoNothing(),
                             kTestResourceId, media::AudioCodec::kPCM);

  // The handler should not be initialized with bad data, and `Play` should
  // return `false`.
  EXPECT_FALSE(handler.IsInitialized());
  EXPECT_FALSE(handler.Play());

  // Call `Stop` to ensure that there is no crash.
  handler.Stop();
}

class AudioStreamHandlerTestWithParams
    : public AudioStreamHandlerTest,
      public testing::WithParamInterface<TestParams> {
 protected:
  std::unique_ptr<AudioStreamHandler> CreateHandler(
      SoundsManager::StreamFactoryBinder binder,
      bool loop = false) {
    const std::string audio_data = GetParam().data_factory.Run();
    EXPECT_CALL(mock_resource_delegate(),
                GetRawDataResource(kTestResourceId, _, _))
        .WillOnce(DoAll(SetArgPointee<2>(audio_data), Return(true)));
    return std::make_unique<AudioStreamHandler>(
        std::move(binder), kTestResourceId, GetParam().codec, loop);
  }
};

TEST_P(AudioStreamHandlerTestWithParams, Play) {
  FakeOutputStream fake_output_stream;
  std::unique_ptr<AudioStreamHandler> handler =
      CreateHandler(fake_output_stream.GetBinder());
  ASSERT_NE(handler, nullptr);

  ASSERT_TRUE(handler->IsInitialized());
  ASSERT_TRUE(handler->Play());

  fake_output_stream.ExpectPlay();
  EXPECT_EQ(1, fake_output_stream.play_count());
  int frames = fake_output_stream.ConsumeAllAudioFrames();
  EXPECT_GT(frames, 0);

  handler->Stop();
  fake_output_stream.ExpectPause();
  EXPECT_EQ(1, fake_output_stream.pause_count());
  fake_output_stream.ExpectDisconnect();
}

TEST_P(AudioStreamHandlerTestWithParams, ConsecutivePlayRequests) {
  FakeOutputStream fake_output_stream;
  std::unique_ptr<AudioStreamHandler> handler =
      CreateHandler(fake_output_stream.GetBinder());
  ASSERT_NE(handler, nullptr);

  ASSERT_TRUE(handler->IsInitialized());
  ASSERT_TRUE(handler->Play());

  fake_output_stream.ExpectPlay();
  EXPECT_EQ(1, fake_output_stream.play_count());

  // Second play request should be ignored.
  EXPECT_TRUE(handler->Play());

  // Stop the handler.
  handler->Stop();
  fake_output_stream.ExpectPause();
  EXPECT_EQ(1, fake_output_stream.pause_count());
  fake_output_stream.ExpectDisconnect();
}

TEST_P(AudioStreamHandlerTestWithParams, PlayWithLoop) {
  FakeOutputStream fake_output_stream;
  std::unique_ptr<AudioStreamHandler> handler =
      CreateHandler(fake_output_stream.GetBinder(), /*loop=*/true);
  ASSERT_NE(handler, nullptr);

  ASSERT_TRUE(handler->IsInitialized());
  ASSERT_TRUE(handler->Play());

  fake_output_stream.ExpectPlay();
  EXPECT_EQ(1, fake_output_stream.play_count());

  // Read 10 buffers to verify it loops.
  int frames = fake_output_stream.ReadBuffers(10);
  EXPECT_EQ(frames, 10 * 1024);

  handler->Stop();
  fake_output_stream.ExpectPause();
  EXPECT_EQ(1, fake_output_stream.pause_count());
  fake_output_stream.ExpectDisconnect();
}

TEST_P(AudioStreamHandlerTestWithParams, PauseAndResume) {
  int baseline_frames = 0;
  {
    FakeOutputStream fake_output_stream;
    std::unique_ptr<AudioStreamHandler> handler =
        CreateHandler(fake_output_stream.GetBinder());
    ASSERT_TRUE(handler->Play());
    fake_output_stream.ExpectPlay();
    EXPECT_EQ(1, fake_output_stream.play_count());
    baseline_frames = fake_output_stream.ConsumeAllAudioFrames();
    EXPECT_GT(baseline_frames, 0);
    handler->Stop();
    fake_output_stream.ExpectPause();
    EXPECT_EQ(1, fake_output_stream.pause_count());
    fake_output_stream.ExpectDisconnect();
  }

  FakeOutputStream fake_output_stream;
  std::unique_ptr<AudioStreamHandler> handler =
      CreateHandler(fake_output_stream.GetBinder());
  ASSERT_NE(handler, nullptr);

  ASSERT_TRUE(handler->IsInitialized());
  ASSERT_TRUE(handler->Play());
  fake_output_stream.ExpectPlay();
  EXPECT_EQ(1, fake_output_stream.play_count());

  // Read only 1 buffer to simulate pause on first render.
  const int frames_before_pause = fake_output_stream.ReadBuffers(1);
  ASSERT_EQ(frames_before_pause, 1024);

  EXPECT_TRUE(handler->Pause());
  fake_output_stream.ExpectPause();
  EXPECT_EQ(1, fake_output_stream.pause_count());
  fake_output_stream.ExpectDisconnect();

  // Resume play.
  ASSERT_TRUE(handler->Play());
  fake_output_stream.ExpectPlay();
  EXPECT_EQ(2, fake_output_stream.play_count());

  int remaining_frames = fake_output_stream.ConsumeAllAudioFrames();
  handler->Stop();
  fake_output_stream.ExpectPause();
  EXPECT_EQ(2, fake_output_stream.pause_count());
  fake_output_stream.ExpectDisconnect();

  const int total_frames = frames_before_pause + remaining_frames;
  EXPECT_GT(total_frames, 0);
  EXPECT_GT(frames_before_pause, 0);
  EXPECT_GT(total_frames, frames_before_pause);
  EXPECT_EQ(total_frames, baseline_frames);
}

TEST_P(AudioStreamHandlerTestWithParams, StopAndPlay) {
  FakeOutputStream fake_output_stream;
  std::unique_ptr<AudioStreamHandler> handler =
      CreateHandler(fake_output_stream.GetBinder());
  ASSERT_NE(handler, nullptr);

  ASSERT_TRUE(handler->IsInitialized());
  ASSERT_TRUE(handler->Play());
  fake_output_stream.ExpectPlay();
  EXPECT_EQ(1, fake_output_stream.play_count());
  int frames1 = fake_output_stream.ConsumeAllAudioFrames();
  EXPECT_GT(frames1, 0);

  handler->Stop();
  fake_output_stream.ExpectPause();
  EXPECT_EQ(1, fake_output_stream.pause_count());
  fake_output_stream.ExpectDisconnect();

  // Play again on the same handler.
  ASSERT_TRUE(handler->Play());
  fake_output_stream.ExpectPlay();
  EXPECT_EQ(2, fake_output_stream.play_count());
  int frames2 = fake_output_stream.ConsumeAllAudioFrames();
  EXPECT_EQ(frames2, frames1);

  handler->Stop();
  fake_output_stream.ExpectPause();
  EXPECT_EQ(2, fake_output_stream.pause_count());
  fake_output_stream.ExpectDisconnect();
}

INSTANTIATE_TEST_SUITE_P(,
                         AudioStreamHandlerTestWithParams,
                         ValuesIn(GetTestParams()),
                         [](const TestParamInfo<TestParams>& info) {
                           return info.param.test_suffix;
                         });

}  // namespace
}  // namespace audio
