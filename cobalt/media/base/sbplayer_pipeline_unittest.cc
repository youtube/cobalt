// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/media/base/sbplayer_pipeline.h"

#include <utility>

#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cobalt/media/base/mock_filters.h"
#include "cobalt/media/base/pipeline.h"
#include "cobalt/media/base/sbplayer_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/chromium/media/filters/chunk_demuxer.h"

namespace cobalt {
namespace media {


// ACTION_P(SetDemuxerProperties, duration) { arg0->SetDuration(duration); }
//
// ACTION_TEMPLATE(PostCallback, HAS_1_TEMPLATE_PARAMS(int, k),
//                 AND_1_VALUE_PARAMS(p0)) {
//   base::ThreadTaskRunnerHandle::Get()->PostTask(
//       FROM_HERE, base::BindOnce(std::move(std::get<k>(args)), p0));
// }


using ::cobalt::media::DefaultSbPlayerInterface;
using ::cobalt::media::SbPlayerInterface;
using ::media::ChunkDemuxer;
using ::media::Demuxer;
using ::media::DemuxerHost;
using ::media::DemuxerStream;
using ::media::MediaTrack;
using ::media::PIPELINE_OK;
using ::media::PipelineStatusCallback;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrictMock;


namespace {
SbDecodeTargetGraphicsContextProvider*
FakeGetSbDecodeTargetGraphicsContextProvider() {
  return NULL;
}
}  // namespace


class SbPlayerPipelineTest : public ::testing::Test {
 public:
  class MockWebMediaPlayerImpl {
   public:
    MockWebMediaPlayerImpl() = default;

    MockWebMediaPlayerImpl(const MockWebMediaPlayerImpl&) = delete;
    MockWebMediaPlayerImpl& operator=(const MockWebMediaPlayerImpl&) = delete;

    virtual ~MockWebMediaPlayerImpl() = default;

    MOCK_METHOD(void, OnDemuxerOpened, ());
    MOCK_METHOD(void, OnProgress, ());
    MOCK_METHOD(void, OnEncryptedMediaInitDataEncounteredWrapper,
                (::media::EmeInitDataType init_data_type,
                 const std::vector<uint8_t>& init_data));

    MOCK_METHOD(void, OnDrmSystemReady,
                (const DrmSystemReadyCB& drm_system_ready_cb));
    MOCK_METHOD(void, OnPipelineEnded, (PipelineStatus));
    MOCK_METHOD(void, OnPipelineError,
                (PipelineStatus, const std::string& message));
    MOCK_METHOD(void, OnPipelineSeek,
                (PipelineStatus, bool is_initial_preroll,
                 const std::string& error_message));
    MOCK_METHOD(void, OnPipelineBufferingState,
                (cobalt::media::Pipeline::BufferingState));
    MOCK_METHOD(void, OnDurationChanged, ());
    MOCK_METHOD(void, OnOutputModeChanged, ());
    MOCK_METHOD(void, OnContentSizeChanged, ());
  };

  SbPlayerPipelineTest() {
    sbplayer_interface_ = std::make_unique<StrictMock<MockSbPlayerInterface>>();

    decode_target_provider_ = new StrictMock<MockDecodeTargetProvider>();

    chunk_demuxer_ = std::make_unique<ChunkDemuxer>(
        base::BindRepeating(&MockWebMediaPlayerImpl::OnDemuxerOpened,
                            base::Unretained(&wmpi_)),
        base::BindRepeating(&MockWebMediaPlayerImpl::OnProgress,
                            base::Unretained(&wmpi_)),
        base::BindRepeating(
            &MockWebMediaPlayerImpl::OnEncryptedMediaInitDataEncounteredWrapper,
            base::Unretained(&wmpi_)),
        &media_log_);

    pipeline_ = new SbPlayerPipeline(
        sbplayer_interface_.get(), nullptr,
        task_environment_.GetMainThreadTaskRunner(),
        base::Bind(FakeGetSbDecodeTargetGraphicsContextProvider), true, false,
        true,
#if SB_API_VERSION >= 15
        kSbPlayerWriteDurationLocal, kSbPlayerWriteDurationRemote,
#endif  // SB_API_VERSION >= 15
        nullptr, &media_metrics_provider_, decode_target_provider_.get());
  }

