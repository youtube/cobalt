// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/win/media_foundation_video_stream.h"

#include <initguid.h>  // NOLINT(build/include_order)
#include <mferror.h>   // NOLINT(build/include_order)
#include <wrl.h>       // NOLINT(build/include_order)

#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "media/base/video_codecs.h"
#include "media/base/video_decoder_config.h"
#include "media/base/win/mf_helpers.h"

namespace media {

using Microsoft::WRL::ComPtr;
using Microsoft::WRL::MakeAndInitialize;

namespace {

// This is supported by Media Foundation.
DEFINE_MEDIATYPE_GUID(MFVideoFormat_THEORA, FCC('theo'))

GUID VideoCodecToMFSubtype(VideoCodec codec) {
  switch (codec) {
    case VideoCodec::kH264:
      return MFVideoFormat_H264;
    case VideoCodec::kVC1:
      return MFVideoFormat_WVC1;
    case VideoCodec::kMPEG2:
      return MFVideoFormat_MPEG2;
    case VideoCodec::kMPEG4:
      return MFVideoFormat_MP4V;
    case VideoCodec::kTheora:
      return MFVideoFormat_THEORA;
    case VideoCodec::kVP8:
      return MFVideoFormat_VP80;
    case VideoCodec::kVP9:
      return MFVideoFormat_VP90;
    case VideoCodec::kHEVC:
      return MFVideoFormat_HEVC;
    case VideoCodec::kDolbyVision:
      // TODO(frankli): DolbyVision also supports H264 when the profile ID is 9
      // (DOLBYVISION_PROFILE9). Will it be fine to use HEVC?
      return MFVideoFormat_HEVC;
    case VideoCodec::kAV1:
      return MFVideoFormat_AV1;
    default:
      return GUID_NULL;
  }
}

MFVideoRotationFormat VideoRotationToMF(VideoRotation rotation) {
  DVLOG(2) << __func__ << ": rotation=" << rotation;

  switch (rotation) {
    case VideoRotation::VIDEO_ROTATION_0:
      return MFVideoRotationFormat_0;
    case VideoRotation::VIDEO_ROTATION_90:
      return MFVideoRotationFormat_90;
    case VideoRotation::VIDEO_ROTATION_180:
      return MFVideoRotationFormat_180;
    case VideoRotation::VIDEO_ROTATION_270:
      return MFVideoRotationFormat_270;
  }
}

MFVideoPrimaries VideoPrimariesToMF(
    media::VideoColorSpace::PrimaryID primary_id) {
  DVLOG(2) << __func__ << ": primary_id=" << static_cast<int>(primary_id);

  switch (primary_id) {
    case VideoColorSpace::PrimaryID::INVALID:
      return MFVideoPrimaries_Unknown;
    case VideoColorSpace::PrimaryID::BT709:
      return MFVideoPrimaries_BT709;
    case VideoColorSpace::PrimaryID::UNSPECIFIED:
      return MFVideoPrimaries_Unknown;
    case VideoColorSpace::PrimaryID::BT470M:
      return MFVideoPrimaries_BT470_2_SysM;
    case VideoColorSpace::PrimaryID::BT470BG:
      return MFVideoPrimaries_BT470_2_SysBG;
    case VideoColorSpace::PrimaryID::SMPTE170M:
      return MFVideoPrimaries_SMPTE170M;
    case VideoColorSpace::PrimaryID::SMPTE240M:
      return MFVideoPrimaries_SMPTE240M;
    case VideoColorSpace::PrimaryID::FILM:
      return MFVideoPrimaries_Unknown;
    case VideoColorSpace::PrimaryID::BT2020:
      return MFVideoPrimaries_BT2020;
    case VideoColorSpace::PrimaryID::SMPTEST428_1:
      return MFVideoPrimaries_Unknown;
    case VideoColorSpace::PrimaryID::SMPTEST431_2:
      return MFVideoPrimaries_Unknown;
    case VideoColorSpace::PrimaryID::SMPTEST432_1:
      return MFVideoPrimaries_Unknown;
    case VideoColorSpace::PrimaryID::EBU_3213_E:
      return MFVideoPrimaries_EBU3213;
    default:
      DLOG(ERROR) << "VideoPrimariesToMF failed due to invalid Video Primary.";
  }

  return MFVideoPrimaries_Unknown;
}

MFVideoTransferFunction VideoTransferFunctionToMF(
    media::VideoColorSpace::TransferID transfer_id) {
  DVLOG(2) << __func__ << ": transfer_id=" << static_cast<int>(transfer_id);

  switch (transfer_id) {
    case VideoColorSpace::TransferID::INVALID:
      return MFVideoTransFunc_Unknown;
    case VideoColorSpace::TransferID::BT709:
      return MFVideoTransFunc_709;
    case VideoColorSpace::TransferID::UNSPECIFIED:
      return MFVideoTransFunc_Unknown;
    case VideoColorSpace::TransferID::GAMMA22:
      return MFVideoTransFunc_22;
    case VideoColorSpace::TransferID::GAMMA28:
      return MFVideoTransFunc_28;
    case VideoColorSpace::TransferID::SMPTE170M:
      return MFVideoTransFunc_709;
    case VideoColorSpace::TransferID::SMPTE240M:
      return MFVideoTransFunc_240M;
    case VideoColorSpace::TransferID::LINEAR:
      return MFVideoTransFunc_10;
    case VideoColorSpace::TransferID::LOG:
      return MFVideoTransFunc_Log_100;
    case VideoColorSpace::TransferID::LOG_SQRT:
      return MFVideoTransFunc_Unknown;
    case VideoColorSpace::TransferID::IEC61966_2_4:
      return MFVideoTransFunc_Unknown;
    case VideoColorSpace::TransferID::BT1361_ECG:
      return MFVideoTransFunc_Unknown;
    case VideoColorSpace::TransferID::IEC61966_2_1:
      return MFVideoTransFunc_sRGB;
    case VideoColorSpace::TransferID::BT2020_10:
      return MFVideoTransFunc_2020;
    case VideoColorSpace::TransferID::BT2020_12:
      return MFVideoTransFunc_2020;
    case VideoColorSpace::TransferID::SMPTEST2084:
      return MFVideoTransFunc_2084;
    case VideoColorSpace::TransferID::SMPTEST428_1:
      return MFVideoTransFunc_Unknown;
    case VideoColorSpace::TransferID::ARIB_STD_B67:
      return MFVideoTransFunc_HLG;
    default:
      DLOG(ERROR) << "VideoTransferFunctionToMF failed due to invalid Transfer "
                     "Function.";
  }

  return MFVideoTransFunc_Unknown;
}

//
// https://docs.microsoft.com/en-us/windows/win32/api/mfobjects/ns-mfobjects-mfoffset
// The value of the MFOffset number is value + (fract / 65536.0f).
MFOffset MakeOffset(float value) {
  MFOffset offset;
  offset.value = base::checked_cast<short>(value);
  offset.fract = base::checked_cast<WORD>(65536 * (value - offset.value));
  return offset;
}

// TODO(frankli): investigate if VideoCodecProfile is needed by MF.
HRESULT GetVideoType(const VideoDecoderConfig& decoder_config,
                     IMFMediaType** media_type_out) {
  ComPtr<IMFMediaType> media_type;
  RETURN_IF_FAILED(MFCreateMediaType(&media_type));
  RETURN_IF_FAILED(media_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video));

