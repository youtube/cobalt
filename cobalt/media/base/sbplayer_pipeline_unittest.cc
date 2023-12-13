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
#include "cobalt/media/base/mock_filters.h"
#include "cobalt/media/base/sbplayer_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace media {


ACTION_P(SetDemuxerProperties, duration) { arg0->SetDuration(duration); }

ACTION_TEMPLATE(PostCallback, HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(p0)) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(std::get<k>(args)), p0));
}


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
  class CallbackHelper : public MockPipelineClient {
   public:
    CallbackHelper() = default;

    CallbackHelper(const CallbackHelper&) = delete;
    CallbackHelper& operator=(const CallbackHelper&) = delete;

    virtual ~CallbackHelper() = default;

    MOCK_METHOD1(OnStart, void(PipelineStatus));
    MOCK_METHOD1(OnSeek, void(PipelineStatus));
    MOCK_METHOD1(OnSuspend, void(PipelineStatus));
    MOCK_METHOD1(OnResume, void(PipelineStatus));
    MOCK_METHOD1(OnCdmAttached, void(bool));
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
  }

  ~SbPlayerPipelineTest() = default;

 protected:
  // Sets up expectations to allow the demuxer to initialize.
  void SetDemuxerExpectations(base::TimeDelta duration) {
    EXPECT_CALL(callbacks_, OnDurationChange());
    EXPECT_CALL(*demuxer_, OnInitialize(_, _))
        .WillOnce(DoAll(SaveArg<0>(&demuxer_host_),
                        SetDemuxerProperties(duration),
                        PostCallback<1>(PIPELINE_OK)));
    EXPECT_CALL(*demuxer_, GetAllStreams()).WillRepeatedly(Return(streams_));
  }

  void SetDemuxerExpectations() {
    // Initialize with a default non-zero duration.
    SetDemuxerExpectations(base::Seconds(10));
  }


  std::unique_ptr<StrictMock<MockDemuxerStream>> CreateStream(
      DemuxerStream::Type type) {
    return std::make_unique<StrictMock<MockDemuxerStream>>(type);
  }

  void CreateAudioStream() {
    audio_stream_ = CreateStream(DemuxerStream::AUDIO);
    streams_.push_back(audio_stream_.get());
  }

  base::test::ScopedTaskEnvironment task_environment_;

  StrictMock<CallbackHelper> callbacks_;

  std::unique_ptr<StrictMock<MockDemuxer>> demuxer_;
  std::unique_ptr<StrictMock<MockSbPlayerInterface>> sbplayer_interface_;
  scoped_refptr<StrictMock<MockDecodeTargetProvider>> decode_target_provider_;

  scoped_refptr<Pipeline> pipeline_;

  // std::unique_ptr<StrictMock<MockRenderer>> scoped_renderer_;
  // base::WeakPtr<MockRenderer> renderer_;
  std::unique_ptr<StrictMock<MockDemuxerStream>> audio_stream_;
  std::unique_ptr<StrictMock<MockDemuxerStream>> video_stream_;
  std::vector<DemuxerStream*> streams_;

  MediaMetricsProvider media_metrics_provider_;
};


TEST_F(SbPlayerPipelineTest, ConstructAndDestroy) {
  EXPECT_FALSE(pipeline_->HasAudio());
  EXPECT_FALSE(pipeline_->HasVideo());
  EXPECT_EQ(1.0, pipeline_->GetVolume());
  EXPECT_EQ(0.0, pipeline_->GetPlaybackRate());
}

TEST_F(SbPlayerPipelineTest, SetVolume) {
  CreateAudioStream();
  SetDemuxerExpectations();

  // The audio renderer should receive a call to SetVolume().
  // float expected = 0.5f;
  // EXPECT_CALL(*renderer_, SetVolume(expected));

  // Initialize then set volume!
  // StartPipelineAndExpect(PIPELINE_OK);
  // pipeline_->SetVolume(expected);
  base::RunLoop().RunUntilIdle();
}


}  // namespace media
}  // namespace cobalt
