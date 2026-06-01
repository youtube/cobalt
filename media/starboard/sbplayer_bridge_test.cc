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
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

struct SbPlayerPrivate {
 public:
  SbPlayerPrivate() = default;
  SbPlayerPrivate(const SbPlayerPrivate&) = delete;
  SbPlayerPrivate& operator=(const SbPlayerPrivate&) = delete;
  ~SbPlayerPrivate() = default;
};

namespace media {

namespace {

using ::base::test::TaskEnvironment;
using ::testing::_;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;

class MockSbPlayerInterface : public SbPlayerInterface {
 public:
  MOCK_METHOD8(Create,
               SbPlayer(SbWindow,
                        const SbPlayerCreationParam*,
                        SbPlayerDeallocateSampleFunc,
                        SbPlayerDecoderStatusFunc,
                        SbPlayerStatusFunc,
                        SbPlayerErrorFunc,
                        void*,
                        SbDecodeTargetGraphicsContextProvider*));
  MOCK_METHOD1(GetPreferredOutputMode,
               SbPlayerOutputMode(const SbPlayerCreationParam*));
  MOCK_METHOD1(Destroy, void(SbPlayer));
  MOCK_METHOD3(Seek, void(SbPlayer, base::TimeDelta, int));
  MOCK_METHOD4(WriteSamples,
               void(SbPlayer, SbMediaType, const SbPlayerSampleInfo*, int));
  MOCK_METHOD2(GetMaximumNumberOfSamplesPerWrite, int(SbPlayer, SbMediaType));
  MOCK_METHOD2(WriteEndOfStream, void(SbPlayer, SbMediaType));
  MOCK_METHOD6(SetBounds, void(SbPlayer, int, int, int, int, int));
  MOCK_METHOD2(SetPlaybackRate, bool(SbPlayer, double));
  MOCK_METHOD2(SetVolume, void(SbPlayer, double));
  MOCK_METHOD2(GetInfo, void(SbPlayer, SbPlayerInfo*));
  MOCK_METHOD1(GetCurrentFrame, SbDecodeTarget(SbPlayer));
  MOCK_METHOD3(GetAudioConfiguration,
               bool(SbPlayer, int, SbMediaAudioConfiguration*));

#if SB_HAS(PLAYER_WITH_URL)
  MOCK_METHOD6(CreateUrlPlayer,
               SbPlayer(const char*,
                        SbWindow,
                        SbPlayerStatusFunc,
                        SbPlayerEncryptedMediaInitDataEncounteredCB,
                        SbPlayerErrorFunc,
                        void*));
  MOCK_METHOD2(SetUrlPlayerDrmSystem, void(SbPlayer, SbDrmSystem));
  MOCK_METHOD1(GetUrlPlayerOutputModeSupported, bool(SbPlayerOutputMode));
  MOCK_METHOD2(GetUrlPlayerExtraInfo, void(SbPlayer, SbUrlPlayerExtraInfo*));
#endif  // SB_HAS(PLAYER_WITH_URL)
};

class MockHost : public SbPlayerBridge::Host {
 public:
  MOCK_METHOD2(OnNeedData, void(DemuxerStream::Type, int));
  MOCK_METHOD1(OnPlayerStatus, void(SbPlayerState));
  MOCK_METHOD2(OnPlayerError, void(SbPlayerError, const std::string&));
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
  }

  ~SbPlayerBridgeTest() override = default;

  std::unique_ptr<SbPlayerBridge> CreateSbPlayerBridge(
      SbPlayer fake_player,
      SbPlayerOutputMode output_mode = kSbPlayerOutputModePunchOut) {
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
        /*max_video_input_size=*/0, SbPlayerBridge::ExperimentalFeatures{});
  }

  TaskEnvironment task_environment_{TaskEnvironment::TimeSource::MOCK_TIME};
  StrictMock<MockSbPlayerInterface> mock_interface_;
  StrictMock<MockHost> mock_host_;

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
  bridge.reset();
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
        EXPECT_EQ(number_of_sample_infos, 1);
        EXPECT_EQ(sample_infos[0].buffer_size, 4);
        EXPECT_EQ(sample_infos[0].timestamp, 10000);
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
        out_info->total_video_frames = 10;
        out_info->dropped_video_frames = 2;
        out_info->current_media_timestamp = 50000;
      }));
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

}  // namespace

}  // namespace media