  GUID mf_subtype = VideoCodecToMFSubtype(decoder_config.codec());
  if (mf_subtype == GUID_NULL) {
    DLOG(ERROR) << "Unsupported codec type: " << decoder_config.codec();
    return MF_E_TOPO_CODEC_NOT_FOUND;
  }
  RETURN_IF_FAILED(media_type->SetGUID(MF_MT_SUBTYPE, mf_subtype));

  UINT32 width = decoder_config.visible_rect().width();
  UINT32 height = decoder_config.visible_rect().height();
  RETURN_IF_FAILED(
      MFSetAttributeSize(media_type.Get(), MF_MT_FRAME_SIZE, width, height));

  UINT32 natural_width = decoder_config.natural_size().width();
  UINT32 natural_height = decoder_config.natural_size().height();
  RETURN_IF_FAILED(
      MFSetAttributeRatio(media_type.Get(), MF_MT_PIXEL_ASPECT_RATIO,
                          height * natural_width, width * natural_height));

  MFVideoArea area;
  area.OffsetX =
      MakeOffset(static_cast<float>(decoder_config.visible_rect().x()));
  area.OffsetY =
      MakeOffset(static_cast<float>(decoder_config.visible_rect().y()));
  area.Area = decoder_config.natural_size().ToSIZE();
  RETURN_IF_FAILED(media_type->SetBlob(MF_MT_GEOMETRIC_APERTURE, (UINT8*)&area,
                                       sizeof(area)));

  if (decoder_config.video_transformation().rotation !=
      VideoRotation::VIDEO_ROTATION_0) {
    MFVideoRotationFormat mf_rotation =
        VideoRotationToMF(decoder_config.video_transformation().rotation);
    RETURN_IF_FAILED(media_type->SetUINT32(MF_MT_VIDEO_ROTATION, mf_rotation));
  }

