// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_player/video_decoder_client.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "media/base/media_util.h"
#include "media/base/waiting.h"
#include "media/gpu/macros.h"
#include "media/gpu/test/video.h"
#include "media/gpu/test/video_player/frame_renderer.h"
#include "media/gpu/test/video_player/test_vda_video_decoder.h"
#include "media/gpu/test/video_test_helpers.h"
#include "media/media_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
#include "media/gpu/chromeos/platform_video_frame_pool.h"
#include "media/gpu/chromeos/video_decoder_pipeline.h"
#include "media/gpu/chromeos/video_frame_converter.h"
#endif  // BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)

namespace media {
namespace test {

namespace {
// Callbacks can be called from any thread, but WeakPtrs are not thread-safe.
// This helper thunk wraps a WeakPtr into an 'Optional' value, so the WeakPtr is
// only dereferenced after rescheduling the task on the specified task runner.
template <typename F, typename... Args>
void CallbackThunk(
    absl::optional<base::WeakPtr<VideoDecoderClient>> decoder_client,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    F f,
    Args... args) {
  DCHECK(decoder_client);
  task_runner->PostTask(FROM_HERE, base::BindOnce(f, *decoder_client, args...));
}
}  // namespace

VideoDecoderClient::VideoDecoderClient(
    const VideoPlayer::EventCallback& event_cb,
    gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory,
    std::unique_ptr<FrameRenderer> renderer,
    std::vector<std::unique_ptr<VideoFrameProcessor>> frame_processors,
    const VideoDecoderClientConfig& config)
    : event_cb_(event_cb),
      frame_renderer_(std::move(renderer)),
      frame_processors_(std::move(frame_processors)),
      decoder_client_config_(config),
      decoder_client_thread_("VDAClientDecoderThread"),
      decoder_client_state_(VideoDecoderClientState::kUninitialized),
      gpu_memory_buffer_factory_(gpu_memory_buffer_factory) {
  DETACH_FROM_SEQUENCE(decoder_client_sequence_checker_);

  weak_this_ = weak_this_factory_.GetWeakPtr();
}

VideoDecoderClient::~VideoDecoderClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(video_player_sequence_checker_);

  DestroyDecoder();

  // Wait until the renderer and frame processors are done before destroying
  // them. This needs to be done after destroying the decoder so no new frames
  // will be queued while waiting.
  WaitForRenderer();
  WaitForFrameProcessors();
  frame_renderer_ = nullptr;
  frame_processors_.clear();

  decoder_client_thread_.Stop();
}

// static
std::unique_ptr<VideoDecoderClient> VideoDecoderClient::Create(
    const VideoPlayer::EventCallback& event_cb,
    gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory,
    std::unique_ptr<FrameRenderer> frame_renderer,
    std::vector<std::unique_ptr<VideoFrameProcessor>> frame_processors,
    const VideoDecoderClientConfig& config) {
  auto decoder_client = base::WrapUnique(new VideoDecoderClient(
      event_cb, gpu_memory_buffer_factory, std::move(frame_renderer),
      std::move(frame_processors), config));
  if (!decoder_client->CreateDecoder()) {
    return nullptr;
  }
  return decoder_client;
}

bool VideoDecoderClient::CreateDecoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(video_player_sequence_checker_);
  DCHECK(!decoder_client_thread_.IsRunning());
  DCHECK(event_cb_ && frame_renderer_);

  if (!decoder_client_thread_.Start()) {
    VLOGF(1) << "Failed to start decoder thread";
    return false;
  }

  bool success = false;
  base::WaitableEvent done;
  decoder_client_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&VideoDecoderClient::CreateDecoderTask,
                                weak_this_, &success, &done));
  done.Wait();
  return success;
}

void VideoDecoderClient::DestroyDecoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(video_player_sequence_checker_);

  if (!decoder_client_thread_.IsRunning()) {
    return;
  }

  base::WaitableEvent done;
  decoder_client_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&VideoDecoderClient::DestroyDecoderTask,
                                weak_this_, &done));
  done.Wait();
}

