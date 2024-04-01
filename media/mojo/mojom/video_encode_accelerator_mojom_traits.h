// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_MOJOM_VIDEO_ENCODE_ACCELERATOR_MOJOM_TRAITS_H_
#define MEDIA_MOJO_MOJOM_VIDEO_ENCODE_ACCELERATOR_MOJOM_TRAITS_H_

#include "media/base/bitrate.h"
#include "media/base/ipc/media_param_traits.h"
#include "media/mojo/mojom/media_types.mojom-shared.h"
#include "media/mojo/mojom/video_encode_accelerator.mojom-shared.h"
#include "media/video/video_encode_accelerator.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"

namespace mojo {

template <>
struct StructTraits<
    media::mojom::VideoEncodeAcceleratorSupportedProfileDataView,
    media::VideoEncodeAccelerator::SupportedProfile> {
  static media::VideoCodecProfile profile(
      const media::VideoEncodeAccelerator::SupportedProfile& profile) {
    return profile.profile;
  }

  static const gfx::Size& min_resolution(
      const media::VideoEncodeAccelerator::SupportedProfile& profile) {
    return profile.min_resolution;
  }

  static const gfx::Size& max_resolution(
      const media::VideoEncodeAccelerator::SupportedProfile& profile) {
    return profile.max_resolution;
  }

  static uint32_t max_framerate_numerator(
      const media::VideoEncodeAccelerator::SupportedProfile& profile) {
    return profile.max_framerate_numerator;
  }

  static uint32_t max_framerate_denominator(
      const media::VideoEncodeAccelerator::SupportedProfile& profile) {
    return profile.max_framerate_denominator;
  }

  static const std::vector<media::SVCScalabilityMode>& scalability_modes(
      const media::VideoEncodeAccelerator::SupportedProfile& profile) {
    return profile.scalability_modes;
  }

  static bool Read(
      media::mojom::VideoEncodeAcceleratorSupportedProfileDataView data,
      media::VideoEncodeAccelerator::SupportedProfile* out);
};

template <>
struct EnumTraits<media::mojom::VideoEncodeAccelerator_Error,
                  media::VideoEncodeAccelerator::Error> {
  static media::mojom::VideoEncodeAccelerator_Error ToMojom(
      media::VideoEncodeAccelerator::Error error);

  static bool FromMojom(media::mojom::VideoEncodeAccelerator_Error input,
                        media::VideoEncodeAccelerator::Error* out);
};

template <>
class StructTraits<media::mojom::VideoBitrateAllocationDataView,
                   media::VideoBitrateAllocation> {
 public:
  static std::vector<int32_t> bitrates(
      const media::VideoBitrateAllocation& bitrate_allocation);

  static bool Read(media::mojom::VideoBitrateAllocationDataView data,
                   media::VideoBitrateAllocation* out_bitrate_allocation);
};

template <>
struct UnionTraits<media::mojom::CodecMetadataDataView,
                   media::BitstreamBufferMetadata> {
  static media::mojom::CodecMetadataDataView::Tag GetTag(
      const media::BitstreamBufferMetadata& metadata) {
    if (metadata.h264) {
      return media::mojom::CodecMetadataDataView::Tag::H264;
    } else if (metadata.vp8) {
      return media::mojom::CodecMetadataDataView::Tag::VP8;
    } else if (metadata.vp9) {
      return media::mojom::CodecMetadataDataView::Tag::VP9;
    }
    NOTREACHED();
    return media::mojom::CodecMetadataDataView::Tag::VP8;
  }

  static bool IsNull(const media::BitstreamBufferMetadata& metadata) {
    return !metadata.h264 && !metadata.vp8 && !metadata.vp9;
  }

  static void SetToNull(media::BitstreamBufferMetadata* metadata) {
    metadata->h264.reset();
    metadata->vp8.reset();
    metadata->vp9.reset();
  }

  static const media::H264Metadata& h264(
      const media::BitstreamBufferMetadata& metadata) {
    return *metadata.h264;
  }

  static const media::Vp8Metadata& vp8(
      const media::BitstreamBufferMetadata& metadata) {
    return *metadata.vp8;
  }

  static const media::Vp9Metadata& vp9(
      const media::BitstreamBufferMetadata& metadata) {
    return *metadata.vp9;
  }

  static bool Read(media::mojom::CodecMetadataDataView data,
                   media::BitstreamBufferMetadata* metadata);
};

template <>
class StructTraits<media::mojom::BitstreamBufferMetadataDataView,
                   media::BitstreamBufferMetadata> {
 public:
  static size_t payload_size_bytes(const media::BitstreamBufferMetadata& bbm) {
    return bbm.payload_size_bytes;
  }
  static bool key_frame(const media::BitstreamBufferMetadata& bbm) {
    return bbm.key_frame;
  }
  static base::TimeDelta timestamp(const media::BitstreamBufferMetadata& bbm) {
    return bbm.timestamp;
  }
  static const media::BitstreamBufferMetadata& codec_metadata(
      const media::BitstreamBufferMetadata& bbm) {
    return bbm;
  }

  static bool Read(media::mojom::BitstreamBufferMetadataDataView data,
                   media::BitstreamBufferMetadata* out_metadata);
};

template <>
class StructTraits<media::mojom::H264MetadataDataView, media::H264Metadata> {
 public:
  static uint8_t temporal_idx(const media::H264Metadata& vp8) {
    return vp8.temporal_idx;
  }

