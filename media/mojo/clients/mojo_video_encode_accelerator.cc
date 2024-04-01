// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/clients/mojo_video_encode_accelerator.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "media/base/video_frame.h"
#include "media/gpu/gpu_video_accelerator_util.h"
#include "media/mojo/common/mojo_shared_buffer_video_frame.h"
#include "media/mojo/mojom/video_encoder_info.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace media {

namespace {

// File-static mojom::VideoEncodeAcceleratorClient implementation to trampoline
// method calls to its |client_|. Note that this class is thread hostile when
// bound.
class VideoEncodeAcceleratorClient
    : public mojom::VideoEncodeAcceleratorClient {
 public:
  VideoEncodeAcceleratorClient(
      VideoEncodeAccelerator::Client* client,
      mojo::PendingReceiver<mojom::VideoEncodeAcceleratorClient> receiver);

  VideoEncodeAcceleratorClient(const VideoEncodeAcceleratorClient&) = delete;
  VideoEncodeAcceleratorClient& operator=(const VideoEncodeAcceleratorClient&) =
      delete;

  ~VideoEncodeAcceleratorClient() override = default;

  // mojom::VideoEncodeAcceleratorClient impl.
  void RequireBitstreamBuffers(uint32_t input_count,
                               const gfx::Size& input_coded_size,
                               uint32_t output_buffer_size) override;
  void BitstreamBufferReady(
      int32_t bitstream_buffer_id,
      const media::BitstreamBufferMetadata& metadata) override;
  void NotifyError(VideoEncodeAccelerator::Error error) override;
  void NotifyEncoderInfoChange(const VideoEncoderInfo& info) override;

 private:
  VideoEncodeAccelerator::Client* client_;
  mojo::Receiver<mojom::VideoEncodeAcceleratorClient> receiver_;
};

VideoEncodeAcceleratorClient::VideoEncodeAcceleratorClient(
    VideoEncodeAccelerator::Client* client,
    mojo::PendingReceiver<mojom::VideoEncodeAcceleratorClient> receiver)
    : client_(client), receiver_(this, std::move(receiver)) {
  DCHECK(client_);
}

void VideoEncodeAcceleratorClient::RequireBitstreamBuffers(
    uint32_t input_count,
    const gfx::Size& input_coded_size,
    uint32_t output_buffer_size) {
  DVLOG(2) << __func__ << " input_count= " << input_count
           << " input_coded_size= " << input_coded_size.ToString()
           << " output_buffer_size=" << output_buffer_size;
  client_->RequireBitstreamBuffers(input_count, input_coded_size,
                                   output_buffer_size);
}

void VideoEncodeAcceleratorClient::BitstreamBufferReady(
    int32_t bitstream_buffer_id,
    const media::BitstreamBufferMetadata& metadata) {
  DVLOG(2) << __func__ << " bitstream_buffer_id=" << bitstream_buffer_id
           << ", payload_size=" << metadata.payload_size_bytes
           << "B,  key_frame=" << metadata.key_frame;
  client_->BitstreamBufferReady(bitstream_buffer_id, metadata);
}

void VideoEncodeAcceleratorClient::NotifyError(
    VideoEncodeAccelerator::Error error) {
  DVLOG(2) << __func__;
  client_->NotifyError(error);
}

void VideoEncodeAcceleratorClient::NotifyEncoderInfoChange(
    const VideoEncoderInfo& info) {
  DVLOG(2) << __func__;
  client_->NotifyEncoderInfoChange(info);
}

}  // anonymous namespace

MojoVideoEncodeAccelerator::MojoVideoEncodeAccelerator(
    mojo::PendingRemote<mojom::VideoEncodeAccelerator> vea,
    const SupportedProfiles& supported_profiles)
    : vea_(std::move(vea)), supported_profiles_(supported_profiles) {
  DVLOG(1) << __func__;
  DCHECK(vea_);
}

VideoEncodeAccelerator::SupportedProfiles
MojoVideoEncodeAccelerator::GetSupportedProfiles() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return supported_profiles_;
}