  ~SbPlayerPipelineTest() = default;

 protected:
  // Sets up expectations to allow the demuxer to initialize.
  // void SetDemuxerExpectations(base::TimeDelta duration) {
  //   EXPECT_CALL(wmpi_, OnDurationChange());
  //   // EXPECT_CALL(*demuxer_, OnInitialize(_, _))
  //   //     .WillOnce(DoAll(SaveArg<0>(&demuxer_host_),
  //   //                     SetDemuxerProperties(duration),
  //   //                     PostCallback<1>(PIPELINE_OK)));
  //   // EXPECT_CALL(*demuxer_,
  //   GetAllStreams()).WillRepeatedly(Return(streams_));
  // }

  // void SetDemuxerExpectations() {
  //   // Initialize with a default non-zero duration.
  //   SetDemuxerExpectations(base::Seconds(10));
  // }


  std::unique_ptr<StrictMock<MockDemuxerStream>> CreateStream(
      DemuxerStream::Type type) {
    return std::make_unique<StrictMock<MockDemuxerStream>>(type);
  }

  void CreateAudioStream() {
    audio_stream_ = CreateStream(DemuxerStream::AUDIO);
    streams_.push_back(audio_stream_.get());
  }

  void StartPipeline() {
    pipeline_->Start(
        chunk_demuxer_.get(),
        base::BindRepeating(&MockWebMediaPlayerImpl::OnDrmSystemReady,
                            base::Unretained(&wmpi_)),
        base::BindRepeating(&MockWebMediaPlayerImpl::OnPipelineEnded,
                            base::Unretained(&wmpi_)),
        base::BindRepeating(&MockWebMediaPlayerImpl::OnPipelineError,
                            base::Unretained(&wmpi_)),
        base::BindRepeating(&MockWebMediaPlayerImpl::OnPipelineSeek,
                            base::Unretained(&wmpi_)),
        base::BindRepeating(&MockWebMediaPlayerImpl::OnPipelineBufferingState,
                            base::Unretained(&wmpi_)),
        base::BindRepeating(&MockWebMediaPlayerImpl::OnDurationChanged,
                            base::Unretained(&wmpi_)),
        base::BindRepeating(&MockWebMediaPlayerImpl::OnOutputModeChanged,
                            base::Unretained(&wmpi_)),
        base::BindRepeating(&MockWebMediaPlayerImpl::OnContentSizeChanged,
                            base::Unretained(&wmpi_)),
        "");

    // EXPECT_CALL(wmpi_, OnWaiting(_)).Times(0);
    // EXPECT_CALL(wmpi_, OnWaiting(_)).Times(0);
  }

  base::test::ScopedTaskEnvironment task_environment_;

  StrictMock<MockWebMediaPlayerImpl> wmpi_;

  std::unique_ptr<StrictMock<MockSbPlayerInterface>> sbplayer_interface_;
  scoped_refptr<StrictMock<MockDecodeTargetProvider>> decode_target_provider_;

  scoped_refptr<Pipeline> pipeline_;

  std::unique_ptr<::media::Demuxer> progressive_demuxer_;
  std::unique_ptr<ChunkDemuxer> chunk_demuxer_;

  // std::unique_ptr<StrictMock<MockRenderer>> scoped_renderer_;
  // base::WeakPtr<MockRenderer> renderer_;
  std::unique_ptr<StrictMock<MockDemuxerStream>> audio_stream_;
  std::unique_ptr<StrictMock<MockDemuxerStream>> video_stream_;
  std::vector<DemuxerStream*> streams_;

