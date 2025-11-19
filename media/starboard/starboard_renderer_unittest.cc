// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "media/starboard/starboard_renderer.h"

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_util.h"
#include "media/base/mock_filters.h"
#include "media/base/test_helpers.h"
#include "media/starboard/sbplayer_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;

struct SbPlayerPrivate {
 public:
  SbPlayerPrivate() = default;

  SbPlayerPrivate(const SbPlayerPrivate&) = delete;
  SbPlayerPrivate& operator=(const SbPlayerPrivate&) = delete;

  ~SbPlayerPrivate() = default;
};

namespace media {

namespace {

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
  SbPlayerOutputMode GetPreferredOutputMode(
      const SbPlayerCreationParam* creation_param) override {
    return kSbPlayerOutputModePunchOut;
  }
  void Destroy(SbPlayer player) override {
    if (player) {
      delete player;
    }
  }
  MOCK_METHOD3(Seek, void(SbPlayer, base::TimeDelta, int));
  MOCK_METHOD4(WriteSamples,
               void(SbPlayer, SbMediaType, const SbPlayerSampleInfo*, int));
  int GetMaximumNumberOfSamplesPerWrite(SbPlayer player,
                                        SbMediaType sample_type) override {
    return 1;
  }
  MOCK_METHOD2(WriteEndOfStream, void(SbPlayer, SbMediaType));
  MOCK_METHOD6(SetBounds, void(SbPlayer, int, int, int, int, int));
  bool SetPlaybackRate(SbPlayer player, double playback_rate) override {
    return true;
  }
  void SetVolume(SbPlayer player, double volume) override {}
  MOCK_METHOD2(GetInfo, void(SbPlayer, SbPlayerInfo*));
  SbDecodeTarget GetCurrentFrame(SbPlayer player) override {
    return kSbDecodeTargetInvalid;
  }

#if SB_HAS(PLAYER_WITH_URL)
  MOCK_METHOD6(CreateUrlPlayer,
               SbPlayer(const char*,
                        SbWindow,
                        SbPlayerStatusFunc,
                        SbPlayerEncryptedMediaInitDataEncounteredCB,
                        SbPlayerErrorFunc,
                        void*));
  MOCK_METHOD2(SetUrlPlayerDrmSystem, void(SbPlayer, SbDrmSystem));
  MOCK_METHOD2(GetUrlPlayerExtraInfo, void(SbPlayer, SbUrlPlayerExtraInfo*));

  bool GetUrlPlayerOutputModeSupported(
      SbPlayerOutputMode output_mode) override {
    return true
  }
#endif  // SB_HAS(PLAYER_WITH_URL)

  bool GetAudioConfiguration(
      SbPlayer player,
      int index,
      SbMediaAudioConfiguration* out_audio_configuration) {
    return true;
  }
};

class StarboardRendererTest : public testing::Test {
 protected:
  StarboardRendererTest() {
    renderer_->SetSbPlayerInterfaceForTesting(&mock_sbplayer_interface_);
    renderer_->SetStarboardRendererCallbacks(
        /*paint_video_hole_frame_cb=*/base::DoNothing(),
        /*update_starboard_rendering_mode_cb=*/base::DoNothing()
#if BUILDFLAG(IS_ANDROID)
            ,
        /*request_overlay_info_cb=*/base::DoNothing()
#endif  // BUILDFLAG(IS_ANDROID)
    );

    EXPECT_CALL(media_resource_, GetAllStreams())
        .WillRepeatedly(Invoke(this, &StarboardRendererTest::GetAllStreams));
    EXPECT_CALL(media_resource_, GetType())
        .WillRepeatedly(Return(MediaResource::STREAM));
  }

  ~StarboardRendererTest() override {}

  void AddStream(DemuxerStream::Type type, bool encrypted) {
    streams_.push_back(CreateMockDemuxerStream(type, encrypted));
  }

  std::vector<DemuxerStream*> GetAllStreams() {
    std::vector<DemuxerStream*> streams;
    for (auto& stream : streams_) {
      streams.push_back(stream.get());
    }
    return streams;
  }

