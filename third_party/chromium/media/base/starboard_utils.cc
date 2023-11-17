// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "media/base/starboard_utils.h"

#include <algorithm>

#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decrypt_config.h"
#include "starboard/common/media.h"
#include "starboard/configuration.h"
#include "starboard/memory.h"

using base::Time;
using base::TimeDelta;

namespace media {
namespace {

int GetBitsPerPixel(const std::string& mime_type) {
  auto codecs = ExtractCodecs(mime_type);
  SbMediaVideoCodec codec;
  int profile;
  int level;
  int bit_depth;
  SbMediaPrimaryId primary_id;
  SbMediaTransferId transfer_id;
  SbMediaMatrixId matrix_id;

  if (starboard::ParseVideoCodec(codecs.c_str(), &codec, &profile, &level,
                                 &bit_depth, &primary_id, &transfer_id,
                                 &matrix_id)) {
    return bit_depth;
  }

  // Assume SDR when there isn't enough information to determine the bit depth.
  return 8;
}

}  // namespace

SbMediaAudioCodec MediaAudioCodecToSbMediaAudioCodec(AudioCodec codec) {
  switch (codec) {
    case AudioCodec::kAAC:
      return kSbMediaAudioCodecAac;
    case AudioCodec::kAC3:
#if SB_API_VERSION < 15
      if (!kSbHasAc3Audio) {
        DLOG(ERROR) << "Audio codec AC3 not enabled on this platform. To "
                    << "enable it, set kSbHasAc3Audio to |true|.";
        return kSbMediaAudioCodecNone;
      }
#endif  // SB_API_VERSION < 15
      return kSbMediaAudioCodecAc3;
    case AudioCodec::kEAC3:
#if SB_API_VERSION < 15
      if (!kSbHasAc3Audio) {
        DLOG(ERROR) << "Audio codec AC3 not enabled on this platform. To "
                    << "enable it, set kSbHasAc3Audio to |true|.";
        return kSbMediaAudioCodecNone;
      }
#endif  // SB_API_VERSION < 15
      return kSbMediaAudioCodecEac3;
    case AudioCodec::kVorbis:
      return kSbMediaAudioCodecVorbis;
    case AudioCodec::kOpus:
      return kSbMediaAudioCodecOpus;
#if SB_API_VERSION >= 14
    case AudioCodec::kMP3:
      return kSbMediaAudioCodecMp3;
    case AudioCodec::kFLAC:
      return kSbMediaAudioCodecFlac;
    case AudioCodec::kPCM:
      return kSbMediaAudioCodecPcm;
#endif  // SB_API_VERSION >= 14
#if SB_API_VERSION >= 15
    // TODO(b/271301103): Enable this once IAMF is added to Chromium.
    // case AudioCodec::kIAMF:
    //  return kSbMediaAudioCodecPcm;
#endif  // SB_API_VERSION >= 15
    default:
      // Cobalt only supports a subset of audio codecs defined by Chromium.
      DLOG(ERROR) << "Unsupported audio codec " << GetCodecName(codec);
      return kSbMediaAudioCodecNone;
  }
  NOTREACHED();
  return kSbMediaAudioCodecNone;
}

SbMediaVideoCodec MediaVideoCodecToSbMediaVideoCodec(VideoCodec codec) {
  switch (codec) {
    case VideoCodec::kH264:
      return kSbMediaVideoCodecH264;
    case VideoCodec::kVC1:
      return kSbMediaVideoCodecVc1;
    case VideoCodec::kMPEG2:
      return kSbMediaVideoCodecMpeg2;
    case VideoCodec::kTheora:
      return kSbMediaVideoCodecTheora;
    case VideoCodec::kVP8:
      return kSbMediaVideoCodecVp8;
    case VideoCodec::kVP9:
      return kSbMediaVideoCodecVp9;
    case VideoCodec::kHEVC:
      return kSbMediaVideoCodecH265;
    case VideoCodec::kAV1:
      return kSbMediaVideoCodecAv1;
    default:
      // Cobalt only supports a subset of video codecs defined by Chromium.
      DLOG(ERROR) << "Unsupported video codec " << GetCodecName(codec);
      return kSbMediaVideoCodecNone;
  }
  NOTREACHED();
  return kSbMediaVideoCodecNone;
}

SbMediaAudioStreamInfo MediaAudioConfigToSbMediaAudioStreamInfo(
    const AudioDecoderConfig& audio_decoder_config,
    const char* mime_type) {
  DCHECK(audio_decoder_config.IsValidConfig());

  SbMediaAudioStreamInfo audio_stream_info;

  audio_stream_info.codec =
      MediaAudioCodecToSbMediaAudioCodec(audio_decoder_config.codec());
  audio_stream_info.mime = mime_type;

#if SB_API_VERSION < 15
  audio_stream_info.format_tag = 0x00ff;
#endif // SB_API_VERSION < 15
  audio_stream_info.number_of_channels =
      ChannelLayoutToChannelCount(audio_decoder_config.channel_layout());
  audio_stream_info.samples_per_second =
      audio_decoder_config.samples_per_second();
#if SB_API_VERSION < 15
  audio_stream_info.average_bytes_per_second = 1;
  audio_stream_info.block_alignment = 4;
#endif // SB_API_VERSION < 15
  audio_stream_info.bits_per_sample = audio_decoder_config.bits_per_channel();

  const auto& extra_data = audio_stream_info.codec == kSbMediaAudioCodecAac
                               ? audio_decoder_config.aac_extra_data()
                               : audio_decoder_config.extra_data();
  audio_stream_info.audio_specific_config_size =
      static_cast<uint16_t>(extra_data.size());
  if (audio_stream_info.audio_specific_config_size == 0) {
    audio_stream_info.audio_specific_config = NULL;
  } else {
    audio_stream_info.audio_specific_config = extra_data.data();
  }

  return audio_stream_info;
}

DemuxerStream::Type SbMediaTypeToDemuxerStreamType(SbMediaType type) {
  if (type == kSbMediaTypeAudio) {
    return DemuxerStream::AUDIO;
  }
  DCHECK_EQ(type, kSbMediaTypeVideo);
  return DemuxerStream::VIDEO;
}

SbMediaType DemuxerStreamTypeToSbMediaType(DemuxerStream::Type type) {
  if (type == DemuxerStream::AUDIO) {
    return kSbMediaTypeAudio;
  }
  DCHECK_EQ(type, DemuxerStream::VIDEO);
  return kSbMediaTypeVideo;
}

void FillDrmSampleInfo(const scoped_refptr<DecoderBuffer>& buffer,
                       SbDrmSampleInfo* drm_info,
                       SbDrmSubSampleMapping* subsample_mapping) {
  DCHECK(drm_info);
  DCHECK(subsample_mapping);

  const DecryptConfig* config = buffer->decrypt_config();

  if (config->encryption_scheme() == EncryptionScheme::kCenc) {
    drm_info->encryption_scheme = kSbDrmEncryptionSchemeAesCtr;
  } else {
    DCHECK_EQ(config->encryption_scheme(), EncryptionScheme::kCbcs);
    drm_info->encryption_scheme = kSbDrmEncryptionSchemeAesCbc;
  }

  // Set content of |drm_info| to default or invalid values.
  drm_info->encryption_pattern.crypt_byte_block = 0;
  drm_info->encryption_pattern.skip_byte_block = 0;
  drm_info->initialization_vector_size = 0;
  drm_info->identifier_size = 0;
  drm_info->subsample_count = 0;
  drm_info->subsample_mapping = NULL;

  if (!config || config->iv().empty() || config->key_id().empty()) {
    return;
  }

  DCHECK_LE(config->iv().size(), sizeof(drm_info->initialization_vector));
  DCHECK_LE(config->key_id().size(), sizeof(drm_info->identifier));

  if (config->iv().size() > sizeof(drm_info->initialization_vector) ||
      config->key_id().size() > sizeof(drm_info->identifier)) {
    return;
  }

  memcpy(drm_info->initialization_vector, &config->iv()[0],
         config->iv().size());
  drm_info->initialization_vector_size = config->iv().size();
  memcpy(drm_info->identifier, &config->key_id()[0], config->key_id().size());
  drm_info->identifier_size = config->key_id().size();
  drm_info->subsample_count = config->subsamples().size();

  if (drm_info->subsample_count > 0) {
    COMPILE_ASSERT(sizeof(SbDrmSubSampleMapping) == sizeof(SubsampleEntry),
                   SubSampleEntrySizesMatch);
    drm_info->subsample_mapping =
        reinterpret_cast<const SbDrmSubSampleMapping*>(
            &config->subsamples()[0]);
  } else {
    drm_info->subsample_count = 1;
    drm_info->subsample_mapping = subsample_mapping;
    subsample_mapping->clear_byte_count = 0;
    subsample_mapping->encrypted_byte_count = buffer->data_size();
  }

  if (buffer->decrypt_config()->HasPattern()) {
    drm_info->encryption_pattern.crypt_byte_block =
        config->encryption_pattern()->crypt_byte_block();
    drm_info->encryption_pattern.skip_byte_block =
        config->encryption_pattern()->skip_byte_block();
  }
}

// Ensure that the enums in starboard/media.h match enums in
// VideoColorSpace and gfx::ColorSpace.
#define ENUM_EQ(a, b) \
  COMPILE_ASSERT(static_cast<int>(a) == static_cast<int>(b), mismatching_enums)

// Ensure PrimaryId enums convert correctly.
ENUM_EQ(kSbMediaPrimaryIdReserved0, VideoColorSpace::PrimaryID::INVALID);
ENUM_EQ(kSbMediaPrimaryIdBt709, VideoColorSpace::PrimaryID::BT709);
ENUM_EQ(kSbMediaPrimaryIdUnspecified, VideoColorSpace::PrimaryID::UNSPECIFIED);
ENUM_EQ(kSbMediaPrimaryIdBt470M, VideoColorSpace::PrimaryID::BT470M);
ENUM_EQ(kSbMediaPrimaryIdBt470Bg, VideoColorSpace::PrimaryID::BT470BG);
ENUM_EQ(kSbMediaPrimaryIdSmpte170M, VideoColorSpace::PrimaryID::SMPTE170M);
ENUM_EQ(kSbMediaPrimaryIdSmpte240M, VideoColorSpace::PrimaryID::SMPTE240M);
ENUM_EQ(kSbMediaPrimaryIdFilm, VideoColorSpace::PrimaryID::FILM);
ENUM_EQ(kSbMediaPrimaryIdBt2020, VideoColorSpace::PrimaryID::BT2020);
ENUM_EQ(kSbMediaPrimaryIdSmpteSt4281, VideoColorSpace::PrimaryID::SMPTEST428_1);
ENUM_EQ(kSbMediaPrimaryIdSmpteSt4312, VideoColorSpace::PrimaryID::SMPTEST431_2);
ENUM_EQ(kSbMediaPrimaryIdSmpteSt4321, VideoColorSpace::PrimaryID::SMPTEST432_1);

// Ensure TransferId enums convert correctly.
ENUM_EQ(kSbMediaTransferIdReserved0, VideoColorSpace::TransferID::INVALID);
ENUM_EQ(kSbMediaTransferIdBt709, VideoColorSpace::TransferID::BT709);
ENUM_EQ(kSbMediaTransferIdUnspecified,
        VideoColorSpace::TransferID::UNSPECIFIED);
ENUM_EQ(kSbMediaTransferIdGamma22, VideoColorSpace::TransferID::GAMMA22);
ENUM_EQ(kSbMediaTransferIdGamma28, VideoColorSpace::TransferID::GAMMA28);
ENUM_EQ(kSbMediaTransferIdSmpte170M, VideoColorSpace::TransferID::SMPTE170M);
ENUM_EQ(kSbMediaTransferIdSmpte240M, VideoColorSpace::TransferID::SMPTE240M);
ENUM_EQ(kSbMediaTransferIdLinear, VideoColorSpace::TransferID::LINEAR);
ENUM_EQ(kSbMediaTransferIdLog, VideoColorSpace::TransferID::LOG);
ENUM_EQ(kSbMediaTransferIdLogSqrt, VideoColorSpace::TransferID::LOG_SQRT);
ENUM_EQ(kSbMediaTransferIdIec6196624,
        VideoColorSpace::TransferID::IEC61966_2_4);
ENUM_EQ(kSbMediaTransferIdBt1361Ecg, VideoColorSpace::TransferID::BT1361_ECG);
ENUM_EQ(kSbMediaTransferIdIec6196621,
        VideoColorSpace::TransferID::IEC61966_2_1);
ENUM_EQ(kSbMediaTransferId10BitBt2020, VideoColorSpace::TransferID::BT2020_10);
ENUM_EQ(kSbMediaTransferId12BitBt2020, VideoColorSpace::TransferID::BT2020_12);
ENUM_EQ(kSbMediaTransferIdSmpteSt2084,
        VideoColorSpace::TransferID::SMPTEST2084);
ENUM_EQ(kSbMediaTransferIdSmpteSt4281,
        VideoColorSpace::TransferID::SMPTEST428_1);
ENUM_EQ(kSbMediaTransferIdAribStdB67,
        VideoColorSpace::TransferID::ARIB_STD_B67);

// Ensure MatrixId enums convert correctly.
ENUM_EQ(kSbMediaMatrixIdRgb, VideoColorSpace::MatrixID::RGB);
ENUM_EQ(kSbMediaMatrixIdBt709, VideoColorSpace::MatrixID::BT709);
ENUM_EQ(kSbMediaMatrixIdUnspecified, VideoColorSpace::MatrixID::UNSPECIFIED);
ENUM_EQ(kSbMediaMatrixIdFcc, VideoColorSpace::MatrixID::FCC);
ENUM_EQ(kSbMediaMatrixIdBt470Bg, VideoColorSpace::MatrixID::BT470BG);
ENUM_EQ(kSbMediaMatrixIdSmpte170M, VideoColorSpace::MatrixID::SMPTE170M);
ENUM_EQ(kSbMediaMatrixIdSmpte240M, VideoColorSpace::MatrixID::SMPTE240M);
ENUM_EQ(kSbMediaMatrixIdYCgCo, VideoColorSpace::MatrixID::YCOCG);
ENUM_EQ(kSbMediaMatrixIdBt2020NonconstantLuminance,
        VideoColorSpace::MatrixID::BT2020_NCL);
ENUM_EQ(kSbMediaMatrixIdBt2020ConstantLuminance,
        VideoColorSpace::MatrixID::BT2020_CL);
ENUM_EQ(kSbMediaMatrixIdYDzDx, VideoColorSpace::MatrixID::YDZDX);

#if SB_API_VERSION >= 14
ENUM_EQ(kSbMediaMatrixIdInvalid, VideoColorSpace::MatrixID::INVALID);
#endif  // SB_API_VERSION >= 14

// Ensure RangeId enums convert correctly.
ENUM_EQ(kSbMediaRangeIdUnspecified, gfx::ColorSpace::RangeID::INVALID);
ENUM_EQ(kSbMediaRangeIdLimited, gfx::ColorSpace::RangeID::LIMITED);
ENUM_EQ(kSbMediaRangeIdFull, gfx::ColorSpace::RangeID::FULL);
ENUM_EQ(kSbMediaRangeIdDerived, gfx::ColorSpace::RangeID::DERIVED);

SbMediaColorMetadata MediaToSbMediaColorMetadata(
    const VideoColorSpace& color_space,
    const absl::optional<gfx::HDRMetadata>& hdr_metadata,
    const std::string& mime_type) {
  SbMediaColorMetadata sb_media_color_metadata = {};

  sb_media_color_metadata.bits_per_channel = GetBitsPerPixel(mime_type);

  // Copy the other color metadata below.
  // TODO(b/230915942): Revisit to ensure that the metadata is valid and
  // consider deprecate them from `SbMediaColorMetadata`.
  sb_media_color_metadata.chroma_subsampling_horizontal = 0;
  sb_media_color_metadata.chroma_subsampling_vertical = 0;
  sb_media_color_metadata.cb_subsampling_horizontal = 0;
  sb_media_color_metadata.cb_subsampling_vertical = 0;
  sb_media_color_metadata.chroma_siting_horizontal = 0;
  sb_media_color_metadata.chroma_siting_vertical = 0;

  // Copy the HDR Metadata below.
  SbMediaMasteringMetadata sb_media_mastering_metadata = {};

  if (hdr_metadata) {
    const auto& color_volume_metadata = hdr_metadata->color_volume_metadata;

    sb_media_mastering_metadata.primary_r_chromaticity_x =
        color_volume_metadata.primary_r.x();
    sb_media_mastering_metadata.primary_r_chromaticity_y =
        color_volume_metadata.primary_r.y();

    sb_media_mastering_metadata.primary_g_chromaticity_x =
        color_volume_metadata.primary_g.x();
    sb_media_mastering_metadata.primary_g_chromaticity_y =
        color_volume_metadata.primary_g.y();

    sb_media_mastering_metadata.primary_b_chromaticity_x =
        color_volume_metadata.primary_b.x();
    sb_media_mastering_metadata.primary_b_chromaticity_y =
        color_volume_metadata.primary_b.y();

    sb_media_mastering_metadata.white_point_chromaticity_x =
        color_volume_metadata.white_point.x();
    sb_media_mastering_metadata.white_point_chromaticity_y =
        color_volume_metadata.white_point.y();

    sb_media_mastering_metadata.luminance_max =
        color_volume_metadata.luminance_max;
    sb_media_mastering_metadata.luminance_min =
        color_volume_metadata.luminance_min;

    sb_media_color_metadata.mastering_metadata = sb_media_mastering_metadata;
    sb_media_color_metadata.max_cll = hdr_metadata->max_content_light_level;
    sb_media_color_metadata.max_fall =
        hdr_metadata->max_frame_average_light_level;
  }

  // Copy the color space below.
  sb_media_color_metadata.primaries =
      static_cast<SbMediaPrimaryId>(color_space.primaries);
  sb_media_color_metadata.transfer =
      static_cast<SbMediaTransferId>(color_space.transfer);
  sb_media_color_metadata.matrix =
      static_cast<SbMediaMatrixId>(color_space.matrix);

#if SB_API_VERSION < 14
  if (color_space.matrix == VideoColorSpace::MatrixID::INVALID) {
    sb_media_color_metadata.matrix = kSbMediaMatrixIdUnknown;
  }
#endif  // SB_API_VERSION < 14

  sb_media_color_metadata.range =
      static_cast<SbMediaRangeId>(color_space.range);
  // TODO(b/230915942): Revisit to see if we have to support custom primary id.
  // if (sb_media_color_metadata.primaries == kSbMediaPrimaryIdCustom) {
  //   const float* custom_primary_matrix = color_space.custom_primary_matrix();
  //   memcpy(sb_media_color_metadata.custom_primary_matrix,
  //          custom_primary_matrix,
  //          sizeof(sb_media_color_metadata.custom_primary_matrix));
  // }

  return sb_media_color_metadata;
}
int GetSbMediaVideoBufferBudget(const VideoDecoderConfig* video_config,
                                const std::string& mime_type) {
  if (!video_config) {
    return DecoderBuffer::Allocator::GetInstance()->GetVideoBufferBudget(
        kSbMediaVideoCodecH264, 1920, 1080, 8);
  }

  auto width = video_config->visible_rect().size().width();
  auto height = video_config->visible_rect().size().height();
  auto bits_per_pixel = GetBitsPerPixel(mime_type);
  auto codec = MediaVideoCodecToSbMediaVideoCodec(video_config->codec());
  return DecoderBuffer::Allocator::GetInstance()->GetVideoBufferBudget(
      codec, width, height, bits_per_pixel);
}

std::string ExtractCodecs(const std::string& mime_type) {
  static const char kCodecs[] = "codecs=";

  // SplitString will also trim the results.
  std::vector<std::string> tokens = ::base::SplitString(
      mime_type, ";", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  for (size_t i = 1; i < tokens.size(); ++i) {
    if (strncasecmp(tokens[i].c_str(), kCodecs, strlen(kCodecs))) {
      continue;
    }
    auto codec = tokens[i].substr(strlen(kCodecs));
    base::TrimString(codec, " \"", &codec);
    return codec;
  }
  LOG(WARNING) << "Failed to find codecs in mime type \"" << mime_type << '\"';
  return "";
}

}  // namespace media
