// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_video_capturer_source.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/task/bind_post_task.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_sink.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/mediastream/mock_mojo_media_stream_dispatcher_host.h"
#include "third_party/blink/renderer/modules/mediastream/mock_video_capturer_source.h"
#include "third_party/blink/renderer/modules/mediastream/video_track_adapter_settings.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"
#include "third_party/blink/renderer/platform/video_capture/video_capturer_source.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_media.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

using ::testing::_;
using ::testing::InSequence;

namespace blink {

namespace {

class FakeMediaStreamVideoSink : public MediaStreamVideoSink {
 public:
  FakeMediaStreamVideoSink(base::TimeTicks* capture_time,
                           media::VideoFrameMetadata* metadata,
                           base::OnceClosure got_frame_cb)
      : capture_time_(capture_time),
        metadata_(metadata),
        got_frame_cb_(std::move(got_frame_cb)) {}

  void ConnectToTrack(const WebMediaStreamTrack& track) {
    MediaStreamVideoSink::ConnectToTrack(
        track,
        ConvertToBaseRepeatingCallback(
            CrossThreadBindRepeating(&FakeMediaStreamVideoSink::OnVideoFrame,
                                     WTF::CrossThreadUnretained(this))),
        MediaStreamVideoSink::IsSecure::kYes,
        MediaStreamVideoSink::UsesAlpha::kDefault);
  }

  void DisconnectFromTrack() { MediaStreamVideoSink::DisconnectFromTrack(); }

  void OnVideoFrame(scoped_refptr<media::VideoFrame> frame,
                    std::vector<scoped_refptr<media::VideoFrame>> scaled_frames,
                    base::TimeTicks capture_time) {
    *capture_time_ = capture_time;
    *metadata_ = frame->metadata();
    std::move(got_frame_cb_).Run();
  }

 private:
  base::TimeTicks* const capture_time_;
  media::VideoFrameMetadata* const metadata_;
  base::OnceClosure got_frame_cb_;
};

}  // namespace

class MediaStreamVideoCapturerSourceTest : public testing::Test {
 public:
  MediaStreamVideoCapturerSourceTest() : source_stopped_(false) {
    auto delegate = std::make_unique<MockVideoCapturerSource>();
    delegate_ = delegate.get();
    EXPECT_CALL(*delegate_, GetPreferredFormats());
    auto video_capturer_source =
        std::make_unique<MediaStreamVideoCapturerSource>(
            scheduler::GetSingleThreadTaskRunnerForTesting(),
            /*LocalFrame =*/nullptr,
            WTF::BindOnce(&MediaStreamVideoCapturerSourceTest::OnSourceStopped,
                          WTF::Unretained(this)),
            std::move(delegate));
    video_capturer_source_ = video_capturer_source.get();
    video_capturer_source_->SetMediaStreamDispatcherHostForTesting(
        mock_dispatcher_host_.CreatePendingRemoteAndBind());
    stream_source_ = MakeGarbageCollected<MediaStreamSource>(
        "dummy_source_id", MediaStreamSource::kTypeVideo, "dummy_source_name",
        false /* remote */, std::move(video_capturer_source));
    stream_source_id_ = stream_source_->Id();

    MediaStreamVideoCapturerSource::DeviceCapturerFactoryCallback callback =
        WTF::BindRepeating(
            &MediaStreamVideoCapturerSourceTest::RecreateVideoCapturerSource,
            WTF::Unretained(this));
    video_capturer_source_->SetDeviceCapturerFactoryCallbackForTesting(
        std::move(callback));
  }

  void TearDown() override {
    stream_source_ = nullptr;
    WebHeap::CollectAllGarbageForTesting();
  }

  WebMediaStreamTrack StartSource(
      const VideoTrackAdapterSettings& adapter_settings,
      const absl::optional<bool>& noise_reduction,
      bool is_screencast,
      double min_frame_rate) {
    bool enabled = true;
    // CreateVideoTrack will trigger OnConstraintsApplied.
    return MediaStreamVideoTrack::CreateVideoTrack(
        video_capturer_source_, adapter_settings, noise_reduction,
        is_screencast, min_frame_rate, absl::nullopt, absl::nullopt,
        absl::nullopt, false,
        WTF::BindOnce(&MediaStreamVideoCapturerSourceTest::OnConstraintsApplied,
                      base::Unretained(this)),
        enabled);
  }

