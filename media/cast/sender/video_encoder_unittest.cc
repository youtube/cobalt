// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/sender/video_encoder.h"

#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "media/base/fake_single_thread_task_runner.h"
#include "media/base/video_frame.h"
#include "media/cast/cast_environment.h"
#include "media/cast/common/rtp_time.h"
#include "media/cast/sender/fake_video_encode_accelerator_factory.h"
#include "media/cast/sender/video_frame_factory.h"
#include "media/cast/test/utility/default_config.h"
#include "media/cast/test/utility/video_utility.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_MAC)
#include "media/cast/sender/h264_vt_encoder.h"
#endif

namespace media {
namespace cast {

class VideoEncoderTest
    : public ::testing::TestWithParam<std::pair<Codec, bool>> {
 protected:
  VideoEncoderTest()
      : task_runner_(new FakeSingleThreadTaskRunner(&testing_clock_)),
        task_runner_handle_override_(task_runner_),
        cast_environment_(new CastEnvironment(&testing_clock_,
                                              task_runner_,
                                              task_runner_,
                                              task_runner_)),
        video_config_(GetDefaultVideoSenderConfig()),
        operational_status_(STATUS_UNINITIALIZED) {
    testing_clock_.Advance(base::TimeTicks::Now() - base::TimeTicks());
    first_frame_time_ = testing_clock_.NowTicks();
  }

  ~VideoEncoderTest() override = default;

  void SetUp() final {
    video_config_.codec = GetParam().first;
    video_config_.use_external_encoder = GetParam().second;

    if (video_config_.use_external_encoder) {
      vea_factory_ =
          std::make_unique<FakeVideoEncodeAcceleratorFactory>(task_runner_);
    }
  }

  void TearDown() final {
    video_encoder_.reset();
    RunTasksAndAdvanceClock();
  }

  void CreateEncoder() {
    ASSERT_EQ(STATUS_UNINITIALIZED, operational_status_);
    video_config_.video_codec_params.max_number_of_video_buffers_used = 1;
    video_encoder_ = VideoEncoder::Create(
        cast_environment_, video_config_,
        base::BindRepeating(&VideoEncoderTest::OnOperationalStatusChange,
                            base::Unretained(this)),
        base::BindRepeating(
            &FakeVideoEncodeAcceleratorFactory::CreateVideoEncodeAccelerator,
            base::Unretained(vea_factory_.get())));
    RunTasksAndAdvanceClock();
    if (is_encoder_present())
      ASSERT_EQ(STATUS_INITIALIZED, operational_status_);
  }

  bool is_encoder_present() const {
    return !!video_encoder_;
  }

  bool is_testing_software_vp8_encoder() const {
    return video_config_.codec == CODEC_VIDEO_VP8 &&
        !video_config_.use_external_encoder;
  }

  bool is_testing_video_toolbox_encoder() const {
    return
#if defined(OS_MAC)
        (!video_config_.use_external_encoder &&
         H264VideoToolboxEncoder::IsSupported(video_config_)) ||
#endif
        false;
  }

  bool is_testing_platform_encoder() const {
    return video_config_.use_external_encoder ||
        is_testing_video_toolbox_encoder();
  }

  bool encoder_has_resize_delay() const {
    return is_testing_platform_encoder() && !is_testing_video_toolbox_encoder();
  }

  VideoEncoder* video_encoder() const {
    return video_encoder_.get();
  }

  void DestroyEncoder() {
    video_encoder_.reset();
  }

  base::TimeTicks Now() { return testing_clock_.NowTicks(); }

  void RunTasksAndAdvanceClock() {
    DCHECK_GT(video_config_.max_frame_rate, 0);
    const base::TimeDelta frame_duration =
        base::Microseconds(1000000.0 / video_config_.max_frame_rate);
#if defined(OS_MAC)
    if (is_testing_video_toolbox_encoder()) {
      // The H264VideoToolboxEncoder (on MAC_OSX and IOS) is not a faked
      // implementation in these tests, and performs its encoding asynchronously
      // on an unknown set of threads.  Therefore, sleep the current thread for
      // the real amount of time to avoid excessively spinning the CPU while
      // waiting for something to happen.
      base::PlatformThread::Sleep(frame_duration);
    }
#endif
    task_runner_->RunTasks();
    testing_clock_.Advance(frame_duration);
  }

  // Creates a new VideoFrame of the given |size|, filled with a test pattern.
  // When available, it attempts to use the VideoFrameFactory provided by the
  // encoder.
  scoped_refptr<media::VideoFrame> CreateTestVideoFrame(const gfx::Size& size) {
    const base::TimeDelta timestamp =
        testing_clock_.NowTicks() - first_frame_time_;
    scoped_refptr<media::VideoFrame> frame;
    if (video_frame_factory_) {
      DVLOG(1) << "MaybeCreateFrame";
      frame = video_frame_factory_->MaybeCreateFrame(size, timestamp);
    }
    if (!frame) {
      DVLOG(1) << "No VideoFrame, create using VideoFrame::CreateFrame";
      frame = media::VideoFrame::CreateFrame(PIXEL_FORMAT_I420, size,
                                             gfx::Rect(size), size, timestamp);
    }
    PopulateVideoFrame(frame.get(), 123);
    return frame;
  }

  // If the implementation of |video_encoder_| is ExternalVideoEncoder, check
  // that the VEA factory has responded (by running the callbacks) a specific
  // number of times.  Otherwise, check that the VEA factory is inactive.
  void ExpectVEAResponseForExternalVideoEncoder(int vea_response_count) const {
    if (!vea_factory_)
      return;
    EXPECT_EQ(vea_response_count, vea_factory_->vea_response_count());
  }

  void SetVEAFactoryAutoRespond(bool auto_respond) {
    if (vea_factory_)
      vea_factory_->SetAutoRespond(auto_respond);
  }

 private:
  void OnOperationalStatusChange(OperationalStatus status) {
    DVLOG(1) << "OnOperationalStatusChange: from " << operational_status_
             << " to " << status;
    operational_status_ = status;

    EXPECT_TRUE(operational_status_ == STATUS_CODEC_REINIT_PENDING ||
                operational_status_ == STATUS_INITIALIZED);

    // Create the VideoFrameFactory the first time status changes to
    // STATUS_INITIALIZED.
    if (operational_status_ == STATUS_INITIALIZED && !video_frame_factory_)
      video_frame_factory_ = video_encoder_->CreateVideoFrameFactory();
  }

  base::SimpleTestTickClock testing_clock_;
  const scoped_refptr<FakeSingleThreadTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandleOverrideForTesting task_runner_handle_override_;
  const scoped_refptr<CastEnvironment> cast_environment_;
  FrameSenderConfig video_config_;
  std::unique_ptr<FakeVideoEncodeAcceleratorFactory> vea_factory_;
  base::TimeTicks first_frame_time_;
  OperationalStatus operational_status_;
  std::unique_ptr<VideoEncoder> video_encoder_;
  std::unique_ptr<VideoFrameFactory> video_frame_factory_;

  DISALLOW_COPY_AND_ASSIGN(VideoEncoderTest);
};

// Tests that the encoder outputs encoded frames, and also responds to frame
// size changes. See media/cast/receiver/video_decoder_unittest.cc for a
// complete encode/decode cycle of varied frame sizes that actually checks the
// frame content. Fails consistently on official builds: crbug.com/612496
#ifdef OFFICIAL_BUILD
#define MAYBE_EncodesVariedFrameSizes DISABLED_EncodesVariedFrameSizes
#else
#define MAYBE_EncodesVariedFrameSizes EncodesVariedFrameSizes
#endif
TEST_P(VideoEncoderTest, MAYBE_EncodesVariedFrameSizes) {
  CreateEncoder();
  SetVEAFactoryAutoRespond(true);

  ExpectVEAResponseForExternalVideoEncoder(0);

  std::vector<gfx::Size> frame_sizes;
  frame_sizes.push_back(gfx::Size(128, 72));
  frame_sizes.push_back(gfx::Size(64, 36));    // Shrink both dimensions.
  frame_sizes.push_back(gfx::Size(30, 20));    // Shrink both dimensions again.
  frame_sizes.push_back(gfx::Size(20, 30));    // Same area.
  frame_sizes.push_back(gfx::Size(60, 40));    // Grow both dimensions.
  frame_sizes.push_back(gfx::Size(58, 40));    // Shrink only one dimension.
  frame_sizes.push_back(gfx::Size(58, 38));    // Shrink the other dimension.
  frame_sizes.push_back(gfx::Size(32, 18));    // Shrink both dimensions again.
  frame_sizes.push_back(gfx::Size(34, 18));    // Grow only one dimension.
  frame_sizes.push_back(gfx::Size(34, 20));    // Grow the other dimension.
  frame_sizes.push_back(gfx::Size(192, 108));  // Grow both dimensions again.

  int count_frames_accepted = 0;
  using EncodedFrames = std::vector<std::unique_ptr<SenderEncodedFrame>>;
  EncodedFrames encoded_frames;
  base::WeakPtrFactory<EncodedFrames> encoded_frames_weak_factory(
      &encoded_frames);

  // Encode several frames at each size. For encoders with a resize delay,
  // expect the first one or more frames are dropped while the encoder
  // re-inits. For all encoders, expect one key frame followed by all delta
  // frames.
  for (const auto& frame_size : frame_sizes) {
    // Encode frames until there are four consecutive frames successfully
    // encoded.
    while (encoded_frames.size() <= 4 ||
           !(encoded_frames[encoded_frames.size() - 1] &&
             encoded_frames[encoded_frames.size() - 2] &&
             encoded_frames[encoded_frames.size() - 3] &&
             encoded_frames[encoded_frames.size() - 4])) {
      auto video_frame = CreateTestVideoFrame(frame_size);
      const base::TimeTicks reference_time = Now();
      const base::TimeDelta timestamp = video_frame->timestamp();
      const bool accepted_request = video_encoder()->EncodeVideoFrame(
          std::move(video_frame), reference_time,
          base::BindOnce(
              [](base::WeakPtr<EncodedFrames> encoded_frames,
                 RtpTimeTicks expected_rtp_timestamp,
                 base::TimeTicks expected_reference_time,
                 std::unique_ptr<SenderEncodedFrame> encoded_frame) {
                if (!encoded_frames) {
                  return;
                }
                if (encoded_frame) {
                  EXPECT_EQ(expected_rtp_timestamp,
                            encoded_frame->rtp_timestamp);
                  EXPECT_EQ(expected_reference_time,
                            encoded_frame->reference_time);
                }
                encoded_frames->emplace_back(std::move(encoded_frame));
              },
              encoded_frames_weak_factory.GetWeakPtr(),
              RtpTimeTicks::FromTimeDelta(timestamp, kVideoFrequency),
              reference_time));
      if (accepted_request) {
        ++count_frames_accepted;
      }
      if (!encoder_has_resize_delay()) {
        EXPECT_TRUE(accepted_request);
      }
      RunTasksAndAdvanceClock();
    }
  }

  // Flush the encoder and wait until all queued frames have been delivered.
  // Then, shut it all down.
  video_encoder()->EmitFrames();
  while (encoded_frames.size() < static_cast<size_t>(count_frames_accepted))
    RunTasksAndAdvanceClock();
  DestroyEncoder();
  RunTasksAndAdvanceClock();
  encoded_frames_weak_factory.InvalidateWeakPtrs();

  // Walk through the encoded frames and check that they have reasonable frame
  // IDs, dependency relationships, etc. provided.
  FrameId last_key_frame_id;
  for (const std::unique_ptr<SenderEncodedFrame>& encoded_frame :
       encoded_frames) {
    if (!encoded_frame) {
      continue;
    }

    if (encoded_frame->dependency == EncodedFrame::KEY) {
      EXPECT_EQ(encoded_frame->frame_id, encoded_frame->referenced_frame_id);
      last_key_frame_id = encoded_frame->frame_id;
    } else {
      EXPECT_EQ(EncodedFrame::DEPENDENT, encoded_frame->dependency);
      EXPECT_GT(encoded_frame->frame_id, encoded_frame->referenced_frame_id);
      // There must always be a KEY frame before any DEPENDENT ones.
      ASSERT_FALSE(last_key_frame_id.is_null());
      EXPECT_GE(encoded_frame->referenced_frame_id, last_key_frame_id);
    }

    EXPECT_FALSE(encoded_frame->data.empty());

    // The utilization metrics are computed for all but the Mac Video Toolbox
    // encoder.
    if (is_testing_software_vp8_encoder()) {
      ASSERT_TRUE(std::isfinite(encoded_frame->encoder_utilization));
      EXPECT_LE(0.0, encoded_frame->encoder_utilization);
      ASSERT_TRUE(std::isfinite(encoded_frame->lossy_utilization));
      EXPECT_LE(0.0, encoded_frame->lossy_utilization);
    }
  }
}

// Verify that everything goes well even if ExternalVideoEncoder is destroyed
// before it has a chance to receive the VEA creation callback.  For all other
// encoders, this tests that the encoder can be safely destroyed before the task
// is run that delivers the first EncodedFrame.
// Fails consistently on official builds: crbug.com/612496
#ifdef OFFICIAL_BUILD
#define MAYBE_CanBeDestroyedBeforeVEAIsCreated \
  DISABLED_CanBeDestroyedBeforeVEAIsCreated
#else
#define MAYBE_CanBeDestroyedBeforeVEAIsCreated CanBeDestroyedBeforeVEAIsCreated
#endif
TEST_P(VideoEncoderTest, MAYBE_CanBeDestroyedBeforeVEAIsCreated) {
  CreateEncoder();

  // Send a frame to spawn creation of the ExternalVideoEncoder instance.
  video_encoder()->EncodeVideoFrame(
      CreateTestVideoFrame(gfx::Size(128, 72)), Now(),
      base::BindOnce([](std::unique_ptr<SenderEncodedFrame> encoded_frame) {}));

  // Destroy the encoder, and confirm the VEA Factory did not respond yet.
  DestroyEncoder();
  ExpectVEAResponseForExternalVideoEncoder(0);

  // Allow the VEA Factory to respond by running the creation callback.  When
  // the task runs, it will be a no-op since the weak pointers to the
  // ExternalVideoEncoder were invalidated.
  SetVEAFactoryAutoRespond(true);
  RunTasksAndAdvanceClock();
  ExpectVEAResponseForExternalVideoEncoder(1);
}

namespace {
std::vector<std::pair<Codec, bool>> DetermineEncodersToTest() {
  std::vector<std::pair<Codec, bool>> values;
  // Fake encoder.
  values.push_back(std::make_pair(CODEC_VIDEO_FAKE, false));
  // Software VP8 encoder.
  values.push_back(std::make_pair(CODEC_VIDEO_VP8, false));
  // Hardware-accelerated encoder (faked).
  values.push_back(std::make_pair(CODEC_VIDEO_VP8, true));
#if defined(OS_MAC)
  // VideoToolbox encoder (when VideoToolbox is present).
  FrameSenderConfig video_config = GetDefaultVideoSenderConfig();
  video_config.use_external_encoder = false;
  video_config.codec = CODEC_VIDEO_H264;
  if (H264VideoToolboxEncoder::IsSupported(video_config))
    values.push_back(std::make_pair(CODEC_VIDEO_H264, false));
#endif
  return values;
}
}  // namespace

INSTANTIATE_TEST_SUITE_P(All,
                         VideoEncoderTest,
                         ::testing::ValuesIn(DetermineEncodersToTest()));

}  // namespace cast
}  // namespace media