  static bool layer_sync(const media::H264Metadata& vp8) {
    return vp8.layer_sync;
  }

  static bool Read(media::mojom::H264MetadataDataView data,
                   media::H264Metadata* out_metadata);
};

template <>
class StructTraits<media::mojom::Vp8MetadataDataView, media::Vp8Metadata> {
 public:
  static bool non_reference(const media::Vp8Metadata& vp8) {
    return vp8.non_reference;
  }

  static uint8_t temporal_idx(const media::Vp8Metadata& vp8) {
    return vp8.temporal_idx;
  }

  static bool layer_sync(const media::Vp8Metadata& vp8) {
    return vp8.layer_sync;
  }

  static bool Read(media::mojom::Vp8MetadataDataView data,
                   media::Vp8Metadata* out_metadata);
};

template <>
class StructTraits<media::mojom::Vp9MetadataDataView, media::Vp9Metadata> {
 public:
  static bool inter_pic_predicted(const media::Vp9Metadata& vp9) {
    return vp9.inter_pic_predicted;
  }
  static bool temporal_up_switch(const media::Vp9Metadata& vp9) {
    return vp9.temporal_up_switch;
  }
  static bool referenced_by_upper_spatial_layers(
      const media::Vp9Metadata& vp9) {
    return vp9.referenced_by_upper_spatial_layers;
  }
  static bool reference_lower_spatial_layers(const media::Vp9Metadata& vp9) {
    return vp9.reference_lower_spatial_layers;
  }
  static bool end_of_picture(const media::Vp9Metadata& vp9) {
    return vp9.end_of_picture;
  }
  static uint8_t temporal_idx(const media::Vp9Metadata& vp9) {
    return vp9.temporal_idx;
  }
  static uint8_t spatial_idx(const media::Vp9Metadata& vp9) {
    return vp9.spatial_idx;
  }
  static const std::vector<gfx::Size>& spatial_layer_resolutions(
      const media::Vp9Metadata& vp9) {
    return vp9.spatial_layer_resolutions;
  }
  static const std::vector<uint8_t>& p_diffs(const media::Vp9Metadata& vp9) {
    return vp9.p_diffs;
  }

  static bool Read(media::mojom::Vp9MetadataDataView data,
                   media::Vp9Metadata* out_metadata);
};

template <>
struct EnumTraits<media::mojom::VideoEncodeAcceleratorConfig_StorageType,
                  media::VideoEncodeAccelerator::Config::StorageType> {
  static media::mojom::VideoEncodeAcceleratorConfig_StorageType ToMojom(
      media::VideoEncodeAccelerator::Config::StorageType input);

  static bool FromMojom(
      media::mojom::VideoEncodeAcceleratorConfig_StorageType,
      media::VideoEncodeAccelerator::Config::StorageType* output);
};

template <>
struct EnumTraits<media::mojom::VideoEncodeAcceleratorConfig_InterLayerPredMode,
                  media::VideoEncodeAccelerator::Config::InterLayerPredMode> {
  static media::mojom::VideoEncodeAcceleratorConfig_InterLayerPredMode ToMojom(
      media::VideoEncodeAccelerator::Config::InterLayerPredMode input);

  static bool FromMojom(
      media::mojom::VideoEncodeAcceleratorConfig_InterLayerPredMode,
      media::VideoEncodeAccelerator::Config::InterLayerPredMode* output);
};

template <>
struct EnumTraits<media::mojom::VideoEncodeAcceleratorConfig_ContentType,
                  media::VideoEncodeAccelerator::Config::ContentType> {
  static media::mojom::VideoEncodeAcceleratorConfig_ContentType ToMojom(
      media::VideoEncodeAccelerator::Config::ContentType input);

  static bool FromMojom(
      media::mojom::VideoEncodeAcceleratorConfig_ContentType,
      media::VideoEncodeAccelerator::Config::ContentType* output);
};

template <>
struct StructTraits<media::mojom::SpatialLayerDataView,
                    media::VideoEncodeAccelerator::Config::SpatialLayer> {
  static int32_t width(
      const media::VideoEncodeAccelerator::Config::SpatialLayer& input) {
    return input.width;
  }

  static int32_t height(
      const media::VideoEncodeAccelerator::Config::SpatialLayer& input) {
    return input.height;
  }

  static uint32_t bitrate_bps(
      const media::VideoEncodeAccelerator::Config::SpatialLayer& input) {
    return input.bitrate_bps;
  }

  static uint32_t framerate(
      const media::VideoEncodeAccelerator::Config::SpatialLayer& input) {
    return input.framerate;
  }

  static uint8_t max_qp(
      const media::VideoEncodeAccelerator::Config::SpatialLayer& input) {
    return input.max_qp;
  }

  static uint8_t num_of_temporal_layers(
      const media::VideoEncodeAccelerator::Config::SpatialLayer& input) {
    return input.num_of_temporal_layers;
  }

  static bool Read(media::mojom::SpatialLayerDataView input,
                   media::VideoEncodeAccelerator::Config::SpatialLayer* output);
};

template <>
struct EnumTraits<media::mojom::Bitrate_Mode, media::Bitrate::Mode> {
  static media::mojom::Bitrate_Mode ToMojom(media::Bitrate::Mode input);

