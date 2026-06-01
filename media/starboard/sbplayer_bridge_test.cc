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

#include "media/starboard/sbplayer_bridge.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/test_helpers.h"
#include "media/base/video_decoder_config.h"
#include "media/starboard/sbplayer_interface.h"
#include "starboard/extension/experimental_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

struct SbPlayerPrivate {
 public:
  SbPlayerPrivate() = default;
  SbPlayerPrivate(const SbPlayerPrivate&) = delete;
  SbPlayerPrivate& operator=(const SbPlayerPrivate&) = delete;
  ~SbPlayerPrivate() = default;
};

class ExperimentalFeaturesMock {
 public:
  MOCK_METHOD(void,
              SetExperimentalFeatures,
              (const StarboardExtensionExperimentalFeatures*),
              ());
};
ExperimentalFeaturesMock* g_experimental_features_mock = nullptr;

extern "C" void SetExperimentalFeaturesForCurrentThreadMock(
    const StarboardExtensionExperimentalFeatures* experimental_features) {
  if (g_experimental_features_mock) {
    g_experimental_features_mock->SetExperimentalFeatures(
        experimental_features);
  }
}

static StarboardExtensionExperimentalFeaturesConfigurationApi
    g_experimental_features_api = {
        kStarboardExtensionExperimentalFeaturesConfigurationName, 1,
        &SetExperimentalFeaturesForCurrentThreadMock};

bool g_enable_experimental_features_extension = false;

extern "C" const void* SbSystemGetExtension(const char* name) {
  if (strcmp(name, kStarboardExtensionExperimentalFeaturesConfigurationName) ==
      0) {
    return g_enable_experimental_features_extension
               ? &g_experimental_features_api
               : nullptr;
  }
  return nullptr;
}

namespace media {

namespace {

using ::testing::_;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;

class MockSbPlayerInterface : public SbPlayerInterface {
 public:
  MOCK_METHOD(SbPlayer,
              Create,
              (SbWindow,
               const SbPlayerCreationParam*,
               SbPlayerDeallocateSampleFunc,
               SbPlayerDecoderStatusFunc,
               SbPlayerStatusFunc,
               SbPlayerErrorFunc,
               void*,
               SbDecodeTargetGraphicsContextProvider*),
              (override));
  MOCK_METHOD(SbPlayerOutputMode,
              GetPreferredOutputMode,
              (const SbPlayerCreationParam*),
              (override));
  MOCK_METHOD(void, Destroy, (SbPlayer), (override));
  MOCK_METHOD(void, Seek, (SbPlayer, base::TimeDelta, int), (override));
  MOCK_METHOD(void,
              WriteSamples,
              (SbPlayer, SbMediaType, const SbPlayerSampleInfo*, int),
              (override));
  MOCK_METHOD(int,
              GetMaximumNumberOfSamplesPerWrite,
              (SbPlayer, SbMediaType),
              (override));
  MOCK_METHOD(void, WriteEndOfStream, (SbPlayer, SbMediaType), (override));
  MOCK_METHOD(void, SetBounds, (SbPlayer, int, int, int, int, int), (override));
  MOCK_METHOD(bool, SetPlaybackRate, (SbPlayer, double), (override));
  MOCK_METHOD(void, SetVolume, (SbPlayer, double), (override));
  MOCK_METHOD(void, GetInfo, (SbPlayer, SbPlayerInfo*), (override));
  MOCK_METHOD(SbDecodeTarget, GetCurrentFrame, (SbPlayer), (override));
  MOCK_METHOD(bool,
              GetAudioConfiguration,
              (SbPlayer, int, SbMediaAudioConfiguration*),
              (override));

#if SB_HAS(PLAYER_WITH_URL)
  MOCK_METHOD(SbPlayer,
              CreateUrlPlayer,
              (const char*,
               SbWindow,
               SbPlayerStatusFunc,
               SbPlayerEncryptedMediaInitDataEncounteredCB,
               SbPlayerErrorFunc,
               void*),
              (override));
  MOCK_METHOD(void, SetUrlPlayerDrmSystem, (SbPlayer, SbDrmSystem), (override));
  MOCK_METHOD(bool,
              GetUrlPlayerOutputModeSupported,
              (SbPlayerOutputMode),
              (override));
  MOCK_METHOD(void,
              GetUrlPlayerExtraInfo,
              (SbPlayer, SbUrlPlayerExtraInfo*),
              (override));
#endif  // SB_HAS(PLAYER_WITH_URL)
};

class MockHost : public SbPlayerBridge::Host {
 public:
  MOCK_METHOD(void, OnNeedData, (DemuxerStream::Type, int), (override));
  MOCK_METHOD(void, OnPlayerStatus, (SbPlayerState), (override));
  MOCK_METHOD(void,
              OnPlayerError,
              (SbPlayerError, const std::string&),
              (override));
};

class SbPlayerBridgeTest : public testing::Test {
 protected:
  void SetUp() override {
    // Set up default responses for mock interface to avoid noise.
    ON_CALL(mock_interface_, GetPreferredOutputMode(_))
        .WillByDefault(Return(kSbPlayerOutputModePunchOut));
    ON_CALL(mock_interface_, GetMaximumNumberOfSamplesPerWrite(_, _))
        .WillByDefault(Return(1));
    ON_CALL(mock_interface_, SetPlaybackRate(_, _)).WillByDefault(Return(true));

    g_experimental_features_mock = &mock_experimental_features_;
  }

