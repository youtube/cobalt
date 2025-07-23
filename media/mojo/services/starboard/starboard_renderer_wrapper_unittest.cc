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

#include "media/mojo/services/starboard/starboard_renderer_wrapper.h"

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "media/base/media_util.h"
#include "media/base/mock_filters.h"
#include "media/base/test_helpers.h"
#include "media/gpu/starboard/starboard_gpu_factory_impl.h"
#include "media/mojo/mojom/renderer_extensions.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrictMock;

namespace media {

namespace {

class MockStarboardRenderer : public StarboardRenderer {
 public:
  MockStarboardRenderer(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      std::unique_ptr<MediaLog> media_log,
      const base::UnguessableToken& overlay_plane_id,
      TimeDelta audio_write_duration_local,
      TimeDelta audio_write_duration_remote,
      const std::string& max_video_capabilities,
      const AndroidOverlayMojoFactoryCB android_overlay_factory_cb)
      : StarboardRenderer(task_runner,
                          std::move(media_log),
                          overlay_plane_id,
                          audio_write_duration_local,
                          audio_write_duration_remote,
                          max_video_capabilities,
                          android_overlay_factory_cb) {}

  MockStarboardRenderer(const MockStarboardRenderer&) = delete;
  MockStarboardRenderer& operator=(const MockStarboardRenderer&) = delete;

  ~MockStarboardRenderer() = default;

  // Renderer implementation.
  void Initialize(MediaResource* media_resource,
                  RendererClient* client,
                  PipelineStatusCallback init_cb) override {
    OnInitialize(media_resource, client, init_cb);
  }
  MOCK_METHOD3(OnInitialize,
               void(MediaResource* media_resource,
                    RendererClient* client,
                    PipelineStatusCallback& init_cb));
  MOCK_METHOD1(SetLatencyHint, void(absl::optional<TimeDelta>));
  MOCK_METHOD1(SetPreservesPitch, void(bool));
  MOCK_METHOD1(SetWasPlayedWithUserActivation, void(bool));
  void Flush(base::OnceClosure flush_cb) override { OnFlush(flush_cb); }
  MOCK_METHOD1(OnFlush, void(base::OnceClosure& flush_cb));
  MOCK_METHOD1(StartPlayingFrom, void(base::TimeDelta timestamp));
  MOCK_METHOD1(SetPlaybackRate, void(double playback_rate));
  MOCK_METHOD1(SetVolume, void(float volume));
  MOCK_METHOD0(GetMediaTime, base::TimeDelta());
  MOCK_METHOD0(HasAudio, bool());
  MOCK_METHOD0(HasVideo, bool());
  void SetCdm(CdmContext* cdm_context, CdmAttachedCB cdm_attached_cb) override {
    OnSetCdm(cdm_context, cdm_attached_cb);
  }
  MOCK_METHOD2(OnSetCdm,
               void(CdmContext* cdm_context, CdmAttachedCB& cdm_attached_cb));
  MOCK_METHOD2(OnSelectedVideoTrackChanged,
               void(std::vector<DemuxerStream*>, base::OnceClosure));
  MOCK_METHOD2(OnSelectedAudioTracksChanged,
               void(std::vector<DemuxerStream*>, base::OnceClosure));
  RendererType GetRendererType() override { return RendererType::kTest; }

  base::WeakPtr<MockStarboardRenderer> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockStarboardRenderer> weak_ptr_factory_{this};
};

class MockStarboardGpuFactory : public StarboardGpuFactory {
 public:
  MockStarboardGpuFactory() = default;

  MockStarboardGpuFactory(const MockStarboardGpuFactory&) = delete;
  MockStarboardGpuFactory& operator=(const MockStarboardGpuFactory&) = delete;

  ~MockStarboardGpuFactory() = default;

  void Initialize(base::UnguessableToken channel_token,
                  int32_t route_id,
                  base::OnceClosure callback) override {
    std::move(callback).Run();
  }

