// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/openh264_video_encoder.h"

#include <algorithm>
#include <limits>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/svc_scalability_mode.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"

namespace media {

namespace {

Status SetUpOpenH264Params(const VideoEncoder::Options& options,
                           SEncParamExt* params) {
  params->bEnableFrameSkip = false;
  params->iPaddingFlag = 0;
  params->iComplexityMode = MEDIUM_COMPLEXITY;
  params->iUsageType = CAMERA_VIDEO_REAL_TIME;
  params->bEnableDenoise = false;
  // Set to 1 due to https://crbug.com/583348
  params->iMultipleThreadIdc = 1;
  if (options.framerate.has_value())
    params->fMaxFrameRate = options.framerate.value();
  params->iPicHeight = options.frame_size.height();
  params->iPicWidth = options.frame_size.width();

  if (options.keyframe_interval.has_value())
    params->uiIntraPeriod = options.keyframe_interval.value();

  if (options.bitrate.has_value()) {
    auto& bitrate = options.bitrate.value();
    params->iRCMode = RC_BITRATE_MODE;
    params->iTargetBitrate = base::saturated_cast<int>(bitrate.target());
  } else {
    params->iRCMode = RC_OFF_MODE;
  }

  int num_temporal_layers = 1;
  if (options.scalability_mode) {
    switch (options.scalability_mode.value()) {
      case SVCScalabilityMode::kL1T2:
        num_temporal_layers = 2;
        break;
      case SVCScalabilityMode::kL1T3:
        num_temporal_layers = 3;
        break;
      default:
        NOTREACHED() << "Unsupported SVC: "
                     << GetScalabilityModeName(
                            options.scalability_mode.value());
    }
  }

  params->iTemporalLayerNum = num_temporal_layers;
  params->iSpatialLayerNum = 1;
  params->sSpatialLayers[0].fFrameRate = params->fMaxFrameRate;
  params->sSpatialLayers[0].iMaxSpatialBitrate = params->iTargetBitrate;
  params->sSpatialLayers[0].iSpatialBitrate = params->iTargetBitrate;
  params->sSpatialLayers[0].iVideoHeight = params->iPicHeight;
  params->sSpatialLayers[0].iVideoWidth = params->iPicWidth;
  params->sSpatialLayers[0].sSliceArgument.uiSliceMode = SM_SINGLE_SLICE;

  return Status();
}
}  // namespace

OpenH264VideoEncoder::ISVCEncoderDeleter::ISVCEncoderDeleter() = default;
OpenH264VideoEncoder::ISVCEncoderDeleter::ISVCEncoderDeleter(
    const ISVCEncoderDeleter&) = default;
OpenH264VideoEncoder::ISVCEncoderDeleter&
OpenH264VideoEncoder::ISVCEncoderDeleter::operator=(const ISVCEncoderDeleter&) =
    default;
void OpenH264VideoEncoder::ISVCEncoderDeleter::operator()(ISVCEncoder* codec) {
  if (codec) {
    if (initialized_) {
      auto result = codec->Uninitialize();
      DCHECK_EQ(cmResultSuccess, result);
    }
    WelsDestroySVCEncoder(codec);
  }
}
void OpenH264VideoEncoder::ISVCEncoderDeleter::MarkInitialized() {
  initialized_ = true;
}

OpenH264VideoEncoder::OpenH264VideoEncoder() : codec_() {}
OpenH264VideoEncoder::~OpenH264VideoEncoder() = default;

void OpenH264VideoEncoder::Initialize(VideoCodecProfile profile,
                                      const Options& options,
                                      OutputCB output_cb,
                                      StatusCB done_cb) {
  done_cb = BindToCurrentLoop(std::move(done_cb));
  if (codec_) {
    std::move(done_cb).Run(StatusCode::kEncoderInitializeTwice);
    return;
  }

  profile_ = profile;
  if (profile != H264PROFILE_BASELINE) {
    auto status =
        Status(StatusCode::kEncoderInitializationError, "Unsupported profile");
    std::move(done_cb).Run(status);
    return;
  }

  ISVCEncoder* raw_codec = nullptr;
  if (WelsCreateSVCEncoder(&raw_codec) != 0) {
    auto status = Status(StatusCode::kEncoderInitializationError,
                         "Failed to create OpenH264 encoder.");
    std::move(done_cb).Run(status);
    return;
  }
  svc_encoder_unique_ptr codec(raw_codec, ISVCEncoderDeleter());
  raw_codec = nullptr;

  Status status;

  SEncParamExt params = {};
  if (int err = codec->GetDefaultParams(&params)) {
    status = Status(StatusCode::kEncoderInitializationError,
                    "Failed to get default params.")
                 .WithData("error", err);
    std::move(done_cb).Run(status);
    return;
  }

  status = SetUpOpenH264Params(options, &params);
  if (!status.is_ok()) {
    std::move(done_cb).Run(status);
    return;
  }

  if (int err = codec->InitializeExt(&params)) {
    status = Status(StatusCode::kEncoderInitializationError,
                    "Failed to initialize OpenH264 encoder.")
                 .WithData("error", err);
    std::move(done_cb).Run(status);
    return;
  }
  codec.get_deleter().MarkInitialized();

  int video_format = EVideoFormatType::videoFormatI420;
  if (int err = codec->SetOption(ENCODER_OPTION_DATAFORMAT, &video_format)) {
    status = Status(StatusCode::kEncoderInitializationError,
                    "Failed to set data format for OpenH264 encoder")
                 .WithData("error", err);
    std::move(done_cb).Run(status);
    return;
  }

  if (!options.avc.produce_annexb)
    h264_converter_ = std::make_unique<H264AnnexBToAvcBitstreamConverter>();

  options_ = options;
  output_cb_ = BindToCurrentLoop(std::move(output_cb));
  codec_ = std::move(codec);
  std::move(done_cb).Run(OkStatus());
}

Status OpenH264VideoEncoder::DrainOutputs(const SFrameBSInfo& frame_info,
                                          base::TimeDelta timestamp) {
  VideoEncoderOutput result;
  result.key_frame = (frame_info.eFrameType == videoFrameTypeIDR);
  result.timestamp = timestamp;

  DCHECK_GT(frame_info.iFrameSizeInBytes, 0);
  size_t total_chunk_size = frame_info.iFrameSizeInBytes;
  result.data.reset(new uint8_t[total_chunk_size]);
  auto* gather_buffer = result.data.get();

  if (h264_converter_) {
    // Copy data to a temporary buffer instead.
    conversion_buffer_.resize(total_chunk_size);
    gather_buffer = conversion_buffer_.data();
  }

  result.temporal_id = -1;
  size_t written_size = 0;
  for (int layer_idx = 0; layer_idx < frame_info.iLayerNum; ++layer_idx) {
    const SLayerBSInfo& layer_info = frame_info.sLayerInfo[layer_idx];

    // All layers in the same frame must have the same temporal_id.
    if (result.temporal_id == -1)
      result.temporal_id = layer_info.uiTemporalId;
    else if (result.temporal_id != layer_info.uiTemporalId)
      return Status(StatusCode::kEncoderFailedEncode);

    size_t layer_len = 0;
    for (int nal_idx = 0; nal_idx < layer_info.iNalCount; ++nal_idx)
      layer_len += layer_info.pNalLengthInByte[nal_idx];
    if (written_size + layer_len > total_chunk_size)
      return Status(StatusCode::kEncoderFailedEncode);

    memcpy(gather_buffer + written_size, layer_info.pBsBuf, layer_len);
    written_size += layer_len;
  }
  DCHECK_EQ(written_size, total_chunk_size);

  if (!h264_converter_) {
    result.size = total_chunk_size;

    output_cb_.Run(std::move(result), absl::optional<CodecDescription>());
    return OkStatus();
  }

  size_t converted_output_size = 0;
  bool config_changed = false;
  auto status = h264_converter_->ConvertChunk(
      conversion_buffer_,
      base::span<uint8_t>(result.data.get(), total_chunk_size), &config_changed,
      &converted_output_size);

  if (!status.is_ok())
    return status;

  result.size = converted_output_size;

  absl::optional<CodecDescription> desc;
  if (config_changed) {
    const auto& config = h264_converter_->GetCurrentConfig();
    desc = CodecDescription();
    if (!config.Serialize(desc.value())) {
      return Status(StatusCode::kEncoderFailedEncode,
                    "Failed to serialize AVC decoder config");
    }
  }

  output_cb_.Run(std::move(result), std::move(desc));
  return OkStatus();
}

void OpenH264VideoEncoder::Encode(scoped_refptr<VideoFrame> frame,
                                  bool key_frame,
                                  StatusCB done_cb) {
  Status status;
  done_cb = BindToCurrentLoop(std::move(done_cb));
  if (!codec_) {
    std::move(done_cb).Run(StatusCode::kEncoderInitializeNeverCompleted);
    return;
  }

  if (!frame) {
    std::move(done_cb).Run(Status(StatusCode::kEncoderFailedEncode,
                                  "No frame provided for encoding."));
    return;
  }
  const bool supported_format = frame->format() == PIXEL_FORMAT_NV12 ||
                                frame->format() == PIXEL_FORMAT_I420 ||
                                frame->format() == PIXEL_FORMAT_XBGR ||
                                frame->format() == PIXEL_FORMAT_XRGB ||
                                frame->format() == PIXEL_FORMAT_ABGR ||
                                frame->format() == PIXEL_FORMAT_ARGB;
  if ((!frame->IsMappable() && !frame->HasGpuMemoryBuffer()) ||
      !supported_format) {
    status =
        Status(StatusCode::kEncoderFailedEncode, "Unexpected frame format.")
            .WithData("IsMappable", frame->IsMappable())
            .WithData("format", frame->format());
    std::move(done_cb).Run(std::move(status));
    return;
  }

  if (frame->format() == PIXEL_FORMAT_NV12 && frame->HasGpuMemoryBuffer()) {
    frame = ConvertToMemoryMappedFrame(frame);
    if (!frame) {
      std::move(done_cb).Run(
          Status(StatusCode::kEncoderFailedEncode,
                 "Convert GMB frame to MemoryMappedFrame failed."));
      return;
    }
  }

  if (frame->format() != PIXEL_FORMAT_I420) {
    // OpenH264 can resize frame automatically, but since we're converting
    // pixel fromat anyway we can do resize as well.
    auto i420_frame = frame_pool_.CreateFrame(
        PIXEL_FORMAT_I420, options_.frame_size, gfx::Rect(options_.frame_size),
        options_.frame_size, frame->timestamp());
    if (i420_frame) {
      status = ConvertAndScaleFrame(*frame, *i420_frame, conversion_buffer_);
    } else {
      status = Status(StatusCode::kEncoderFailedEncode,
                      "Can't allocate an I420 frame.");
    }
    if (!status.is_ok()) {
      std::move(done_cb).Run(std::move(status));
      return;
    }
    frame = std::move(i420_frame);
  }

  SSourcePicture picture = {};
  picture.iPicWidth = frame->visible_rect().width();
  picture.iPicHeight = frame->visible_rect().height();
  picture.iColorFormat = EVideoFormatType::videoFormatI420;
  picture.uiTimeStamp = frame->timestamp().InMilliseconds();
  picture.pData[0] = frame->visible_data(VideoFrame::kYPlane);
  picture.pData[1] = frame->visible_data(VideoFrame::kUPlane);
  picture.pData[2] = frame->visible_data(VideoFrame::kVPlane);
  picture.iStride[0] = frame->stride(VideoFrame::kYPlane);
  picture.iStride[1] = frame->stride(VideoFrame::kUPlane);
  picture.iStride[2] = frame->stride(VideoFrame::kVPlane);

  if (key_frame) {
    if (int err = codec_->ForceIntraFrame(true)) {
      std::move(done_cb).Run(
          Status(StatusCode::kEncoderFailedEncode, "Can't make keyframe.")
              .WithData("error", err));
      return;
    }
  }

  SFrameBSInfo frame_info = {};
  TRACE_EVENT0("media", "OpenH264::EncodeFrame");
  if (int err = codec_->EncodeFrame(&picture, &frame_info)) {
    std::move(done_cb).Run(Status(StatusCode::kEncoderFailedEncode,
                                  "Failed to encode using OpenH264.")
                               .WithData("error", err));
    return;
  }

  status = DrainOutputs(frame_info, frame->timestamp());
  std::move(done_cb).Run(std::move(status));
}

void OpenH264VideoEncoder::ChangeOptions(const Options& options,
                                         OutputCB output_cb,
                                         StatusCB done_cb) {
  done_cb = BindToCurrentLoop(std::move(done_cb));
  if (!codec_) {
    std::move(done_cb).Run(StatusCode::kEncoderInitializeNeverCompleted);
    return;
  }

  Status status;
  SEncParamExt params = {};
  if (int err = codec_->GetDefaultParams(&params)) {
    status = Status(StatusCode::kEncoderInitializationError,
                    "Failed to get default params.")
                 .WithData("error", err);
    std::move(done_cb).Run(status);
    return;
  }

  status = SetUpOpenH264Params(options, &params);
  if (!status.is_ok()) {
    std::move(done_cb).Run(status);
    return;
  }

  if (int err =
          codec_->SetOption(ENCODER_OPTION_SVC_ENCODE_PARAM_EXT, &params)) {
    status = Status(StatusCode::kEncoderInitializationError,
                    "OpenH264 encoder failed to set new SEncParamExt.")
                 .WithData("error", err);
    std::move(done_cb).Run(status);
    return;
  }

  if (options.avc.produce_annexb) {
    h264_converter_.reset();
  } else if (!h264_converter_) {
    h264_converter_ = std::make_unique<H264AnnexBToAvcBitstreamConverter>();
  }

  if (!output_cb.is_null())
    output_cb_ = BindToCurrentLoop(std::move(output_cb));
  std::move(done_cb).Run(OkStatus());
}

void OpenH264VideoEncoder::Flush(StatusCB done_cb) {
  done_cb = BindToCurrentLoop(std::move(done_cb));
  if (!codec_) {
    std::move(done_cb).Run(StatusCode::kEncoderInitializeNeverCompleted);
    return;
  }

  // Nothing to do really.
  std::move(done_cb).Run(OkStatus());
}

}  // namespace media
