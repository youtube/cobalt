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
#include <optional>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_util.h"
#include "media/base/mock_filters.h"
#include "media/base/test_helpers.h"
#include "media/starboard/mock_sbplayer_interface.h"
#include "media/starboard/sbplayer_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_IOS_TVOS)
#include "starboard/tvos/shared/media/url_player.h"
#endif  // BUILDFLAG(IS_IOS_TVOS)

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;

namespace media {

namespace {

constexpr char kSbPlayerCreateHistogram[] =
    "Cobalt.Media.LatencyTiming.SbPlayerCreate";
constexpr char kSbPlayerCreateToInitializedHistogram[] =
    "Cobalt.Media.LatencyTiming.SbPlayerCreateToInitialized";
// Prefix of all the SbPlayerDestroy histograms, which are split by the state of
// the player when it is destroyed.
constexpr char kSbPlayerDestroyHistogram[] =
    "Cobalt.Media.LatencyTiming.SbPlayerDestroy";
constexpr char kSbPlayerDestroyBeforeInitializedHistogram[] =
    "Cobalt.Media.LatencyTiming.SbPlayerDestroy.BeforeInitialized";

// Returns the number of samples recorded in all the SbPlayerDestroy
// histograms.
size_t GetTotalDestroyCount(const base::HistogramTester& histogram_tester) {
  size_t count = 0;
  for (const auto& [name, histogram_count] :
       histogram_tester.GetTotalCountsForPrefix(kSbPlayerDestroyHistogram)) {
    count += histogram_count;
  }
  return count;
}

// Returns the number of samples recorded in histogram |name| and in its
// ".Subsequent" variant. Which of the two a sample lands in depends on whether
// an earlier test in the same process already created an SbPlayer.
size_t GetTotalCountAcrossOccurrences(
    const base::HistogramTester& histogram_tester,
    const std::string& name) {
  size_t count = 0;
  for (const std::string& histogram : {name, name + ".Subsequent"}) {
    for (const base::Bucket& bucket :
         histogram_tester.GetAllSamples(histogram)) {
      count += bucket.count;
    }
  }
  return count;
}

class StarboardRendererTest : public testing::Test {
 protected:
  StarboardRendererTest() {
    renderer_ = std::make_unique<StarboardRenderer>(
        task_environment_.GetMainThreadTaskRunner(),
        std::make_unique<NullMediaLog>(),
        /*overlay_plane_id=*/base::UnguessableToken::Create(),
        /*audio_write_duration_local=*/base::Seconds(1),
        /*audio_write_duration_remote=*/base::Seconds(1),
        /*max_video_capabilities=*/"",
        /*max_video_resolution=*/"",
        StarboardRendererConfig::ExperimentalFeatures{},
        /*viewport_size=*/gfx::Size()
#if BUILDFLAG(IS_ANDROID)
            ,
        /*android_overlay_factory_cb=*/AndroidOverlayMojoFactoryCB()
#endif  // BUILDFLAG(IS_ANDROID)
    );
    renderer_->SetStarboardRendererCallbacks(
        /*paint_video_hole_frame_cb=*/base::DoNothing(),
        /*update_starboard_rendering_mode_cb=*/base::DoNothing(),
        /*get_sb_window_handle_cb=*/base::NullCallback()
#if BUILDFLAG(IS_ANDROID)
            ,
        /*request_overlay_info_cb=*/base::DoNothing()
#endif  // BUILDFLAG(IS_ANDROID)
    );
    StarboardRenderer::GetDecodeTargetGraphicsContextProviderFunc
        get_decode_target_graphics_context_provider_func = base::BindRepeating(
            &StarboardRendererTest::GetSbDecodeTargetGraphicsContextProvider,
            base::Unretained(this));
    renderer_->set_decode_target_graphics_context_provider(
        get_decode_target_graphics_context_provider_func);

    EXPECT_CALL(media_resource_, GetAllStreams())
        .WillRepeatedly(Invoke(this, &StarboardRendererTest::GetAllStreams));
  }

  ~StarboardRendererTest() override = default;

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

    SbPlayer player = reinterpret_cast<SbPlayer>(new MockSbPlayer());
    EXPECT_CALL(mock_sbplayer_interface_, Create(_, _, _, _, _, _, _, _))
        .WillOnce(DoAll(
            SaveArg<2>(&deallocate_sample_cb_), SaveArg<3>(&decoder_status_cb_),
            SaveArg<4>(&player_status_cb_), SaveArg<5>(&player_error_cb_),
            SaveArg<6>(&context_), Return(player)));

    if (encrypted) {
      EXPECT_CALL(set_cdm_cb_, Run(true));
      renderer_->SetCdm(&cdm_context_, set_cdm_cb_.Get());
    }

    EXPECT_CALL(renderer_init_cb_, Run(HasStatusCode(PIPELINE_OK)));
    renderer_->Initialize(&media_resource_, &renderer_client_,
                          renderer_init_cb_.Get());
    return player;
  }

  SbDecodeTargetGraphicsContextProvider*
  GetSbDecodeTargetGraphicsContextProvider() {
    return &decode_target_graphics_context_provider_;
  }

  // Moves `player` to the playing state: reports kSbPlayerStateInitialized,
  // then calls StartPlayingFrom(), and records the seek ticket so later
  // decoder status callbacks are accepted.
  void StartPlayback(SbPlayer player) {
    ASSERT_TRUE(player_status_cb_);
    player_status_cb_(player, context_, kSbPlayerStateInitialized,
                      /*ticket=*/SB_PLAYER_INITIAL_TICKET);
    task_environment_.RunUntilIdle();

    EXPECT_CALL(mock_sbplayer_interface_, Seek(player, base::TimeDelta(), _))
        .WillOnce(SaveArg<2>(&seek_ticket_));
    renderer_->StartPlayingFrom(base::TimeDelta());
    task_environment_.RunUntilIdle();
  }