bool VideoDecoderClient::WaitForFrameProcessors() {
  bool success = true;
  for (auto& frame_processor : frame_processors_)
    success &= frame_processor->WaitUntilDone();
  return success;
}

void VideoDecoderClient::WaitForRenderer() {
  ASSERT_TRUE(frame_renderer_);
  frame_renderer_->WaitUntilRenderingDone();
}

FrameRenderer* VideoDecoderClient::GetFrameRenderer() const {
  return frame_renderer_.get();
}

void VideoDecoderClient::Initialize(const Video* video) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(video_player_sequence_checker_);
  DCHECK(video);

  base::WaitableEvent done;
  decoder_client_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&VideoDecoderClient::InitializeDecoderTask,
                                weak_this_, video, &done));
  done.Wait();
}

void VideoDecoderClient::Play() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(video_player_sequence_checker_);

  decoder_client_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&VideoDecoderClient::PlayTask, weak_this_));
}

void VideoDecoderClient::Flush() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(video_player_sequence_checker_);

  decoder_client_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&VideoDecoderClient::FlushTask, weak_this_));
}

void VideoDecoderClient::Reset() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(video_player_sequence_checker_);

  decoder_client_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&VideoDecoderClient::ResetTask, weak_this_));
}

void VideoDecoderClient::CreateDecoderTask(bool* success,
                                           base::WaitableEvent* done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_client_sequence_checker_);
  DCHECK_EQ(decoder_client_state_, VideoDecoderClientState::kUninitialized);
  ASSERT_TRUE(!decoder_) << "Can't create decoder: already created";

  switch (decoder_client_config_.implementation) {
    case DecoderImplementation::kVD:
#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
      decoder_ = VideoDecoderPipeline::Create(
          base::ThreadTaskRunnerHandle::Get(),
          std::make_unique<PlatformVideoFramePool>(gpu_memory_buffer_factory_),
          std::make_unique<VideoFrameConverter>(),
          std::make_unique<NullMediaLog>());
#endif  // BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
      break;
    case DecoderImplementation::kVDA:
    case DecoderImplementation::kVDVDA:
      // The video decoder client expects decoders to use the VD interface. We
      // can use the TestVDAVideoDecoder wrapper here to test VDA-based video
      // decoders.
      decoder_ = std::make_unique<TestVDAVideoDecoder>(
          decoder_client_config_.implementation ==
              DecoderImplementation::kVDVDA,
          gfx::ColorSpace(), frame_renderer_.get(), gpu_memory_buffer_factory_);
      break;
  }

  *success = (decoder_ != nullptr);
  done->Signal();
}

void VideoDecoderClient::InitializeDecoderTask(const Video* video,
                                               base::WaitableEvent* done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_client_sequence_checker_);
  DCHECK(decoder_client_state_ == VideoDecoderClientState::kUninitialized ||
         decoder_client_state_ == VideoDecoderClientState::kIdle);
  ASSERT_TRUE(decoder_) << "Can't initialize decoder: not created yet";
  ASSERT_TRUE(video);

  video_ = video;
  encoded_data_helper_ =
      std::make_unique<EncodedDataHelper>(video_->Data(), video_->Profile());

  // (Re-)initialize the decoder.
  VideoDecoderConfig config(
      video_->Codec(), video_->Profile(),
      VideoDecoderConfig::AlphaMode::kIsOpaque, VideoColorSpace(),
      kNoTransformation, video_->Resolution(), gfx::Rect(video_->Resolution()),
      video_->Resolution(), std::vector<uint8_t>(0), EncryptionScheme());

  VideoDecoder::InitCB init_cb = base::BindOnce(
      CallbackThunk<decltype(&VideoDecoderClient::DecoderInitializedTask),
                    Status>,
      weak_this_, decoder_client_thread_.task_runner(),
      &VideoDecoderClient::DecoderInitializedTask);
  VideoDecoder::OutputCB output_cb = base::BindRepeating(
      CallbackThunk<decltype(&VideoDecoderClient::FrameReadyTask),
                    scoped_refptr<VideoFrame>>,
      weak_this_, decoder_client_thread_.task_runner(),
      &VideoDecoderClient::FrameReadyTask);

  decoder_->Initialize(config, false, nullptr, std::move(init_cb), output_cb,
                       WaitingCB());

  DCHECK_LE(decoder_client_config_.max_outstanding_decode_requests,
            static_cast<size_t>(decoder_->GetMaxDecodeRequests()));

  done->Signal();
}