  SbPlayer InitializeWithAudioAndVideo(bool encrypted = false) {
    AddStream(DemuxerStream::AUDIO, encrypted);
    AddStream(DemuxerStream::VIDEO, encrypted);

    SbPlayer player = new SbPlayerPrivate();
    EXPECT_CALL(mock_sbplayer_interface_, Create(_, _, _, _, _, _, _, _))
        .WillOnce(DoAll(SaveArg<3>(&decoder_status_cb_),
                        SaveArg<4>(&player_status_cb_),
                        SaveArg<5>(&player_error_cb_), SaveArg<6>(&context_),
                        Return(player)));

    if (encrypted) {
      EXPECT_CALL(set_cdm_cb_, Run(true));
      renderer_->SetCdm(&cdm_context_, set_cdm_cb_.Get());
    }

    EXPECT_CALL(renderer_init_cb_, Run(HasStatusCode(PIPELINE_OK)));
    renderer_->Initialize(&media_resource_, &renderer_client_,
                          renderer_init_cb_.Get());
    return player;
  }

  base::test::TaskEnvironment task_environment_;
  const std::unique_ptr<StarboardRenderer> renderer_ =
      std::make_unique<StarboardRenderer>(
          task_environment_.GetMainThreadTaskRunner(),
          std::make_unique<NullMediaLog>(),
          /*overlay_plane_id=*/base::UnguessableToken::Create(),
          /*audio_write_duration_local=*/base::Seconds(1),
          /*audio_write_duration_remote=*/base::Seconds(1),
          /*max_video_capabilities=*/"");
  base::MockOnceCallback<void(bool)> set_cdm_cb_;
  base::MockOnceCallback<void(PipelineStatus)> renderer_init_cb_;
  NiceMock<MockCdmContext> cdm_context_;
  NiceMock<MockMediaResource> media_resource_;
  NiceMock<MockRendererClient> renderer_client_;
  std::vector<std::unique_ptr<StrictMock<MockDemuxerStream>>> streams_;
  StrictMock<MockSbPlayerInterface> mock_sbplayer_interface_;
  SbPlayerDecoderStatusFunc decoder_status_cb_ = nullptr;
  SbPlayerStatusFunc player_status_cb_ = nullptr;
  SbPlayerErrorFunc player_error_cb_ = nullptr;
  void* context_ = nullptr;
};

TEST_F(StarboardRendererTest, InitializeWithClearContent) {
  InitializeWithAudioAndVideo();
  task_environment_.RunUntilIdle();
}

TEST_F(StarboardRendererTest, InitializeWaitsForCdm) {
  AddStream(DemuxerStream::AUDIO, /*encrypted=*/true);
  AddStream(DemuxerStream::VIDEO, /*encrypted=*/true);

  EXPECT_CALL(renderer_client_, OnWaiting(WaitingReason::kNoCdm));

  renderer_->Initialize(&media_resource_, &renderer_client_,
                        renderer_init_cb_.Get());

  task_environment_.RunUntilIdle();
}

TEST_F(StarboardRendererTest, SetCdmThenInitialize) {
  InitializeWithAudioAndVideo(/*encrypted=*/true);
  task_environment_.RunUntilIdle();
}

TEST_F(StarboardRendererTest, InitializeThenSetCdm) {
  AddStream(DemuxerStream::AUDIO, /*encrypted=*/true);
  AddStream(DemuxerStream::VIDEO, /*encrypted=*/true);

  SbPlayer player = new SbPlayerPrivate();
  EXPECT_CALL(mock_sbplayer_interface_, Create(_, _, _, _, _, _, _, _))
      .WillOnce(Return(player));
  EXPECT_CALL(renderer_client_, OnWaiting(WaitingReason::kNoCdm));
  renderer_->Initialize(&media_resource_, &renderer_client_,
                        renderer_init_cb_.Get());
  task_environment_.RunUntilIdle();

  EXPECT_CALL(set_cdm_cb_, Run(true));
  EXPECT_CALL(renderer_init_cb_, Run(HasStatusCode(PIPELINE_OK)));

  renderer_->SetCdm(&cdm_context_, set_cdm_cb_.Get());

  task_environment_.RunUntilIdle();
}

TEST_F(StarboardRendererTest, InitializeFailsWithNoStreams) {
  EXPECT_CALL(renderer_init_cb_,
              Run(HasStatusCode(DEMUXER_ERROR_NO_SUPPORTED_STREAMS)));

  renderer_->Initialize(&media_resource_, &renderer_client_,
                        renderer_init_cb_.Get());

  task_environment_.RunUntilIdle();
}

TEST_F(StarboardRendererTest, InitializeWithInvalidSbPlayer) {
  AddStream(DemuxerStream::AUDIO, /*encrypted=*/false);
  AddStream(DemuxerStream::VIDEO, /*encrypted=*/false);

  EXPECT_CALL(mock_sbplayer_interface_, Create(_, _, _, _, _, _, _, _))
      .WillOnce(Return(kSbPlayerInvalid));
  EXPECT_CALL(renderer_init_cb_,
              Run(HasStatusCode(DECODER_ERROR_NOT_SUPPORTED)));

  renderer_->Initialize(&media_resource_, &renderer_client_,
                        renderer_init_cb_.Get());

  task_environment_.RunUntilIdle();
}

TEST_F(StarboardRendererTest, OnPlayerStatusCallbacksPresenting) {
  SbPlayer player = InitializeWithAudioAndVideo();
  ASSERT_TRUE(player_status_cb_);
  player_status_cb_(player, context_, kSbPlayerStateInitialized,
                    /*ticket=*/SB_PLAYER_INITIAL_TICKET);
  task_environment_.RunUntilIdle();
  EXPECT_CALL(renderer_client_,
              OnBufferingStateChange(BUFFERING_HAVE_ENOUGH,
                                     BUFFERING_CHANGE_REASON_UNKNOWN));
  player_status_cb_(player, context_, kSbPlayerStatePresenting,
                    /*ticket=*/SB_PLAYER_INITIAL_TICKET);
  task_environment_.RunUntilIdle();
}

TEST_F(StarboardRendererTest, OnPlayerStatusCallbacksEnded) {
  SbPlayer player = InitializeWithAudioAndVideo();
  ASSERT_TRUE(player_status_cb_);
  player_status_cb_(player, context_, kSbPlayerStateInitialized,
                    /*ticket=*/SB_PLAYER_INITIAL_TICKET);
  task_environment_.RunUntilIdle();

  EXPECT_CALL(renderer_client_, OnEnded());
  player_status_cb_(player, context_, kSbPlayerStateEndOfStream,
                    /*ticket=*/SB_PLAYER_INITIAL_TICKET);
  task_environment_.RunUntilIdle();
}

TEST_F(StarboardRendererTest, OnPlayerErrorCallback) {
  SbPlayer player = InitializeWithAudioAndVideo();
  ASSERT_TRUE(player_error_cb_);

  EXPECT_CALL(renderer_client_, OnError(HasStatusCode(PIPELINE_ERROR_DECODE)));
  player_error_cb_(player, context_, kSbPlayerErrorDecode, "decoding failed");
  task_environment_.RunUntilIdle();
}

TEST_F(StarboardRendererTest, RejectCdmSwitching) {
  EXPECT_CALL(set_cdm_cb_, Run(true));
  renderer_->SetCdm(&cdm_context_, set_cdm_cb_.Get());
  task_environment_.RunUntilIdle();

  base::MockOnceCallback<void(bool)> second_set_cdm_cb;
  EXPECT_CALL(second_set_cdm_cb, Run(false));
  renderer_->SetCdm(&cdm_context_, second_set_cdm_cb.Get());
  task_environment_.RunUntilIdle();
}

}  // namespace

}  // namespace media
