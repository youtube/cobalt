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
MockGetSbDecodeTargetGraphicsContextProvider() {
  return NULL;
}
}  // namespace


class SbPlayerPipelineTest : public ::testing::Test {
 public:
  // Used for setting expectations on pipeline callbacks.  Using a StrictMock
  // also lets us test for missing callbacks.
  class CallbackHelper {
   public:
    CallbackHelper() = default;

    CallbackHelper(const CallbackHelper&) = delete;
    CallbackHelper& operator=(const CallbackHelper&) = delete;

    virtual ~CallbackHelper() = default;

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

  SbPlayerPipelineTest()
      : demuxer_(std::make_unique<StrictMock<MockDemuxer>>()),
        sbplayer_interface_(
            std::make_unique<StrictMock<MockSbPlayerInterface>>()) {
    decode_target_provider_ = new StrictMock<MockDecodeTargetProvider>();
    pipeline_ = new SbPlayerPipeline(
        sbplayer_interface_.get(), nullptr,
        task_environment_.GetMainThreadTaskRunner(),
        base::Bind(MockGetSbDecodeTargetGraphicsContextProvider), true, false,
        true,
#if SB_API_VERSION >= 15
        10, 10,
#endif  // SB_API_VERSION >= 15
        nullptr, &media_metrics_provider_, decode_target_provider_.get());

    std::vector<DemuxerStream*> empty;
    EXPECT_CALL(*demuxer_, GetAllStreams()).WillRepeatedly(Return(empty));

    EXPECT_CALL(*demuxer_, GetTimelineOffset())
        .WillRepeatedly(Return(base::Time()));

    EXPECT_CALL(*demuxer_, GetStartTime()).WillRepeatedly(Return(start_time_));
  }

  ~SbPlayerPipelineTest() = default;

 protected:
  // Sets up expectations to allow the demuxer to initialize.
  // void SetDemuxerExpectations(base::TimeDelta duration) {
  //   EXPECT_CALL(callbacks_, OnDurationChange());
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
        demuxer_.get(),
        base::BindRepeating(&CallbackHelper::OnDrmSystemReady,
                            base::Unretained(&callbacks_)),
        base::BindRepeating(&CallbackHelper::OnPipelineEnded,
                            base::Unretained(&callbacks_)),
        base::BindRepeating(&CallbackHelper::OnPipelineError,
                            base::Unretained(&callbacks_)),
        base::BindRepeating(&CallbackHelper::OnPipelineSeek,
                            base::Unretained(&callbacks_)),
        base::BindRepeating(&CallbackHelper::OnPipelineBufferingState,
                            base::Unretained(&callbacks_)),
        base::BindRepeating(&CallbackHelper::OnDurationChanged,
                            base::Unretained(&callbacks_)),
        base::BindRepeating(&CallbackHelper::OnOutputModeChanged,
                            base::Unretained(&callbacks_)),
        base::BindRepeating(&CallbackHelper::OnContentSizeChanged,
                            base::Unretained(&callbacks_)),
        "");

    // EXPECT_CALL(callbacks_, OnWaiting(_)).Times(0);
    // EXPECT_CALL(callbacks_, OnWaiting(_)).Times(0);
  }

  base::test::ScopedTaskEnvironment task_environment_;

  StrictMock<CallbackHelper> callbacks_;

  std::unique_ptr<StrictMock<MockDemuxer>> demuxer_;
  std::unique_ptr<StrictMock<MockSbPlayerInterface>> sbplayer_interface_;
  scoped_refptr<StrictMock<MockDecodeTargetProvider>> decode_target_provider_;

  scoped_refptr<SbPlayerPipeline> pipeline_;

  // std::unique_ptr<StrictMock<MockRenderer>> scoped_renderer_;
  // base::WeakPtr<MockRenderer> renderer_;
  std::unique_ptr<StrictMock<MockDemuxerStream>> audio_stream_;
  std::unique_ptr<StrictMock<MockDemuxerStream>> video_stream_;
  std::vector<DemuxerStream*> streams_;

  base::TimeDelta start_time_;
  MediaMetricsProvider media_metrics_provider_;
};


TEST_F(SbPlayerPipelineTest, ConstructAndDestroy) {
  EXPECT_FALSE(pipeline_->HasAudio());
  EXPECT_FALSE(pipeline_->HasVideo());
  EXPECT_EQ(1.0, pipeline_->GetVolume());
  EXPECT_EQ(0.0, pipeline_->GetPlaybackRate());
}

TEST_F(SbPlayerPipelineTest, PipelineStart) {
  EXPECT_CALL(*demuxer_, OnInitialize(pipeline_.get(), _)).Times(1);
  StartPipeline();
  base::RunLoop().RunUntilIdle();
}

TEST_F(SbPlayerPipelineTest, SetDrmSystem) {
  EXPECT_CALL(*demuxer_, OnInitialize(pipeline_.get(), _)).Times(1);
  StartPipeline();
  base::RunLoop().RunUntilIdle();
}


}  // namespace media
}  // namespace cobalt