void VideoDecoderClient::DestroyDecoderTask(base::WaitableEvent* done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_client_sequence_checker_);
  DCHECK_EQ(0u, num_outstanding_decode_requests_);
  DVLOGF(4);

  // Invalidate all scheduled tasks.
  weak_this_factory_.InvalidateWeakPtrs();

  // Destroy the decoder. This will destroy all video frames.
  if (decoder_) {
    decoder_.reset();
  }

  decoder_client_state_ = VideoDecoderClientState::kUninitialized;
  done->Signal();
}

void VideoDecoderClient::PlayTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_client_sequence_checker_);
  DVLOGF(4);

  // This method should only be called when the decoder client is idle. If
  // called e.g. while flushing, the behavior is undefined.
  ASSERT_EQ(decoder_client_state_, VideoDecoderClientState::kIdle);

  // Start decoding the first fragments. While in the decoding state new
  // fragments will automatically be fed to the decoder, when the decoder
  // notifies us it reached the end of a bitstream buffer.
  decoder_client_state_ = VideoDecoderClientState::kDecoding;
  for (size_t i = 0; i < decoder_client_config_.max_outstanding_decode_requests;
       ++i) {
    DecodeNextFragmentTask();
  }
}

void VideoDecoderClient::DecodeNextFragmentTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_client_sequence_checker_);
  DVLOGF(4);

  // Stop decoding fragments if we're no longer in the decoding state.
  if (decoder_client_state_ != VideoDecoderClientState::kDecoding)
    return;

  // Flush immediately when we reached the end of the stream. This changes the
  // state to kFlushing so further decode tasks will be aborted.
  if (encoded_data_helper_->ReachEndOfStream()) {
    FlushTask();
    return;
  }

  scoped_refptr<DecoderBuffer> bitstream_buffer =
      encoded_data_helper_->GetNextBuffer();
  if (!bitstream_buffer) {
    LOG(ERROR) << "Failed to get next video stream data";
    return;
  }
  bitstream_buffer->set_timestamp(base::TimeTicks::Now().since_origin());
  bool has_config_info = media::test::EncodedDataHelper::HasConfigInfo(
      bitstream_buffer->data(), bitstream_buffer->data_size(),
      video_->Profile());

  VideoDecoder::DecodeCB decode_cb = base::BindOnce(
      CallbackThunk<decltype(&VideoDecoderClient::DecodeDoneTask),
                    media::Status>,
      weak_this_, decoder_client_thread_.task_runner(),
      &VideoDecoderClient::DecodeDoneTask);
  decoder_->Decode(std::move(bitstream_buffer), std::move(decode_cb));

  num_outstanding_decode_requests_++;

  // Throw event when we encounter a config info in a H.264/HEVC stream.
  if (has_config_info)
    FireEvent(VideoPlayerEvent::kConfigInfo);
}

void VideoDecoderClient::FlushTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_client_sequence_checker_);
  DVLOGF(4);

  // Changing the state to flushing will abort any pending decodes.
  decoder_client_state_ = VideoDecoderClientState::kFlushing;

  VideoDecoder::DecodeCB flush_done_cb =
      base::BindOnce(CallbackThunk<decltype(&VideoDecoderClient::FlushDoneTask),
                                   media::Status>,
                     weak_this_, decoder_client_thread_.task_runner(),
                     &VideoDecoderClient::FlushDoneTask);
  decoder_->Decode(DecoderBuffer::CreateEOSBuffer(), std::move(flush_done_cb));

  FireEvent(VideoPlayerEvent::kFlushing);
}