  // Has the renderer write one sample of `size` bytes of `type` to `player`,
  // then has `player` deallocate it so SbPlayerBridge counts it as decoded.
  // Requires InitializeWithAudioAndVideo(), which adds the audio stream at
  // index 0 and the video stream at index 1, followed by StartPlayback().
  void WriteAndDeallocateSample(SbPlayer player,
                                DemuxerStream::Type type,
                                size_t size) {
    const bool is_audio = type == DemuxerStream::AUDIO;
    const SbMediaType sb_type =
        is_audio ? kSbMediaTypeAudio : kSbMediaTypeVideo;

    DemuxerStream::ReadCB read_cb;
    EXPECT_CALL(*streams_[is_audio ? 0 : 1], OnRead(_))
        .WillOnce(
            Invoke([&](DemuxerStream::ReadCB& cb) { read_cb = std::move(cb); }))
        .RetiresOnSaturation();

    const void* sample_buffer = nullptr;
    EXPECT_CALL(mock_sbplayer_interface_, WriteSamples(player, sb_type, _, _))
        .WillOnce(Invoke([&](SbPlayer /*player*/, SbMediaType /*type*/,
                             const SbPlayerSampleInfo* sample_infos,
                             int number_of_sample_infos) {
          ASSERT_EQ(number_of_sample_infos, 1);
          EXPECT_EQ(sample_infos[0].buffer_size, static_cast<int>(size));
          sample_buffer = sample_infos[0].buffer;
        }))
        .RetiresOnSaturation();

    ASSERT_TRUE(decoder_status_cb_);
    decoder_status_cb_(player, context_, sb_type,
                       kSbPlayerDecoderStateNeedsData, seek_ticket_);
    task_environment_.RunUntilIdle();

    ASSERT_FALSE(read_cb.is_null());
    scoped_refptr<DecoderBuffer> buffer =
        DecoderBuffer::CopyFrom(std::vector<uint8_t>(size, 0xab));
    buffer->set_timestamp(base::Milliseconds(next_sample_timestamp_ms_++));
    std::move(read_cb).Run(DemuxerStream::kOk, {buffer});
    task_environment_.RunUntilIdle();

    ASSERT_NE(sample_buffer, nullptr);
    ASSERT_TRUE(deallocate_sample_cb_);
    deallocate_sample_cb_(player, context_, sample_buffer);
    task_environment_.RunUntilIdle();
  }

  // Makes SbPlayerGetInfo() report `video_frames_decoded` and
  // `video_frames_dropped` for `player`.
  void SetPlayerInfo(SbPlayer player,
                     int video_frames_decoded,
                     int video_frames_dropped) {
    EXPECT_CALL(mock_sbplayer_interface_, GetInfo(player, _))
        .WillRepeatedly(Invoke([=](SbPlayer /*player*/, SbPlayerInfo* info) {
          *info = SbPlayerInfo{};
          info->total_video_frames = video_frames_decoded;
          info->dropped_video_frames = video_frames_dropped;
          info->playback_rate = 1.0;
        }));
  }

  // Calls GetMediaTime(), which polls SbPlayerBridge and posts statistics
  // updates, and returns all statistics updates received since the last call.
  // Note that this verifies and clears all expectations on
  // `renderer_client_`.
  std::vector<PipelineStatistics> PollStatistics() {
    std::vector<PipelineStatistics> stats;
    EXPECT_CALL(renderer_client_, OnStatisticsUpdate(_))
        .WillRepeatedly(
            Invoke([&](const PipelineStatistics& s) { stats.push_back(s); }));
    renderer_->GetMediaTime();
    task_environment_.RunUntilIdle();
    testing::Mock::VerifyAndClearExpectations(&renderer_client_);
    return stats;
  }

#if BUILDFLAG(IS_IOS_TVOS)
  SbPlayer InitializeUrlPlayer() {
    renderer_->SetSourceUrl("https://example.com/master.m3u8");

    SbPlayer player = reinterpret_cast<SbPlayer>(new MockSbPlayer());
    EXPECT_CALL(mock_sbplayer_interface_, CreateUrlPlayer(_, _, _, _, _, _))
        .WillOnce(DoAll(SaveArg<2>(&player_status_cb_),
                        SaveArg<4>(&player_error_cb_), SaveArg<5>(&context_),
                        Return(player)));
    EXPECT_CALL(renderer_init_cb_, Run(_)).WillOnce(SaveArg<0>(&init_status_));

    renderer_->Initialize(&media_resource_, &renderer_client_,
                          renderer_init_cb_.Get());
    task_environment_.RunUntilIdle();
    return player;
  }

  void ReportPlayerError(SbPlayer player,
                         SbPlayerError error,
                         const char* message) {
    ASSERT_TRUE(player_error_cb_);
    player_error_cb_(player, context_, error, message);
    task_environment_.RunUntilIdle();
  }
#endif  // BUILDFLAG(IS_IOS_TVOS)