  MockVideoCapturerSource& mock_delegate() { return *delegate_; }

  void OnSourceStopped(const WebMediaStreamSource& source) {
    source_stopped_ = true;
    if (source.IsNull())
      return;
    EXPECT_EQ(String(source.Id()), stream_source_id_);
  }
  void OnStarted(bool result) {
    RunState run_state = result ? RunState::kRunning : RunState::kStopped;
    video_capturer_source_->OnRunStateChanged(delegate_->capture_params(),
                                              run_state);
  }

  void SetStopCaptureFlag() { stop_capture_flag_ = true; }

  MOCK_METHOD0(MockNotification, void());

  std::unique_ptr<VideoCapturerSource> RecreateVideoCapturerSource(
      const base::UnguessableToken& session_id) {
    auto delegate = std::make_unique<MockVideoCapturerSource>();
    delegate_ = delegate.get();
    EXPECT_CALL(*delegate_, MockStartCapture(_, _, _));
    return delegate;
  }

 protected:
  void OnConstraintsApplied(WebPlatformMediaStreamSource* source,
                            mojom::blink::MediaStreamRequestResult result,
                            const WebString& result_name) {}

  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;

  Persistent<MediaStreamSource> stream_source_;
  MockMojoMediaStreamDispatcherHost mock_dispatcher_host_;
  MediaStreamVideoCapturerSource*
      video_capturer_source_;          // owned by |stream_source_|.
  MockVideoCapturerSource* delegate_;  // owned by |source_|.
  String stream_source_id_;
  bool source_stopped_;
  bool stop_capture_flag_ = false;
};

TEST_F(MediaStreamVideoCapturerSourceTest, StartAndStop) {
  InSequence s;
  EXPECT_CALL(mock_delegate(), MockStartCapture(_, _, _));
  WebMediaStreamTrack track =
      StartSource(VideoTrackAdapterSettings(), absl::nullopt, false, 0.0);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(MediaStreamSource::kReadyStateLive,
            stream_source_->GetReadyState());
  EXPECT_FALSE(source_stopped_);

  // A bogus notification of running from the delegate when the source has
  // already started should not change the state.
  delegate_->SetRunning(true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(MediaStreamSource::kReadyStateLive,
            stream_source_->GetReadyState());
  EXPECT_FALSE(source_stopped_);
  EXPECT_TRUE(video_capturer_source_->GetCurrentFormat().has_value());

  // If the delegate stops, the source should stop.
  EXPECT_CALL(mock_delegate(), MockStopCapture());
  delegate_->SetRunning(false);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(MediaStreamSource::kReadyStateEnded,
            stream_source_->GetReadyState());
  // Verify that WebPlatformMediaStreamSource::SourceStoppedCallback has
  // been triggered.
  EXPECT_TRUE(source_stopped_);
}

TEST_F(MediaStreamVideoCapturerSourceTest, CaptureTimeAndMetadataPlumbing) {
  VideoCaptureDeliverFrameCB deliver_frame_cb;
  VideoCapturerSource::RunningCallback running_cb;

  InSequence s;
  EXPECT_CALL(mock_delegate(), MockStartCapture(_, _, _))
      .WillOnce(testing::DoAll(testing::SaveArg<1>(&deliver_frame_cb),
                               testing::SaveArg<2>(&running_cb)));
  EXPECT_CALL(mock_delegate(), RequestRefreshFrame());
  EXPECT_CALL(mock_delegate(), MockStopCapture());
  WebMediaStreamTrack track =
      StartSource(VideoTrackAdapterSettings(), absl::nullopt, false, 0.0);
  running_cb.Run(RunState::kRunning);

  base::RunLoop run_loop;
  base::TimeTicks reference_capture_time =
      base::TimeTicks::FromInternalValue(60013);
  base::TimeTicks capture_time;
  media::VideoFrameMetadata metadata;
  FakeMediaStreamVideoSink fake_sink(
      &capture_time, &metadata,
      base::BindPostTaskToCurrentDefault(run_loop.QuitClosure()));
  fake_sink.ConnectToTrack(track);
  const scoped_refptr<media::VideoFrame> frame =
      media::VideoFrame::CreateBlackFrame(gfx::Size(2, 2));
  frame->metadata().frame_rate = 30.0;
  PostCrossThreadTask(
      *Platform::Current()->GetIOTaskRunner(), FROM_HERE,
      CrossThreadBindOnce(deliver_frame_cb, frame,
                          std::vector<scoped_refptr<media::VideoFrame>>(),
                          reference_capture_time));
  run_loop.Run();
  fake_sink.DisconnectFromTrack();
  EXPECT_EQ(reference_capture_time, capture_time);
  EXPECT_EQ(30.0, *metadata.frame_rate);
}

TEST_F(MediaStreamVideoCapturerSourceTest, Restart) {
  InSequence s;
  EXPECT_CALL(mock_delegate(), MockStartCapture(_, _, _));
  WebMediaStreamTrack track =
      StartSource(VideoTrackAdapterSettings(), absl::nullopt, false, 0.0);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(MediaStreamSource::kReadyStateLive,
            stream_source_->GetReadyState());
  EXPECT_FALSE(source_stopped_);

  EXPECT_CALL(mock_delegate(), MockStopCapture());
  EXPECT_TRUE(video_capturer_source_->IsRunning());
  video_capturer_source_->StopForRestart(
      WTF::BindOnce([](MediaStreamVideoSource::RestartResult result) {
        EXPECT_EQ(result, MediaStreamVideoSource::RestartResult::IS_STOPPED);
      }));
  base::RunLoop().RunUntilIdle();
  // When the source has stopped for restart, the source is not considered
  // stopped, even if the underlying delegate is not running anymore.
  // WebPlatformMediaStreamSource::SourceStoppedCallback should not be
  // triggered.
  EXPECT_EQ(stream_source_->GetReadyState(),
            MediaStreamSource::kReadyStateLive);
  EXPECT_FALSE(source_stopped_);
  EXPECT_FALSE(video_capturer_source_->IsRunning());

  // A second StopForRestart() should fail with invalid state, since it only
  // makes sense when the source is running. Existing ready state should remain
  // the same.
  EXPECT_FALSE(video_capturer_source_->IsRunning());
  video_capturer_source_->StopForRestart(
      WTF::BindOnce([](MediaStreamVideoSource::RestartResult result) {
        EXPECT_EQ(result, MediaStreamVideoSource::RestartResult::INVALID_STATE);
      }));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(stream_source_->GetReadyState(),
            MediaStreamSource::kReadyStateLive);
  EXPECT_FALSE(source_stopped_);
  EXPECT_FALSE(video_capturer_source_->IsRunning());

  // Restart the source. With the mock delegate, any video format will do.
  EXPECT_CALL(mock_delegate(), MockStartCapture(_, _, _));
  EXPECT_FALSE(video_capturer_source_->IsRunning());
  video_capturer_source_->Restart(
      media::VideoCaptureFormat(),
      WTF::BindOnce([](MediaStreamVideoSource::RestartResult result) {
        EXPECT_EQ(result, MediaStreamVideoSource::RestartResult::IS_RUNNING);
      }));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(stream_source_->GetReadyState(),
            MediaStreamSource::kReadyStateLive);
  EXPECT_TRUE(video_capturer_source_->IsRunning());

  // A second Restart() should fail with invalid state since Restart() is
  // defined only when the source is stopped for restart. Existing ready state
  // should remain the same.
  EXPECT_TRUE(video_capturer_source_->IsRunning());
  video_capturer_source_->Restart(
      media::VideoCaptureFormat(),
      WTF::BindOnce([](MediaStreamVideoSource::RestartResult result) {
        EXPECT_EQ(result, MediaStreamVideoSource::RestartResult::INVALID_STATE);
      }));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(stream_source_->GetReadyState(),
            MediaStreamSource::kReadyStateLive);
  EXPECT_TRUE(video_capturer_source_->IsRunning());

  // An delegate stop should stop the source and change the track state to
  // "ended".
  EXPECT_CALL(mock_delegate(), MockStopCapture());
  delegate_->SetRunning(false);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(MediaStreamSource::kReadyStateEnded,
            stream_source_->GetReadyState());
  // Verify that WebPlatformMediaStreamSource::SourceStoppedCallback has
  // been triggered.
  EXPECT_TRUE(source_stopped_);
  EXPECT_FALSE(video_capturer_source_->IsRunning());
}

TEST_F(MediaStreamVideoCapturerSourceTest, StartStopAndNotify) {
  InSequence s;
  EXPECT_CALL(mock_delegate(), MockStartCapture(_, _, _));
  WebMediaStreamTrack web_track =
      StartSource(VideoTrackAdapterSettings(), absl::nullopt, false, 0.0);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(MediaStreamSource::kReadyStateLive,
            stream_source_->GetReadyState());
  EXPECT_FALSE(source_stopped_);

  stop_capture_flag_ = false;
  EXPECT_CALL(mock_delegate(), MockStopCapture())
      .WillOnce(InvokeWithoutArgs(
          this, &MediaStreamVideoCapturerSourceTest::SetStopCaptureFlag));
  EXPECT_CALL(*this, MockNotification());
  MediaStreamTrackPlatform* track =
      MediaStreamTrackPlatform::GetTrack(web_track);
  track->StopAndNotify(
      WTF::BindOnce(&MediaStreamVideoCapturerSourceTest::MockNotification,
                    base::Unretained(this)));
  EXPECT_EQ(MediaStreamSource::kReadyStateEnded,
            stream_source_->GetReadyState());
  EXPECT_TRUE(source_stopped_);
  // It is a requirement that StopCapture() gets called in the same task as
  // StopAndNotify(), as CORS security checks for element capture rely on this.
  EXPECT_TRUE(stop_capture_flag_);
  // The readyState is updated in the current task, but the notification is
  // received on a separate task.
  base::RunLoop().RunUntilIdle();
}

TEST_F(MediaStreamVideoCapturerSourceTest, ChangeSource) {
  InSequence s;
  EXPECT_CALL(mock_delegate(), MockStartCapture(_, _, _));
  WebMediaStreamTrack track =
      StartSource(VideoTrackAdapterSettings(), absl::nullopt, false, 0.0);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(MediaStreamSource::kReadyStateLive,
            stream_source_->GetReadyState());
  EXPECT_FALSE(source_stopped_);

  // A bogus notification of running from the delegate when the source has
  // already started should not change the state.
  delegate_->SetRunning(true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(MediaStreamSource::kReadyStateLive,
            stream_source_->GetReadyState());
  EXPECT_FALSE(source_stopped_);

  // |ChangeSourceImpl()| will recreate the |delegate_|, so check the
  // |MockStartCapture()| invoking in the |RecreateVideoCapturerSource()|.
  EXPECT_CALL(mock_delegate(), MockStopCapture());
  MediaStreamDevice fake_video_device(
      mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE, "Fake_Video_Device",
      "Fake Video Device");
  video_capturer_source_->ChangeSourceImpl(fake_video_device);
  EXPECT_EQ(MediaStreamSource::kReadyStateLive,
            stream_source_->GetReadyState());
  EXPECT_FALSE(source_stopped_);

  // If the delegate stops, the source should stop.
  EXPECT_CALL(mock_delegate(), MockStopCapture());
  delegate_->SetRunning(false);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(MediaStreamSource::kReadyStateEnded,
            stream_source_->GetReadyState());
  // Verify that WebPlatformMediaStreamSource::SourceStoppedCallback has
  // been triggered.
  EXPECT_TRUE(source_stopped_);
}

}  // namespace blink