bool MojoVideoEncodeAccelerator::Initialize(const Config& config,
                                            Client* client) {
  DVLOG(2) << __func__ << " " << config.AsHumanReadableString();
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!client)
    return false;

  // Get a mojom::VideoEncodeAcceleratorClient bound to a local implementation
  // (VideoEncodeAcceleratorClient) and send the remote.
  mojo::PendingRemote<mojom::VideoEncodeAcceleratorClient> vea_client_remote;
  vea_client_ = std::make_unique<VideoEncodeAcceleratorClient>(
      client, vea_client_remote.InitWithNewPipeAndPassReceiver());

  bool result = false;
  vea_->Initialize(config, std::move(vea_client_remote), &result);
  return result;
}

void MojoVideoEncodeAccelerator::Encode(scoped_refptr<VideoFrame> frame,
                                        bool force_keyframe) {
  DVLOG(2) << __func__ << " tstamp=" << frame->timestamp();
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  size_t num_planes = VideoFrame::NumPlanes(frame->format());
  DCHECK_EQ(num_planes, frame->layout().num_planes());
  DCHECK(vea_.is_bound());

  // GPU memory path: Pass-through.
  if (frame->storage_type() == VideoFrame::STORAGE_GPU_MEMORY_BUFFER) {
    vea_->Encode(frame, force_keyframe,
                 base::BindOnce([](scoped_refptr<VideoFrame>) {}, frame));
    return;
  }

  // Mappable memory path: Copy buffer to shared memory.
  if (frame->format() != PIXEL_FORMAT_I420 &&
      frame->format() != PIXEL_FORMAT_NV12) {
    DLOG(ERROR) << "Unexpected pixel format: "
                << VideoPixelFormatToString(frame->format());
    return;
  }
  if (frame->storage_type() != VideoFrame::STORAGE_SHMEM &&
      frame->storage_type() != VideoFrame::STORAGE_UNOWNED_MEMORY) {
    DLOG(ERROR) << "Unexpected storage type: "
                << VideoFrame::StorageTypeToString(frame->storage_type());
    return;
  }

  scoped_refptr<MojoSharedBufferVideoFrame> mojo_frame =
      MojoSharedBufferVideoFrame::CreateFromYUVFrame(*frame);
  if (!mojo_frame) {
    DLOG(ERROR) << "Failed creating MojoSharedBufferVideoFrame from YUV";
    return;
  }
  vea_->Encode(
      std::move(mojo_frame), force_keyframe,
      base::BindOnce([](scoped_refptr<VideoFrame>) {}, std::move(frame)));
}

void MojoVideoEncodeAccelerator::UseOutputBitstreamBuffer(
    BitstreamBuffer buffer) {
  DVLOG(2) << __func__ << " buffer.id()= " << buffer.id()
           << " buffer.size()= " << buffer.size() << "B";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(buffer.region().IsValid());

  auto buffer_handle =
      mojo::WrapPlatformSharedMemoryRegion(buffer.TakeRegion());

  vea_->UseOutputBitstreamBuffer(buffer.id(), std::move(buffer_handle));
}

void MojoVideoEncodeAccelerator::RequestEncodingParametersChange(
    const Bitrate& bitrate,
    uint32_t framerate) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(vea_.is_bound());

  vea_->RequestEncodingParametersChangeWithBitrate(bitrate, framerate);
}

void MojoVideoEncodeAccelerator::RequestEncodingParametersChange(
    const VideoBitrateAllocation& bitrate,
    uint32_t framerate) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(vea_.is_bound());

  vea_->RequestEncodingParametersChangeWithLayers(bitrate, framerate);
}

bool MojoVideoEncodeAccelerator::IsFlushSupported() {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(vea_.is_bound());

  bool flush_support = false;
  vea_->IsFlushSupported(&flush_support);
  return flush_support;
}

void MojoVideoEncodeAccelerator::Flush(FlushCallback flush_callback) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(vea_.is_bound());

  vea_->Flush(std::move(flush_callback));
}

void MojoVideoEncodeAccelerator::Destroy() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  vea_client_.reset();
  vea_.reset();
  // See media::VideoEncodeAccelerator for more info on this peculiar pattern.
  delete this;
}

MojoVideoEncodeAccelerator::~MojoVideoEncodeAccelerator() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

}  // namespace media