  base::TimeDelta start_time_;
  MediaMetricsProvider media_metrics_provider_;

  ::media::MediaLog media_log_;
};


TEST_F(SbPlayerPipelineTest, ConstructAndDestroy) {
  EXPECT_FALSE(pipeline_->HasAudio());
  EXPECT_FALSE(pipeline_->HasVideo());
  EXPECT_EQ(1.0, pipeline_->GetVolume());
  EXPECT_EQ(0.0, pipeline_->GetPlaybackRate());
}

TEST_F(SbPlayerPipelineTest, PipelineStart) {
  EXPECT_CALL(wmpi_, OnDemuxerOpened());
  StartPipeline();
  base::RunLoop().RunUntilIdle();
}

constexpr char kInputWebm[] = "bear-320x240.webm";

std::string Wurd(std::vector<char>& chars) {
  std::stringstream ss;
  for (const auto& c : chars) {
    ss << c;
  }
  return ss.str();
}

TEST_F(SbPlayerPipelineTest, ReadTestFileData) {
  LOG(INFO) << "YO THOR:" << kSbSystemPathTempDirectory;
  std::vector<char> temp_path(kSbFileMaxPath);
  bool system_path_success = SbSystemGetPath(
      kSbSystemPathTempDirectory, temp_path.data(), temp_path.size());
  LOG(INFO) << "YO THOR:" << kSbSystemPathTempDirectory << " "
            << Wurd(temp_path);
  //  const std::string filename = ResolveTestFilename(kInputWebm);
  //  std::ifstream input(filename, std::ios_base::binary)
}

// /// CreatePlayer(SbDrmSystem drm_system
// TEST_F(SbPlayerPipelineTest, SetDrmSystem) {
//   // CreatePlayer
//   StartPipeline();
//   // pipeline_->OnDemuxerInitialized(PIPELINE_OK);
//   // pipeline_->OnDemuxerInitialized(PIPELINE_ERROR_ABORT);
//   // PIPELINE_ERROR_INITIALIZATION_FAILED = 6,
//   // PIPELINE_ERROR_COULD_NOT_RENDER = 8,
//   // PIPELINE_ERROR_READ = 9,
//
//   // FRIEND_TEST(SbPlayerPipelineTest, SetDrmSystem);
//   base::RunLoop().RunUntilIdle();
//
// }

// SUSPEND and RESUME
//   void Stop(const base::Closure& stop_cb) override;
//   void Seek(base::TimeDelta time, const SeekCB& seek_cb);
//   bool HasAudio() const override;
//   bool HasVideo() const override;

//   float GetPlaybackRate() const override;
//   void SetPlaybackRate(float playback_rate) override;
//   float GetVolume() const override;
//   void SetVolume(float volume) override;

//   base::TimeDelta GetMediaTime() override;
//   ::media::Ranges<base::TimeDelta> GetBufferedTimeRanges() override;
//   base::TimeDelta GetMediaDuration() const override;
// #if SB_HAS(PLAYER_WITH_URL)
//   base::TimeDelta GetMediaStartDate() const override;
// #endif  // SB_HAS(PLAYER_WITH_URL)
//   void GetNaturalVideoSize(gfx::Size* out_size) const override;
//   std::vector<std::string> GetAudioConnectors() const override;

//   bool DidLoadingProgress() const override;
//   PipelineStatistics GetStatistics() const override;
//   SetBoundsCB GetSetBoundsCB() override;
//   void SetPreferredOutputModeToDecodeToTexture() override;
// //
// const SetDrmSystemReadyCB& set_drm_system_ready_cb,
// const PipelineStatusCB& ended_cb,
// const ErrorCB& error_cb, const SeekCB& seek_cb,
// const BufferingStateCB& buffering_state_cb,
// const base::Closure& duration_change_cb,
// const base::Closure& output_mode_change_cb,
// const base::Closure& content_size_change_cb,
// const std::string& max_video_capabilities) {

}  // namespace media
}  // namespace cobalt