  base::test::TaskEnvironment task_environment_;
  base::MockOnceCallback<void(bool)> set_cdm_cb_;
  base::MockOnceCallback<void(PipelineStatus)> renderer_init_cb_;
  NiceMock<MockCdmContext> cdm_context_;
  NiceMock<MockMediaResource> media_resource_;
  NiceMock<MockRendererClient> renderer_client_;
  std::vector<std::unique_ptr<StrictMock<MockDemuxerStream>>> streams_;
  StrictMock<MockSbPlayerInterface> mock_sbplayer_interface_;
  ScopedSbPlayerInterfaceForTesting scoped_sbplayer_interface_{
      &mock_sbplayer_interface_};
  SbPlayerDeallocateSampleFunc deallocate_sample_cb_ = nullptr;
  SbPlayerDecoderStatusFunc decoder_status_cb_ = nullptr;
  SbPlayerStatusFunc player_status_cb_ = nullptr;
  SbPlayerErrorFunc player_error_cb_ = nullptr;
  void* context_ = nullptr;
  SbDecodeTargetGraphicsContextProvider
      decode_target_graphics_context_provider_;
  std::unique_ptr<StarboardRenderer> renderer_;
  int next_sample_timestamp_ms_ = 0;
  int seek_ticket_ = SB_PLAYER_INITIAL_TICKET;
#if BUILDFLAG(IS_IOS_TVOS)
  std::optional<PipelineStatus> init_status_;
#endif  // BUILDFLAG(IS_IOS_TVOS)
};

TEST_F(StarboardRendererTest, InitializeWithClearContent) {
  SbPlayer player = InitializeWithAudioAndVideo();
  ASSERT_TRUE(player_status_cb_);
  player_status_cb_(player, context_, kSbPlayerStateInitialized,
                    /*ticket=*/SB_PLAYER_INITIAL_TICKET);
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
  SbPlayer player = InitializeWithAudioAndVideo(/*encrypted=*/true);
  ASSERT_TRUE(player_status_cb_);
  player_status_cb_(player, context_, kSbPlayerStateInitialized,
                    /*ticket=*/SB_PLAYER_INITIAL_TICKET);
  task_environment_.RunUntilIdle();
}

TEST_F(StarboardRendererTest, InitializeThenSetCdm) {
  AddStream(DemuxerStream::AUDIO, /*encrypted=*/true);
  AddStream(DemuxerStream::VIDEO, /*encrypted=*/true);

  SbPlayer player = reinterpret_cast<SbPlayer>(new MockSbPlayer());
  EXPECT_CALL(mock_sbplayer_interface_, Create(_, _, _, _, _, _, _, _))
      .WillOnce(DoAll(SaveArg<3>(&decoder_status_cb_),
                      SaveArg<4>(&player_status_cb_),
                      SaveArg<5>(&player_error_cb_), SaveArg<6>(&context_),
                      Return(player)));
  EXPECT_CALL(renderer_client_, OnWaiting(WaitingReason::kNoCdm));
  renderer_->Initialize(&media_resource_, &renderer_client_,
                        renderer_init_cb_.Get());
  task_environment_.RunUntilIdle();

  EXPECT_CALL(set_cdm_cb_, Run(true));
  renderer_->SetCdm(&cdm_context_, set_cdm_cb_.Get());
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(player_status_cb_);
  EXPECT_CALL(renderer_init_cb_, Run(HasStatusCode(PIPELINE_OK)));
  player_status_cb_(player, context_, kSbPlayerStateInitialized,
                    /*ticket=*/SB_PLAYER_INITIAL_TICKET);
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
  // No rendering mode is sent when CreatePlayerBridge() fails.
  StrictMock<base::MockRepeatingCallback<void(const StarboardRenderingMode)>>
      update_starboard_rendering_mode_cb;
  renderer_->SetStarboardRendererCallbacks(
      /*paint_video_hole_frame_cb=*/base::DoNothing(),
      update_starboard_rendering_mode_cb.Get(),
      /*get_sb_window_handle_cb=*/base::NullCallback()
#if BUILDFLAG(IS_ANDROID)
          ,
      /*request_overlay_info_cb=*/base::DoNothing()
#endif  // BUILDFLAG(IS_ANDROID)
  );

  AddStream(DemuxerStream::AUDIO, /*encrypted=*/false);
  AddStream(DemuxerStream::VIDEO, /*encrypted=*/false);

  EXPECT_CALL(mock_sbplayer_interface_, Create(_, _, _, _, _, _, _, _))
      .WillOnce(Return(kSbPlayerInvalid));
  EXPECT_CALL(update_starboard_rendering_mode_cb, Run(_)).Times(0);
  EXPECT_CALL(renderer_init_cb_,
              Run(HasStatusCode(DECODER_ERROR_NOT_SUPPORTED)));

  renderer_->Initialize(&media_resource_, &renderer_client_,
                        renderer_init_cb_.Get());

  task_environment_.RunUntilIdle();

  renderer_.reset();
}

TEST_F(StarboardRendererTest, RecordsSbPlayerCreateLatencies) {
  base::HistogramTester histogram_tester;
  SbPlayer player = InitializeWithAudioAndVideo();
  EXPECT_EQ(GetTotalCountAcrossOccurrences(histogram_tester,
                                           kSbPlayerCreateHistogram),
            1u);
  EXPECT_EQ(GetTotalCountAcrossOccurrences(
                histogram_tester, kSbPlayerCreateToInitializedHistogram),
            0u);

  ASSERT_TRUE(player_status_cb_);
  player_status_cb_(player, context_, kSbPlayerStateInitialized,
                    /*ticket=*/SB_PLAYER_INITIAL_TICKET);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(GetTotalCountAcrossOccurrences(histogram_tester,
                                           kSbPlayerCreateHistogram),
            1u);
  EXPECT_EQ(GetTotalCountAcrossOccurrences(
                histogram_tester, kSbPlayerCreateToInitializedHistogram),
            1u);
}

TEST_F(StarboardRendererTest, RecordsSbPlayerCreateLatencyOnFailure) {
  base::HistogramTester histogram_tester;
  AddStream(DemuxerStream::AUDIO, /*encrypted=*/false);
  AddStream(DemuxerStream::VIDEO, /*encrypted=*/false);

  EXPECT_CALL(mock_sbplayer_interface_, Create(_, _, _, _, _, _, _, _))
      .WillOnce(Return(kSbPlayerInvalid));
  EXPECT_CALL(renderer_init_cb_,
              Run(HasStatusCode(DECODER_ERROR_NOT_SUPPORTED)));
  renderer_->Initialize(&media_resource_, &renderer_client_,
                        renderer_init_cb_.Get());
  task_environment_.RunUntilIdle();

  // The failed SbPlayerCreate() call is still recorded, but a player that was
  // never created can't report kSbPlayerStateInitialized.
  EXPECT_EQ(GetTotalCountAcrossOccurrences(histogram_tester,
                                           kSbPlayerCreateHistogram),
            1u);
  EXPECT_EQ(GetTotalCountAcrossOccurrences(
                histogram_tester, kSbPlayerCreateToInitializedHistogram),
            0u);

  // A player that was never created isn't destroyed either.
  renderer_.reset();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(GetTotalDestroyCount(histogram_tester), 0u);
}

TEST_F(StarboardRendererTest, RecordsSbPlayerDestroyLatency) {
  base::HistogramTester histogram_tester;
  InitializeWithAudioAndVideo();
  EXPECT_EQ(GetTotalDestroyCount(histogram_tester), 0u);

  renderer_.reset();
  task_environment_.RunUntilIdle();
  // The player never reported kSbPlayerStateInitialized.
  EXPECT_EQ(GetTotalDestroyCount(histogram_tester), 1u);
  histogram_tester.ExpectTotalCount(kSbPlayerDestroyBeforeInitializedHistogram,
                                    1);
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

TEST_F(StarboardRendererTest, GetMediaTimeReportsSeekTimeUntilPresenting) {
  SbPlayer player = InitializeWithAudioAndVideo();
  ASSERT_TRUE(player_status_cb_);
  player_status_cb_(player, context_, kSbPlayerStateInitialized,
                    /*ticket=*/SB_PLAYER_INITIAL_TICKET);
  task_environment_.RunUntilIdle();

  // Simulate a platform that processes SbPlayerSeek() asynchronously and keeps
  // reporting the pre-seek position from SbPlayerGetInfo() until the seek
  // completes.
  constexpr base::TimeDelta kPreSeekTime = base::Seconds(40);
  constexpr base::TimeDelta kSeekTime = base::Seconds(20);
  constexpr base::TimeDelta kPlayedTime = kSeekTime + base::Milliseconds(500);
  base::TimeDelta platform_media_time = kPreSeekTime;
  EXPECT_CALL(mock_sbplayer_interface_, GetInfo(player, _))
      .WillRepeatedly(Invoke([&](SbPlayer /*player*/, SbPlayerInfo* info) {
        *info = {};
        info->current_media_timestamp = platform_media_time.InMicroseconds();
        info->duration = SB_PLAYER_NO_DURATION;
      }));
  int seek_ticket = SB_PLAYER_INITIAL_TICKET;
  EXPECT_CALL(mock_sbplayer_interface_, Seek(player, kSeekTime, _))
      .WillOnce(SaveArg<2>(&seek_ticket));

  renderer_->StartPlayingFrom(kSeekTime);
  // The stale pre-seek position must not leak out, as the pipeline clamps
  // media time so that it never goes backwards.
  EXPECT_EQ(renderer_->GetMediaTime(), kSeekTime);

  player_status_cb_(player, context_, kSbPlayerStatePrerolling, seek_ticket);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(renderer_->GetMediaTime(), kSeekTime);

  // Once the SbPlayer presents, the platform media time is reported again.
  platform_media_time = kPlayedTime;
  EXPECT_CALL(renderer_client_,
              OnBufferingStateChange(BUFFERING_HAVE_ENOUGH,
                                     BUFFERING_CHANGE_REASON_UNKNOWN));
  player_status_cb_(player, context_, kSbPlayerStatePresenting, seek_ticket);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(renderer_->GetMediaTime(), kPlayedTime);
}

TEST_F(StarboardRendererTest, GetMediaTimeReportsSeekTimeAfterBackwardSeek) {
  SbPlayer player = InitializeWithAudioAndVideo();
  ASSERT_TRUE(player_status_cb_);
  player_status_cb_(player, context_, kSbPlayerStateInitialized,
                    /*ticket=*/SB_PLAYER_INITIAL_TICKET);
  task_environment_.RunUntilIdle();

  base::TimeDelta platform_media_time = base::Seconds(40);
  EXPECT_CALL(mock_sbplayer_interface_, GetInfo(player, _))
      .WillRepeatedly(Invoke([&](SbPlayer /*player*/, SbPlayerInfo* info) {
        *info = {};
        info->current_media_timestamp = platform_media_time.InMicroseconds();
        info->duration = SB_PLAYER_NO_DURATION;
      }));
  int ticket = SB_PLAYER_INITIAL_TICKET;
  EXPECT_CALL(mock_sbplayer_interface_, Seek(player, _, _))
      .WillRepeatedly(SaveArg<2>(&ticket));
  EXPECT_CALL(renderer_client_, OnBufferingStateChange(_, _))
      .Times(AnyNumber());

  // Play from 40s until presenting, then advance to 41s.
  renderer_->StartPlayingFrom(base::Seconds(40));
  const int first_seek_ticket = ticket;
  player_status_cb_(player, context_, kSbPlayerStatePresenting, ticket);
  task_environment_.RunUntilIdle();
  platform_media_time = base::Seconds(41);
  EXPECT_EQ(renderer_->GetMediaTime(), base::Seconds(41));

  // Seek back to 20s. The platform keeps reporting 41s until it presents.
  base::MockOnceClosure flush_cb;
  EXPECT_CALL(flush_cb, Run());
  renderer_->Flush(flush_cb.Get());
  task_environment_.RunUntilIdle();
  renderer_->StartPlayingFrom(base::Seconds(20));
  EXPECT_EQ(renderer_->GetMediaTime(), base::Seconds(20));

  // A late Presenting for the previous seek has a stale ticket and must not
  // end the override.
  player_status_cb_(player, context_, kSbPlayerStatePresenting,
                    first_seek_ticket);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(renderer_->GetMediaTime(), base::Seconds(20));

  platform_media_time = base::Milliseconds(20500);
  player_status_cb_(player, context_, kSbPlayerStatePresenting, ticket);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(renderer_->GetMediaTime(), base::Milliseconds(20500));
}

TEST_F(StarboardRendererTest, StatisticsReportBytesDecodedAsDeltas) {
  SbPlayer player = InitializeWithAudioAndVideo();
  ASSERT_NO_FATAL_FAILURE(StartPlayback(player));

  ASSERT_NO_FATAL_FAILURE(
      WriteAndDeallocateSample(player, DemuxerStream::AUDIO, 4));
  ASSERT_NO_FATAL_FAILURE(
      WriteAndDeallocateSample(player, DemuxerStream::VIDEO, 6));
  SetPlayerInfo(player, /*video_frames_decoded=*/1, /*video_frames_dropped=*/0);
  std::vector<PipelineStatistics> stats = PollStatistics();
  ASSERT_EQ(stats.size(), 1u);
  EXPECT_EQ(stats[0].audio_bytes_decoded, 4u);
  EXPECT_EQ(stats[0].video_bytes_decoded, 6u);
  EXPECT_EQ(stats[0].video_frames_decoded, 1u);
  EXPECT_EQ(stats[0].video_frames_dropped, 0u);

  // The second update must only contain what was decoded since the first one,
  // as RendererClient accumulates the values it receives.
  ASSERT_NO_FATAL_FAILURE(
      WriteAndDeallocateSample(player, DemuxerStream::AUDIO, 8));
  ASSERT_NO_FATAL_FAILURE(
      WriteAndDeallocateSample(player, DemuxerStream::VIDEO, 10));
  SetPlayerInfo(player, /*video_frames_decoded=*/2, /*video_frames_dropped=*/1);
  stats = PollStatistics();
  ASSERT_EQ(stats.size(), 1u);
  EXPECT_EQ(stats[0].audio_bytes_decoded, 8u);
  EXPECT_EQ(stats[0].video_bytes_decoded, 10u);
  EXPECT_EQ(stats[0].video_frames_decoded, 1u);
  EXPECT_EQ(stats[0].video_frames_dropped, 1u);
}

TEST_F(StarboardRendererTest, StatisticsReportedForAudioBytesOnlyChange) {
  SbPlayer player = InitializeWithAudioAndVideo();
  ASSERT_NO_FATAL_FAILURE(StartPlayback(player));

  // No video frames are reported (e.g. audio-only playback), but bytes were
  // decoded, so an update should still be sent.
  ASSERT_NO_FATAL_FAILURE(
      WriteAndDeallocateSample(player, DemuxerStream::AUDIO, 16));
  SetPlayerInfo(player, /*video_frames_decoded=*/0, /*video_frames_dropped=*/0);
  std::vector<PipelineStatistics> stats = PollStatistics();
  ASSERT_EQ(stats.size(), 1u);
  EXPECT_EQ(stats[0].audio_bytes_decoded, 16u);
  EXPECT_EQ(stats[0].video_bytes_decoded, 0u);
  EXPECT_EQ(stats[0].video_frames_decoded, 0u);
  EXPECT_EQ(stats[0].video_frames_dropped, 0u);
}

TEST_F(StarboardRendererTest, StatisticsNotReportedWhenNothingChanges) {
  SbPlayer player = InitializeWithAudioAndVideo();
  ASSERT_NO_FATAL_FAILURE(StartPlayback(player));

  ASSERT_NO_FATAL_FAILURE(
      WriteAndDeallocateSample(player, DemuxerStream::AUDIO, 4));
  ASSERT_NO_FATAL_FAILURE(
      WriteAndDeallocateSample(player, DemuxerStream::VIDEO, 6));
  SetPlayerInfo(player, /*video_frames_decoded=*/1, /*video_frames_dropped=*/0);
  EXPECT_EQ(PollStatistics().size(), 1u);

  // Nothing new was decoded or dropped, so no update should be sent.
  EXPECT_TRUE(PollStatistics().empty());
}

TEST_F(StarboardRendererTest, StatisticsContinueAfterSbPlayerCounterReset) {
  SbPlayer player = InitializeWithAudioAndVideo();
  ASSERT_NO_FATAL_FAILURE(StartPlayback(player));

  SetPlayerInfo(player, /*video_frames_decoded=*/10,
                /*video_frames_dropped=*/3);
  std::vector<PipelineStatistics> stats = PollStatistics();
  ASSERT_EQ(stats.size(), 1u);
  EXPECT_EQ(stats[0].video_frames_decoded, 10u);
  EXPECT_EQ(stats[0].video_frames_dropped, 3u);

  // Some platforms reset the SbPlayer counters, e.g. on seek. Updates should
  // continue from the reset instead of stalling until the old totals are
  // exceeded.
  SetPlayerInfo(player, /*video_frames_decoded=*/4, /*video_frames_dropped=*/1);
  stats = PollStatistics();
  ASSERT_EQ(stats.size(), 1u);
  EXPECT_EQ(stats[0].video_frames_decoded, 4u);
  EXPECT_EQ(stats[0].video_frames_dropped, 1u);

  SetPlayerInfo(player, /*video_frames_decoded=*/6, /*video_frames_dropped=*/1);
  stats = PollStatistics();
  ASSERT_EQ(stats.size(), 1u);
  EXPECT_EQ(stats[0].video_frames_decoded, 2u);
  EXPECT_EQ(stats[0].video_frames_dropped, 0u);
}

TEST_F(StarboardRendererTest, OnPlayerErrorCallback) {
  SbPlayer player = InitializeWithAudioAndVideo();
  ASSERT_TRUE(player_status_cb_);
  player_status_cb_(player, context_, kSbPlayerStateInitialized,
                    /*ticket=*/SB_PLAYER_INITIAL_TICKET);
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(player_error_cb_);

  EXPECT_CALL(renderer_client_, OnError(HasStatusCode(PIPELINE_ERROR_DECODE)));
  player_error_cb_(player, context_, kSbPlayerErrorDecode, "decoding failed");
  task_environment_.RunUntilIdle();
}

TEST_F(StarboardRendererTest, OnErrorDuringInitialization) {
  AddStream(DemuxerStream::AUDIO, /*encrypted=*/false);
  AddStream(DemuxerStream::VIDEO, /*encrypted=*/false);

  SbPlayer player = reinterpret_cast<SbPlayer>(new MockSbPlayer());
  EXPECT_CALL(mock_sbplayer_interface_, Create(_, _, _, _, _, _, _, _))
      .WillOnce(DoAll(SaveArg<3>(&decoder_status_cb_),
                      SaveArg<4>(&player_status_cb_),
                      SaveArg<5>(&player_error_cb_), SaveArg<6>(&context_),
                      Return(player)));

  // Expect renderer_init_cb_ to be called with an error.
  EXPECT_CALL(renderer_init_cb_, Run(HasStatusCode(PIPELINE_ERROR_DECODE)));
  // renderer_client_.OnError should NOT be called because init_cb_ is pending.
  EXPECT_CALL(renderer_client_, OnError(_)).Times(0);

  renderer_->Initialize(&media_resource_, &renderer_client_,
                        renderer_init_cb_.Get());
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(player_error_cb_);
  // Trigger an error before initialization is complete.
  player_error_cb_(player, context_, kSbPlayerErrorDecode, "decoding failed");
  task_environment_.RunUntilIdle();
}

TEST_F(StarboardRendererTest, OnDemuxerErrorDuringInitialization) {
  AddStream(DemuxerStream::AUDIO, /*encrypted=*/false);
  AddStream(DemuxerStream::VIDEO, /*encrypted=*/false);

  SbPlayer player = reinterpret_cast<SbPlayer>(new MockSbPlayer());
  EXPECT_CALL(mock_sbplayer_interface_, Create(_, _, _, _, _, _, _, _))
      .WillOnce(DoAll(SaveArg<3>(&decoder_status_cb_),
                      SaveArg<4>(&player_status_cb_),
                      SaveArg<5>(&player_error_cb_), SaveArg<6>(&context_),
                      Return(player)));

  // Expect renderer_init_cb_ to be called with an error.
  EXPECT_CALL(renderer_init_cb_, Run(HasStatusCode(PIPELINE_ERROR_READ)));
  // renderer_client_.OnError should NOT be called because init_cb_ is pending.
  EXPECT_CALL(renderer_client_, OnError(_)).Times(0);

  renderer_->Initialize(&media_resource_, &renderer_client_,
                        renderer_init_cb_.Get());
  task_environment_.RunUntilIdle();

  // Now that the player is created (but not initialized), simulate the player
  // asking for data.
  DemuxerStream::ReadCB read_cb;
  EXPECT_CALL(*streams_[0], OnRead(_))
      .WillOnce(Invoke(
          [&read_cb](DemuxerStream::ReadCB& cb) { read_cb = std::move(cb); }));
  EXPECT_CALL(*streams_[1], OnRead(_)).Times(0);

  // Trigger OnNeedData to start a read.
  decoder_status_cb_(player, context_, kSbMediaTypeAudio,
                     kSbPlayerDecoderStateNeedsData, SB_PLAYER_INITIAL_TICKET);
  task_environment_.RunUntilIdle();

  ASSERT_FALSE(read_cb.is_null());
  // Simulate a demuxer error.
  std::move(read_cb).Run(DemuxerStream::kError, {});
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

TEST_F(StarboardRendererTest,
       CustomMimeTypesPassedToSbPlayerCreateAndWriteSamples) {
  const std::string kCustomAudioMime =
      "audio/mp4; codecs=\"mp4a.40.2\"; channels=8; bitrate=768000; "
      "audiopassthrough=true; enableresetaudiodecoder=true;";
  const std::string kCustomVideoMime =
      "video/mp4; codecs=\"avc1.64002a\"; width=3840; height=2160; "
      "tunnelmode=true; hdr=hdr10plus; framerate=60;";

  auto audio_stream =
      std::make_unique<StrictMock<MockDemuxerStream>>(DemuxerStream::AUDIO);
  AudioDecoderConfig audio_config = TestAudioConfig::Normal();
  audio_config.set_mime_type(kCustomAudioMime);
  audio_stream->set_audio_decoder_config(audio_config);
  streams_.push_back(std::move(audio_stream));

  auto video_stream =
      std::make_unique<StrictMock<MockDemuxerStream>>(DemuxerStream::VIDEO);
  VideoDecoderConfig video_config = TestVideoConfig::Normal();
  video_config.set_mime_type(kCustomVideoMime);
  video_stream->set_video_decoder_config(video_config);
  streams_.push_back(std::move(video_stream));

  SbPlayer player = reinterpret_cast<SbPlayer>(new MockSbPlayer());
  std::string created_audio_mime;
  std::string created_video_mime;

  EXPECT_CALL(mock_sbplayer_interface_, Create(_, _, _, _, _, _, _, _))
      .WillOnce(Invoke(
          [&](SbWindow /*window*/, const SbPlayerCreationParam* creation_param,
              SbPlayerDeallocateSampleFunc /*sample_deallocate_func*/,
              SbPlayerDecoderStatusFunc decoder_status_func,
              SbPlayerStatusFunc player_status_func,
              SbPlayerErrorFunc player_error_func, void* context,
              SbDecodeTargetGraphicsContextProvider* /*context_provider*/) {
            decoder_status_cb_ = decoder_status_func;
            player_status_cb_ = player_status_func;
            player_error_cb_ = player_error_func;
            context_ = context;
            if (creation_param) {
              if (creation_param->audio_stream_info.mime) {
                created_audio_mime = creation_param->audio_stream_info.mime;
              }
              if (creation_param->video_stream_info.mime) {
                created_video_mime = creation_param->video_stream_info.mime;
              }
            }
            return player;
          }));

  EXPECT_CALL(renderer_init_cb_, Run(HasStatusCode(PIPELINE_OK)));
  renderer_->Initialize(&media_resource_, &renderer_client_,
                        renderer_init_cb_.Get());
  task_environment_.RunUntilIdle();

  // Verify that SbPlayerCreate received the custom MIME strings.
  EXPECT_EQ(created_audio_mime, kCustomAudioMime);
  EXPECT_EQ(created_video_mime, kCustomVideoMime);

  // Verify WriteSamples for Audio with custom MIME parameters.
  DemuxerStream::ReadCB audio_read_cb;
  EXPECT_CALL(*streams_[0], OnRead(_))
      .WillOnce(Invoke(
          [&](DemuxerStream::ReadCB& cb) { audio_read_cb = std::move(cb); }));

  std::string written_audio_mime;
  EXPECT_CALL(mock_sbplayer_interface_,
              WriteSamples(player, kSbMediaTypeAudio, _, _))
      .WillOnce(Invoke([&](SbPlayer /*player*/, SbMediaType /*type*/,
                           const SbPlayerSampleInfo* sample_infos,
                           int number_of_sample_infos) {
        EXPECT_NE(sample_infos, nullptr);
        EXPECT_GT(number_of_sample_infos, 0);
        if (sample_infos && number_of_sample_infos > 0 &&
            sample_infos[0].audio_sample_info.stream_info.mime) {
          written_audio_mime =
              sample_infos[0].audio_sample_info.stream_info.mime;
        }
      }));

  decoder_status_cb_(player, context_, kSbMediaTypeAudio,
                     kSbPlayerDecoderStateNeedsData, SB_PLAYER_INITIAL_TICKET);
  task_environment_.RunUntilIdle();

  ASSERT_FALSE(audio_read_cb.is_null());
  const uint8_t kAudioData[] = {0x01, 0x02, 0x03, 0x04};
  scoped_refptr<DecoderBuffer> audio_buffer =
      DecoderBuffer::CopyFrom(kAudioData);
  std::move(audio_read_cb).Run(DemuxerStream::kOk, {audio_buffer});
  task_environment_.RunUntilIdle();

  EXPECT_EQ(written_audio_mime, kCustomAudioMime);

  // Verify WriteSamples for Video with custom MIME parameters.
  DemuxerStream::ReadCB video_read_cb;
  EXPECT_CALL(*streams_[1], OnRead(_))
      .WillOnce(Invoke(
          [&](DemuxerStream::ReadCB& cb) { video_read_cb = std::move(cb); }));

  std::string written_video_mime;
  EXPECT_CALL(mock_sbplayer_interface_,
              WriteSamples(player, kSbMediaTypeVideo, _, _))
      .WillOnce(Invoke([&](SbPlayer /*player*/, SbMediaType /*type*/,
                           const SbPlayerSampleInfo* sample_infos,
                           int number_of_sample_infos) {
        EXPECT_NE(sample_infos, nullptr);
        EXPECT_GT(number_of_sample_infos, 0);
        if (sample_infos && number_of_sample_infos > 0 &&
            sample_infos[0].video_sample_info.stream_info.mime) {
          written_video_mime =
              sample_infos[0].video_sample_info.stream_info.mime;
        }
      }));

  decoder_status_cb_(player, context_, kSbMediaTypeVideo,
                     kSbPlayerDecoderStateNeedsData, SB_PLAYER_INITIAL_TICKET);
  task_environment_.RunUntilIdle();

  ASSERT_FALSE(video_read_cb.is_null());
  const uint8_t kVideoData[] = {0x00, 0x00, 0x00, 0x01};
  scoped_refptr<DecoderBuffer> video_buffer =
      DecoderBuffer::CopyFrom(kVideoData);
  std::move(video_read_cb).Run(DemuxerStream::kOk, {video_buffer});
  task_environment_.RunUntilIdle();

  EXPECT_EQ(written_video_mime, kCustomVideoMime);

  ASSERT_TRUE(player_status_cb_);
  player_status_cb_(player, context_, kSbPlayerStateInitialized,
                    SB_PLAYER_INITIAL_TICKET);
  task_environment_.RunUntilIdle();
}

TEST_F(StarboardRendererTest,
       ZIndexUpdatesWhenPrimaryPlayerAddedBehindSecondaryPlayer) {
  auto secondary_renderer = std::make_unique<StarboardRenderer>(
      task_environment_.GetMainThreadTaskRunner(),
      std::make_unique<NullMediaLog>(),
      /*overlay_plane_id=*/base::UnguessableToken::Create(),
      /*audio_write_duration_local=*/base::Seconds(1),
      /*audio_write_duration_remote=*/base::Seconds(1),
      /*max_video_capabilities=*/"width=432; height=240; framerate=15;",
      /*max_video_resolution=*/"",
      StarboardRendererConfig::ExperimentalFeatures{},
      /*viewport_size=*/gfx::Size()
#if BUILDFLAG(IS_ANDROID)
          ,
      /*android_overlay_factory_cb=*/AndroidOverlayMojoFactoryCB()
#endif  // BUILDFLAG(IS_ANDROID)
  );
  secondary_renderer->SetStarboardRendererCallbacks(
      /*paint_video_hole_frame_cb=*/base::DoNothing(),
      /*update_starboard_rendering_mode_cb=*/base::DoNothing(),
      /*get_sb_window_handle_cb=*/base::NullCallback()
#if BUILDFLAG(IS_ANDROID)
          ,
      /*request_overlay_info_cb=*/base::DoNothing()
#endif  // BUILDFLAG(IS_ANDROID)
  );
  secondary_renderer->set_decode_target_graphics_context_provider(
      base::BindRepeating(
          [](SbDecodeTargetGraphicsContextProvider* provider) {
            return provider;
          },
          &decode_target_graphics_context_provider_));

  // Secondary (clear) video creates its SbPlayer first, while primary
  // (encrypted) video waits for CDM attachment.
  NiceMock<MockMediaResource> secondary_resource;
  auto secondary_video_stream =
      CreateMockDemuxerStream(DemuxerStream::VIDEO, /*encrypted=*/false);
  std::vector<DemuxerStream*> secondary_streams = {
      secondary_video_stream.get()};
  EXPECT_CALL(secondary_resource, GetAllStreams())
      .WillRepeatedly(Return(secondary_streams));

  AddStream(DemuxerStream::VIDEO, /*encrypted=*/true);

  SbPlayer secondary_player = reinterpret_cast<SbPlayer>(new MockSbPlayer());
  SbPlayer primary_player = reinterpret_cast<SbPlayer>(new MockSbPlayer());

  EXPECT_CALL(mock_sbplayer_interface_, Create(_, _, _, _, _, _, _, _))
      .WillOnce(Return(secondary_player))
      .WillOnce(Return(primary_player));

  NiceMock<MockRendererClient> secondary_client;
  base::MockOnceCallback<void(PipelineStatus)> secondary_init_cb;
  secondary_renderer->Initialize(&secondary_resource, &secondary_client,
                                 secondary_init_cb.Get());

  EXPECT_CALL(renderer_client_, OnWaiting(WaitingReason::kNoCdm));
  renderer_->Initialize(&media_resource_, &renderer_client_,
                        renderer_init_cb_.Get());
  task_environment_.RunUntilIdle();

  // While only the secondary video hole is in the compositor, the
  // VideoGeometrySetter service gives it z_index = 0.
  EXPECT_CALL(mock_sbplayer_interface_,
              SetBounds(secondary_player, 0, 1280, 660, 427, 240));
  secondary_renderer->OnVideoGeometryChange(gfx::Rect(1280, 660, 427, 240),
                                            /*z_index=*/0);

  // Now attach CDM to primary renderer so its SbPlayer is created and its
  // video hole is added behind the secondary one in the compositor.
  EXPECT_CALL(set_cdm_cb_, Run(true));
  renderer_->SetCdm(&cdm_context_, set_cdm_cb_.Get());
  task_environment_.RunUntilIdle();

  // The service then gives the primary z_index = 1 and raises the secondary to
  // z_index = 2. The secondary's rect is unchanged, but its new z-index must
  // still reach SbPlayerSetBounds(). The order in which the two renderers are
  // notified does not matter.
  EXPECT_CALL(mock_sbplayer_interface_,
              SetBounds(primary_player, 1, 0, 0, 1920, 1080));
  EXPECT_CALL(mock_sbplayer_interface_,
              SetBounds(secondary_player, 2, 1280, 660, 427, 240));
  secondary_renderer->OnVideoGeometryChange(gfx::Rect(1280, 660, 427, 240),
                                            /*z_index=*/2);
  renderer_->OnVideoGeometryChange(gfx::Rect(0, 0, 1920, 1080), /*z_index=*/1);

  // An identical rect and z-index doesn't reach SbPlayerSetBounds() again.
  renderer_->OnVideoGeometryChange(gfx::Rect(0, 0, 1920, 1080), /*z_index=*/1);
  secondary_renderer->OnVideoGeometryChange(gfx::Rect(1280, 660, 427, 240),
                                            /*z_index=*/2);
}

#if BUILDFLAG(IS_IOS_TVOS)
TEST_F(StarboardRendererTest, RecordsSbPlayerCreateLatenciesForUrlPlayer) {
  base::HistogramTester histogram_tester;
  SbPlayer player = InitializeUrlPlayer();
  EXPECT_EQ(GetTotalCountAcrossOccurrences(histogram_tester,
                                           kSbPlayerCreateHistogram),
            1u);
  EXPECT_EQ(GetTotalCountAcrossOccurrences(
                histogram_tester, kSbPlayerCreateToInitializedHistogram),
            0u);

  ASSERT_TRUE(player_status_cb_);
  player_status_cb_(player, context_, kSbPlayerStateInitialized,
                    /*ticket=*/SB_PLAYER_INITIAL_TICKET);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(GetTotalCountAcrossOccurrences(histogram_tester,
                                           kSbPlayerCreateHistogram),
            1u);
  EXPECT_EQ(GetTotalCountAcrossOccurrences(
                histogram_tester, kSbPlayerCreateToInitializedHistogram),
            1u);
}

TEST_F(StarboardRendererTest, UrlPlayerSrcNotSupportedDuringInitialization) {
  SbPlayer player = InitializeUrlPlayer();
  ASSERT_FALSE(init_status_.has_value());
  EXPECT_CALL(renderer_client_, OnError(_)).Times(0);

  ReportPlayerError(
      player, static_cast<SbPlayerError>(kSbUrlPlayerErrorSrcNotSupported),
      "AV key value is not loaded.");

  ASSERT_TRUE(init_status_.has_value());
  EXPECT_EQ(init_status_->code(), DEMUXER_ERROR_COULD_NOT_OPEN);
}

TEST_F(StarboardRendererTest, UrlPlayerSrcNotSupportedAfterInitialization) {
  SbPlayer player = InitializeUrlPlayer();
  ASSERT_TRUE(player_status_cb_);
  player_status_cb_(player, context_, kSbPlayerStateInitialized,
                    SB_PLAYER_INITIAL_TICKET);
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(init_status_.has_value());
  ASSERT_EQ(init_status_->code(), PIPELINE_OK);

  EXPECT_CALL(renderer_client_,
              OnError(HasStatusCode(DEMUXER_ERROR_COULD_NOT_OPEN)));
  ReportPlayerError(
      player, static_cast<SbPlayerError>(kSbUrlPlayerErrorSrcNotSupported),
      "item failed");
}
#endif  // BUILDFLAG(IS_IOS_TVOS)

}  // namespace

}  // namespace media