 private:
  void OnWillDestroyStub(bool have_context) override {}
};

class StarboardRendererWrapperTest : public testing::Test {
 protected:
  using RendererExtension = mojom::StarboardRendererExtension;
  using ClientExtension = mojom::StarboardRendererClientExtension;

  StarboardRendererWrapperTest()
      : mock_renderer_(std::make_unique<StrictMock<MockStarboardRenderer>>(
            task_environment_.GetMainThreadTaskRunner(),
            std::make_unique<NullMediaLog>(),
            base::UnguessableToken::Create(),
            base::Seconds(1),
            base::Seconds(1),
            std::string(),
            base::NullCallback())),
        mock_gpu_factory_(task_environment_.GetMainThreadTaskRunner()) {
    // Setup MockStarboardGpuFactory as StarboardGpuFactory so
    // it can overwrite |gpu_factory_| in StarboardRendererWrapper
    // via SetGpuFactoryForTesting().
    gpu_factory_ = std::move(mock_gpu_factory_);
    // Setup traits required by MockStarboardRendererWrapper.
    auto media_log_remote = media_log_.InitWithNewPipeAndPassRemote();
    auto renderer_extension_receiver =
        renderer_extension_.BindNewPipeAndPassReceiver();
    auto client_extension_remote =
        client_extension_.InitWithNewPipeAndPassRemote();
    StarboardRendererTraits traits(
        task_environment_.GetMainThreadTaskRunner(),
        task_environment_.GetMainThreadTaskRunner(),
        std::move(media_log_remote), base::UnguessableToken::Create(),
        base::Seconds(1), base::Seconds(1), std::string(),
        std::move(renderer_extension_receiver),
        std::move(client_extension_remote), base::NullCallback(),
        base::NullCallback());
    renderer_wrapper_ =
        std::make_unique<StarboardRendererWrapper>(std::move(traits));
    renderer_wrapper_->SetRendererForTesting(mock_renderer_.get());
    renderer_wrapper_->SetGpuFactoryForTesting(&gpu_factory_);

    EXPECT_CALL(media_resource_, GetAllStreams())
        .WillRepeatedly(
            Invoke(this, &StarboardRendererWrapperTest::GetAllStreams));
    EXPECT_CALL(media_resource_, GetType())
        .WillRepeatedly(Return(MediaResource::STREAM));
  }

  ~StarboardRendererWrapperTest() override {
    mock_renderer_.reset();
    renderer_wrapper_.reset();
  }

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

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<StrictMock<MockStarboardRenderer>> mock_renderer_;
  base::SequenceBound<StrictMock<MockStarboardGpuFactory>> mock_gpu_factory_;
  base::SequenceBound<StarboardGpuFactory> gpu_factory_;
  std::unique_ptr<StarboardRendererWrapper> renderer_wrapper_;
  NiceMock<MockMediaResource> media_resource_;
  NiceMock<MockRendererClient> renderer_client_;
  base::MockOnceCallback<void(PipelineStatus)> renderer_init_cb_;
  std::vector<std::unique_ptr<StrictMock<MockDemuxerStream>>> streams_;