void VideoDecoderClient::ResetTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_client_sequence_checker_);
  DVLOGF(4);

  // Changing the state to resetting will abort any pending decodes.
  decoder_client_state_ = VideoDecoderClientState::kResetting;
  // TODO(dstaessens@) Allow resetting to any point in the stream.
  encoded_data_helper_->Rewind();

  decoder_->Reset(base::BindOnce(
      CallbackThunk<decltype(&VideoDecoderClient::ResetDoneTask)>, weak_this_,
      decoder_client_thread_.task_runner(),
      &VideoDecoderClient::ResetDoneTask));
  FireEvent(VideoPlayerEvent::kResetting);
}

void VideoDecoderClient::DecoderInitializedTask(Status status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_client_sequence_checker_);
  DCHECK(decoder_client_state_ == VideoDecoderClientState::kUninitialized ||
         decoder_client_state_ == VideoDecoderClientState::kIdle);
  ASSERT_TRUE(status.is_ok()) << "Initializing decoder failed";

  decoder_client_state_ = VideoDecoderClientState::kIdle;
  FireEvent(VideoPlayerEvent::kInitialized);
}

void VideoDecoderClient::DecodeDoneTask(media::Status status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_client_sequence_checker_);
  DCHECK_NE(VideoDecoderClientState::kIdle, decoder_client_state_);
  ASSERT_TRUE(status.code() != media::StatusCode::kAborted ||
              decoder_client_state_ == VideoDecoderClientState::kResetting);
  DVLOGF(4);

  num_outstanding_decode_requests_--;

  // Queue the next fragment to be decoded.
  decoder_client_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoDecoderClient::DecodeNextFragmentTask, weak_this_));
}

void VideoDecoderClient::FrameReadyTask(scoped_refptr<VideoFrame> video_frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_client_sequence_checker_);
  DCHECK(video_frame->metadata().power_efficient);

  frame_renderer_->RenderFrame(video_frame);

  for (auto& frame_processor : frame_processors_)
    frame_processor->ProcessVideoFrame(video_frame, current_frame_index_);

  // Notify the test a frame has been decoded. We should only do this after
  // scheduling the frame to be processed, so calling WaitForFrameProcessors()
  // after receiving this event will always guarantee the frame to be processed.
  FireEvent(VideoPlayerEvent::kFrameDecoded);

  current_frame_index_++;
}

void VideoDecoderClient::FlushDoneTask(media::Status status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_client_sequence_checker_);
  DCHECK_EQ(0u, num_outstanding_decode_requests_);

  // Send an EOS frame to the renderer, so it can reset any internal state it
  // might keep in preparation of the next stream of video frames.
  frame_renderer_->RenderFrame(VideoFrame::CreateEOSFrame());
  decoder_client_state_ = VideoDecoderClientState::kIdle;
  FireEvent(VideoPlayerEvent::kFlushDone);
}

void VideoDecoderClient::ResetDoneTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_client_sequence_checker_);
  DCHECK_EQ(0u, num_outstanding_decode_requests_);

  // We finished resetting to a different point in the stream, so we should
  // update the frame index. Currently only resetting to the start of the stream
  // is supported, so we can set the frame index to zero here.
  current_frame_index_ = 0;

  frame_renderer_->RenderFrame(VideoFrame::CreateEOSFrame());
  decoder_client_state_ = VideoDecoderClientState::kIdle;
  FireEvent(VideoPlayerEvent::kResetDone);
}

void VideoDecoderClient::FireEvent(VideoPlayerEvent event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_client_sequence_checker_);

  bool continue_decoding = event_cb_.Run(event);
  if (!continue_decoding) {
    // Changing the state to idle will abort any pending decodes.
    decoder_client_state_ = VideoDecoderClientState::kIdle;
  }
}

}  // namespace test
}  // namespace media