  MFVideoTransferFunction mf_transfer_function =
      VideoTransferFunctionToMF(decoder_config.color_space_info().transfer);
  RETURN_IF_FAILED(
      media_type->SetUINT32(MF_MT_TRANSFER_FUNCTION, mf_transfer_function));

  MFVideoPrimaries mf_video_primary =
      VideoPrimariesToMF(decoder_config.color_space_info().primaries);
  RETURN_IF_FAILED(
      media_type->SetUINT32(MF_MT_VIDEO_PRIMARIES, mf_video_primary));

  base::UmaHistogramEnumeration(
      "Media.MediaFoundation.VideoColorSpace.TransferID",
      decoder_config.color_space_info().transfer);
  base::UmaHistogramEnumeration(
      "Media.MediaFoundation.VideoColorSpace.PrimaryID",
      decoder_config.color_space_info().primaries);

  *media_type_out = media_type.Detach();
  return S_OK;
}

}  // namespace

/*static*/
HRESULT MediaFoundationVideoStream::Create(
    int stream_id,
    IMFMediaSource* parent_source,
    DemuxerStream* demuxer_stream,
    std::unique_ptr<MediaLog> media_log,
    MediaFoundationStreamWrapper** stream_out) {
  DVLOG(1) << __func__ << ": stream_id=" << stream_id;

  ComPtr<IMFMediaStream> video_stream;
  VideoCodec codec = demuxer_stream->video_decoder_config().codec();
  switch (codec) {
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    case VideoCodec::kH264:
      RETURN_IF_FAILED(MakeAndInitialize<MediaFoundationH264VideoStream>(
          &video_stream, stream_id, parent_source, demuxer_stream,
          std::move(media_log)));
      break;
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
    case VideoCodec::kHEVC:
#endif
#if BUILDFLAG(ENABLE_PLATFORM_DOLBY_VISION)
    case VideoCodec::kDolbyVision:
#endif
#if BUILDFLAG(ENABLE_PLATFORM_HEVC) || BUILDFLAG(ENABLE_PLATFORM_DOLBY_VISION)
      RETURN_IF_FAILED(MakeAndInitialize<MediaFoundationHEVCVideoStream>(
          &video_stream, stream_id, parent_source, demuxer_stream,
          std::move(media_log)));
      break;
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC) ||
        // BUILDFLAG(ENABLE_PLATFORM_DOLBY_VISION)
    default:
      RETURN_IF_FAILED(MakeAndInitialize<MediaFoundationVideoStream>(
          &video_stream, stream_id, parent_source, demuxer_stream,
          std::move(media_log)));
      break;
  }

  *stream_out =
      static_cast<MediaFoundationStreamWrapper*>(video_stream.Detach());
  return S_OK;
}

bool MediaFoundationVideoStream::IsEncrypted() const {
  VideoDecoderConfig decoder_config = demuxer_stream_->video_decoder_config();
  return decoder_config.is_encrypted();
}

HRESULT MediaFoundationVideoStream::GetMediaType(
    IMFMediaType** media_type_out) {
  VideoDecoderConfig decoder_config = demuxer_stream_->video_decoder_config();
  return GetVideoType(decoder_config, media_type_out);
}

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
HRESULT MediaFoundationH264VideoStream::GetMediaType(
    IMFMediaType** media_type_out) {
  VideoDecoderConfig decoder_config = demuxer_stream_->video_decoder_config();
  RETURN_IF_FAILED(GetVideoType(decoder_config, media_type_out));
  // Enable conversion to Annex-B
  demuxer_stream_->EnableBitstreamConverter();
  return S_OK;
}

bool MediaFoundationH264VideoStream::AreFormatChangesEnabled() {
  // Disable explicit format change event for H264 to allow switching to the
  // new stream without a full re-create, which will be much faster. This is
  // also due to the fact that the MFT decoder can handle some format changes
  // without a format change event. For format changes that the MFT decoder
  // cannot support (e.g. codec change), the playback will fail later with
  // MF_E_INVALIDMEDIATYPE (0xC00D36B4).
  return false;
}
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
bool MediaFoundationHEVCVideoStream::AreFormatChangesEnabled() {
  // Disable explicit format change event for HEVC to allow switching to the
  // new stream without a full re-create, which will be much faster. This is
  // also due to the fact that the MFT decoder can handle some format changes
  // without a format change event. For format changes that the MFT decoder
  // cannot support (e.g. codec change), the playback will fail later with
  // MF_E_INVALIDMEDIATYPE (0xC00D36B4).
  return false;
}
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)

}  // namespace media