  mojo::PendingReceiver<mojom::MediaLog> media_log_;
  mojo::Remote<mojom::StarboardRendererExtension> renderer_extension_;
  mojo::PendingReceiver<mojom::StarboardRendererClientExtension>
      client_extension_;
};

TEST_F(StarboardRendererWrapperTest, InitializeWithoutGpuChannelToken) {
  AddStream(DemuxerStream::AUDIO, false);
  AddStream(DemuxerStream::VIDEO, false);

  EXPECT_CALL(*mock_renderer_, OnInitialize(_, _, _))
      .WillOnce(RunOnceCallback<2>(PIPELINE_OK));
  EXPECT_CALL(renderer_init_cb_, Run(HasStatusCode(PIPELINE_OK)));
  renderer_wrapper_->Initialize(&media_resource_, &renderer_client_,
                                renderer_init_cb_.Get());

  task_environment_.RunUntilIdle();
}

TEST_F(StarboardRendererWrapperTest, InitializeWithGpuChannelToken) {
  AddStream(DemuxerStream::AUDIO, false);
  AddStream(DemuxerStream::VIDEO, false);

  auto command_buffer_id = mojom::CommandBufferId::New();
  command_buffer_id->channel_token = base::UnguessableToken::Create();
  command_buffer_id->route_id = 0;
  renderer_wrapper_->OnGpuChannelTokenReady(std::move(command_buffer_id));

  EXPECT_CALL(*mock_renderer_, OnInitialize(_, _, _))
      .WillOnce(RunOnceCallback<2>(PIPELINE_OK));
  EXPECT_CALL(renderer_init_cb_, Run(HasStatusCode(PIPELINE_OK)));
  renderer_wrapper_->Initialize(&media_resource_, &renderer_client_,
                                renderer_init_cb_.Get());

  task_environment_.RunUntilIdle();
}

TEST_F(StarboardRendererWrapperTest, Flush) {
  base::MockOnceClosure closure;
  EXPECT_CALL(*mock_renderer_, OnFlush(_)).WillOnce(RunOnceCallback<0>());
  EXPECT_CALL(closure, Run());
  renderer_wrapper_->Flush(closure.Get());
  task_environment_.RunUntilIdle();
}

TEST_F(StarboardRendererWrapperTest, StartPlayingFrom) {
  const base::TimeDelta kStartTime = base::Seconds(5);
  EXPECT_CALL(*mock_renderer_, StartPlayingFrom(kStartTime));
  renderer_wrapper_->StartPlayingFrom(kStartTime);
  task_environment_.RunUntilIdle();
}

TEST_F(StarboardRendererWrapperTest, SetPlaybackRate) {
  const double kPlaybackRate = 2.5;
  EXPECT_CALL(*mock_renderer_, SetPlaybackRate(kPlaybackRate));
  renderer_wrapper_->SetPlaybackRate(kPlaybackRate);
  task_environment_.RunUntilIdle();
}

TEST_F(StarboardRendererWrapperTest, SetVolume) {
  const float kVolume = 0.75f;
  EXPECT_CALL(*mock_renderer_, SetVolume(kVolume));
  renderer_wrapper_->SetVolume(kVolume);
  task_environment_.RunUntilIdle();
}

TEST_F(StarboardRendererWrapperTest, GetMediaTime) {
  const base::TimeDelta kExpectedTime = base::Seconds(42);
  EXPECT_CALL(*mock_renderer_, GetMediaTime()).WillOnce(Return(kExpectedTime));
  EXPECT_EQ(renderer_wrapper_->GetMediaTime(), kExpectedTime);
  task_environment_.RunUntilIdle();
}

TEST_F(StarboardRendererWrapperTest, SetCdm) {
  base::MockCallback<Renderer::CdmAttachedCB> cdm_attached_cb;
  EXPECT_CALL(*mock_renderer_, OnSetCdm(/*cdm_context=*/nullptr, _))
      .WillOnce(RunOnceCallback<1>(true));
  EXPECT_CALL(cdm_attached_cb, Run(true));
  renderer_wrapper_->SetCdm(/*cdm_context=*/nullptr, cdm_attached_cb.Get());
  task_environment_.RunUntilIdle();
}

TEST_F(StarboardRendererWrapperTest, GetCurrentVideoFrame) {
  base::MockCallback<StarboardRendererWrapper::GetCurrentVideoFrameCallback>
      callback;
  EXPECT_CALL(callback, Run(testing::IsNull()));
  renderer_wrapper_->GetCurrentVideoFrame(callback.Get());
  task_environment_.RunUntilIdle();
}

}  // namespace

}  // namespace media