  static bool FromMojom(media::mojom::Bitrate_Mode,
                        media::Bitrate::Mode* output);
};

template <>
struct StructTraits<media::mojom::BitrateDataView, media::Bitrate> {
  static media::Bitrate::Mode mode(const media::Bitrate& input) {
    return input.mode();
  }

  static uint32_t target(const media::Bitrate& input) { return input.target(); }

  static uint32_t peak(const media::Bitrate& input) { return input.peak(); }

  static bool Read(media::mojom::BitrateDataView input, media::Bitrate* output);
};

template <>
struct StructTraits<media::mojom::VideoEncodeAcceleratorConfigDataView,
                    media::VideoEncodeAccelerator::Config> {
  static media::VideoPixelFormat input_format(
      const media::VideoEncodeAccelerator::Config& input) {
    return input.input_format;
  }

  static const gfx::Size& input_visible_size(
      const media::VideoEncodeAccelerator::Config& input) {
    return input.input_visible_size;
  }

  static media::VideoCodecProfile output_profile(
      const media::VideoEncodeAccelerator::Config& input) {
    return input.output_profile;
  }

  static const media::Bitrate& bitrate(
      const media::VideoEncodeAccelerator::Config& input) {
    return input.bitrate;
  }

  static uint32_t initial_framerate(
      const media::VideoEncodeAccelerator::Config& input) {
    return input.initial_framerate.value_or(0);
  }

  static bool has_initial_framerate(
      const media::VideoEncodeAccelerator::Config& input) {
    return input.initial_framerate.has_value();
  }

  static uint32_t gop_length(
      const media::VideoEncodeAccelerator::Config& input) {
    return input.gop_length.value_or(0);
  }

  static bool has_gop_length(
      const media::VideoEncodeAccelerator::Config& input) {
    return input.gop_length.has_value();
  }

  static uint8_t h264_output_level(
      const media::VideoEncodeAccelerator::Config& input) {
    return input.h264_output_level.value_or(0);
  }

  static bool has_h264_output_level(
      const media::VideoEncodeAccelerator::Config& input) {
    return input.h264_output_level.has_value();
  }

  static bool is_constrained_h264(
      const media::VideoEncodeAccelerator::Config& input) {
    return input.is_constrained_h264;
  }

  static media::VideoEncodeAccelerator::Config::StorageType storage_type(
      const media::VideoEncodeAccelerator::Config& input) {
    return input.storage_type.value_or(
        media::VideoEncodeAccelerator::Config::StorageType::kShmem);
  }

  static bool has_storage_type(
      const media::VideoEncodeAccelerator::Config& input) {
    return input.storage_type.has_value();
  }

  static media::VideoEncodeAccelerator::Config::ContentType content_type(
      const media::VideoEncodeAccelerator::Config& input) {
    return input.content_type;
  }

  static const std::vector<media::VideoEncodeAccelerator::Config::SpatialLayer>&
  spatial_layers(const media::VideoEncodeAccelerator::Config& input) {
    return input.spatial_layers;
  }

  static media::VideoEncodeAccelerator::Config::InterLayerPredMode
  inter_layer_pred(const media::VideoEncodeAccelerator::Config& input) {
    return input.inter_layer_pred;
  }

  static bool require_low_delay(
      const media::VideoEncodeAccelerator::Config& input) {
    return input.require_low_delay;
  }

  static bool Read(media::mojom::VideoEncodeAcceleratorConfigDataView input,
                   media::VideoEncodeAccelerator::Config* output);
};

}  // namespace mojo

#endif  // MEDIA_MOJO_MOJOM_VIDEO_ENCODE_ACCELERATOR_MOJOM_TRAITS_H_