  void TearDown() override {
    g_experimental_features_mock = nullptr;
    g_enable_experimental_features_extension = false;
  }

  std::unique_ptr<SbPlayerBridge> CreateSbPlayerBridge(
      SbPlayer fake_player,
      SbPlayerOutputMode output_mode = kSbPlayerOutputModePunchOut,
      SbPlayerBridge::ExperimentalFeatures experimental_features = {}) {
    EXPECT_CALL(mock_interface_, GetPreferredOutputMode(_))
        .WillRepeatedly(Return(output_mode));

    EXPECT_CALL(mock_interface_, Create(_, _, _, _, _, _, _, _))
        .WillOnce(DoAll(SaveArg<2>(&sample_deallocate_func_),
                        SaveArg<3>(&decoder_status_func_),
                        SaveArg<4>(&player_status_func_),
                        SaveArg<5>(&player_error_func_), SaveArg<6>(&context_),
                        Return(fake_player)));

    AudioDecoderConfig audio_config = TestAudioConfig::Normal();
    VideoDecoderConfig video_config = TestVideoConfig::Normal();

    return std::make_unique<SbPlayerBridge>(
        &mock_interface_, task_environment_.GetMainThreadTaskRunner(),
        base::BindRepeating(
            []() -> SbDecodeTargetGraphicsContextProvider* { return nullptr; }),
        audio_config, "audio/mp4", video_config, "video/mp4",
        /*window=*/nullptr,
        /*drm_system=*/kSbDrmSystemInvalid, &mock_host_,
        /*allow_resume_after_suspend=*/false,
        /*default_output_mode=*/output_mode,
        /*max_video_capabilities=*/"",
        /*max_video_input_size=*/0, experimental_features);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  StrictMock<MockSbPlayerInterface> mock_interface_;
  StrictMock<MockHost> mock_host_;
  StrictMock<ExperimentalFeaturesMock> mock_experimental_features_;

  // Callbacks saved from SbPlayerInterface::Create
  SbPlayerDeallocateSampleFunc sample_deallocate_func_ = nullptr;
  SbPlayerDecoderStatusFunc decoder_status_func_ = nullptr;
  SbPlayerStatusFunc player_status_func_ = nullptr;
  SbPlayerErrorFunc player_error_func_ = nullptr;
  void* context_ = nullptr;
};

TEST_F(SbPlayerBridgeTest, CreateAndDestroy) {
  auto fake_player = std::make_unique<SbPlayerPrivate>();
  auto bridge = CreateSbPlayerBridge(fake_player.get());
  EXPECT_TRUE(bridge->IsValid());

  EXPECT_CALL(mock_interface_, Destroy(fake_player.get()));
}

TEST_F(SbPlayerBridgeTest, PlayerStatusCallback) {
  auto fake_player = std::make_unique<SbPlayerPrivate>();
  auto bridge = CreateSbPlayerBridge(fake_player.get());

  ASSERT_NE(player_status_func_, nullptr);

  // When SbPlayer reports status, SbPlayerBridge::Host should receive it.
  EXPECT_CALL(mock_host_, OnPlayerStatus(kSbPlayerStatePresenting));
  player_status_func_(fake_player.get(), context_, kSbPlayerStatePresenting,
                      SB_PLAYER_INITIAL_TICKET);
  task_environment_.RunUntilIdle();

  EXPECT_CALL(mock_interface_, Destroy(fake_player.get()));
}

TEST_F(SbPlayerBridgeTest, DecoderStatusCallback) {
  auto fake_player = std::make_unique<SbPlayerPrivate>();
  auto bridge = CreateSbPlayerBridge(fake_player.get());

  ASSERT_NE(decoder_status_func_, nullptr);

  // When SbPlayer decoder needs data, SbPlayerBridge::Host should be notified.
  // 1 is max_number_of_buffers_to_write, which is what
  // GetMaximumNumberOfSamplesPerWrite returns.
  EXPECT_CALL(mock_host_, OnNeedData(DemuxerStream::AUDIO, 1));
  decoder_status_func_(fake_player.get(), context_, kSbMediaTypeAudio,
                       kSbPlayerDecoderStateNeedsData,
                       SB_PLAYER_INITIAL_TICKET);
  task_environment_.RunUntilIdle();

  EXPECT_CALL(mock_interface_, Destroy(fake_player.get()));
}

TEST_F(SbPlayerBridgeTest, PlayerErrorCallback) {
  auto fake_player = std::make_unique<SbPlayerPrivate>();
  auto bridge = CreateSbPlayerBridge(fake_player.get());

  ASSERT_NE(player_error_func_, nullptr);

  EXPECT_CALL(mock_host_, OnPlayerError(kSbPlayerErrorDecode, "test error"));
  player_error_func_(fake_player.get(), context_, kSbPlayerErrorDecode,
                     "test error");
  task_environment_.RunUntilIdle();

  EXPECT_CALL(mock_interface_, Destroy(fake_player.get()));
}

TEST_F(SbPlayerBridgeTest, SetVolume) {
  auto fake_player = std::make_unique<SbPlayerPrivate>();
  auto bridge = CreateSbPlayerBridge(fake_player.get());

  EXPECT_CALL(mock_interface_, SetVolume(fake_player.get(), 0.5));
  bridge->SetVolume(0.5f);

  EXPECT_CALL(mock_interface_, Destroy(fake_player.get()));
}

TEST_F(SbPlayerBridgeTest, SetPlaybackRate) {
  auto fake_player = std::make_unique<SbPlayerPrivate>();
  auto bridge = CreateSbPlayerBridge(fake_player.get());

  EXPECT_CALL(mock_interface_, SetPlaybackRate(fake_player.get(), 1.5));
  bridge->SetPlaybackRate(1.5);

  EXPECT_CALL(mock_interface_, Destroy(fake_player.get()));
}

TEST_F(SbPlayerBridgeTest, WriteBuffers) {
  auto fake_player = std::make_unique<SbPlayerPrivate>();
  auto bridge = CreateSbPlayerBridge(fake_player.get());

  const uint8_t kData[] = {0x00, 0x01, 0x02, 0x03};
  scoped_refptr<DecoderBuffer> buffer = DecoderBuffer::CopyFrom(kData);
  buffer->set_timestamp(base::Milliseconds(10));

  std::vector<scoped_refptr<DecoderBuffer>> buffers;
  buffers.push_back(buffer);

  EXPECT_CALL(mock_interface_,
              WriteSamples(fake_player.get(), kSbMediaTypeAudio, _, 1))
      .WillOnce(Invoke([](SbPlayer player, SbMediaType sample_type,
                          const SbPlayerSampleInfo* sample_infos,
                          int number_of_sample_infos) {
        EXPECT_EQ(sample_type, kSbMediaTypeAudio);
        ASSERT_EQ(number_of_sample_infos, 1);
        ASSERT_NE(sample_infos, nullptr);
        EXPECT_EQ(sample_infos[0].buffer_size, 4);
        EXPECT_EQ(sample_infos[0].timestamp, 10'000);
      }));

  bridge->WriteBuffers(DemuxerStream::AUDIO, buffers);
  task_environment_.RunUntilIdle();

  EXPECT_CALL(mock_interface_, Destroy(fake_player.get()));
}

TEST_F(SbPlayerBridgeTest, WriteEndOfStream) {
  auto fake_player = std::make_unique<SbPlayerPrivate>();
  auto bridge = CreateSbPlayerBridge(fake_player.get());

  std::vector<scoped_refptr<DecoderBuffer>> buffers;
  buffers.push_back(DecoderBuffer::CreateEOSBuffer());

  EXPECT_CALL(mock_interface_,
              WriteEndOfStream(fake_player.get(), kSbMediaTypeAudio));

  bridge->WriteBuffers(DemuxerStream::AUDIO, buffers);
  task_environment_.RunUntilIdle();

  EXPECT_CALL(mock_interface_, Destroy(fake_player.get()));
}

TEST_F(SbPlayerBridgeTest, Seek) {
  auto fake_player = std::make_unique<SbPlayerPrivate>();
  auto bridge = CreateSbPlayerBridge(fake_player.get());

  EXPECT_CALL(mock_interface_,
              Seek(fake_player.get(), base::Milliseconds(100), 1));

  bridge->Seek(base::Milliseconds(100));
  task_environment_.RunUntilIdle();

  EXPECT_CALL(mock_interface_, Destroy(fake_player.get()));
}

TEST_F(SbPlayerBridgeTest, SuspendAndResume) {
  auto fake_player = std::make_unique<SbPlayerPrivate>();
  auto bridge = CreateSbPlayerBridge(fake_player.get());

  EXPECT_CALL(mock_interface_, SetPlaybackRate(fake_player.get(), 0.0))
      .WillOnce(Return(true));
  EXPECT_CALL(mock_interface_, GetInfo(fake_player.get(), _))
      .WillOnce(Invoke([](SbPlayer player, SbPlayerInfo* out_info) {
        ASSERT_NE(out_info, nullptr);
        out_info->total_video_frames = 10;
        out_info->dropped_video_frames = 2;
        out_info->current_media_timestamp = 50'000;
      }));

  // Suspend calls Destroy. We must set expectation here because it happens
  // during the test.
  EXPECT_CALL(mock_interface_, Destroy(fake_player.get()));

  bridge->Suspend();
  EXPECT_FALSE(bridge->IsValid());

  auto new_fake_player = std::make_unique<SbPlayerPrivate>();
  EXPECT_CALL(mock_interface_, Create(_, _, _, _, _, _, _, _))
      .WillOnce(DoAll(SaveArg<2>(&sample_deallocate_func_),
                      SaveArg<3>(&decoder_status_func_),
                      SaveArg<4>(&player_status_func_),
                      SaveArg<5>(&player_error_func_), SaveArg<6>(&context_),
                      Return(new_fake_player.get())));

  bridge->Resume(/*window=*/nullptr);
  EXPECT_TRUE(bridge->IsValid());

  EXPECT_CALL(mock_interface_, Destroy(new_fake_player.get()));
}

TEST_F(SbPlayerBridgeTest, ExperimentalFeatures) {
  g_enable_experimental_features_extension = true;

  SbPlayerBridge::ExperimentalFeatures features;
  features.allow_audio_writing_on_pause = true;
  features.enable_flush_during_seek = true;

  EXPECT_CALL(mock_experimental_features_, SetExperimentalFeatures(_))
      .WillOnce(Invoke(
          [](const StarboardExtensionExperimentalFeatures* ext_features) {
            ASSERT_NE(ext_features, nullptr);
            EXPECT_TRUE(ext_features->allow_audio_writing_on_pause);
            EXPECT_TRUE(ext_features->flush_decoder_during_reset);
            EXPECT_FALSE(ext_features->disable_low_performance_sw_decoder);
          }));

  auto fake_player = std::make_unique<SbPlayerPrivate>();
  auto bridge = CreateSbPlayerBridge(fake_player.get(),
                                     kSbPlayerOutputModePunchOut, features);
  task_environment_.RunUntilIdle();

  EXPECT_CALL(mock_interface_, Destroy(fake_player.get()));
}

}  // namespace

}  // namespace media
