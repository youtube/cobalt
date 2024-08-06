// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_MOJO_MOJOM_VIDEO_ENCODE_ACCELERATOR_MOJOM_TRAITS_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_MOJO_MOJOM_VIDEO_ENCODE_ACCELERATOR_MOJOM_TRAITS_H_

#include "third_party/chromium/media/base/bitrate.h"
#include "third_party/chromium/media/base/ipc/media_param_traits.h"
#include "third_party/chromium/media/mojo/mojom/media_types.mojom-shared.h"
#include "third_party/chromium/media/mojo/mojom/video_encode_accelerator.mojom-shared.h"
#include "third_party/chromium/media/video/video_encode_accelerator.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"

namespace mojo {

template <>
struct StructTraits<
    media_m96::mojom::VideoEncodeAcceleratorSupportedProfileDataView,
    media_m96::VideoEncodeAccelerator::SupportedProfile> {
  static media_m96::VideoCodecProfile profile(
      const media_m96::VideoEncodeAccelerator::SupportedProfile& profile) {
    return profile.profile;
  }

  static const gfx::Size& min_resolution(
      const media_m96::VideoEncodeAccelerator::SupportedProfile& profile) {
    return profile.min_resolution;
  }

  static const gfx::Size& max_resolution(
      const media_m96::VideoEncodeAccelerator::SupportedProfile& profile) {
    return profile.max_resolution;
  }

  static uint32_t max_framerate_numerator(
      const media_m96::VideoEncodeAccelerator::SupportedProfile& profile) {
    return profile.max_framerate_numerator;
  }

  static uint32_t max_framerate_denominator(
      const media_m96::VideoEncodeAccelerator::SupportedProfile& profile) {
    return profile.max_framerate_denominator;
  }

  static const std::vector<media_m96::SVCScalabilityMode>& scalability_modes(
      const media_m96::VideoEncodeAccelerator::SupportedProfile& profile) {
    return profile.scalability_modes;
  }

  static bool Read(
      media_m96::mojom::VideoEncodeAcceleratorSupportedProfileDataView data,
      media_m96::VideoEncodeAccelerator::SupportedProfile* out);
};

template <>
struct EnumTraits<media_m96::mojom::VideoEncodeAccelerator_Error,
                  media_m96::VideoEncodeAccelerator::Error> {
  static media_m96::mojom::VideoEncodeAccelerator_Error ToMojom(
      media_m96::VideoEncodeAccelerator::Error error);

  static bool FromMojom(media_m96::mojom::VideoEncodeAccelerator_Error input,
                        media_m96::VideoEncodeAccelerator::Error* out);
};

template <>
class StructTraits<media_m96::mojom::VideoBitrateAllocationDataView,
                   media_m96::VideoBitrateAllocation> {
 public:
  static std::vector<int32_t> bitrates(
      const media_m96::VideoBitrateAllocation& bitrate_allocation);

  static bool Read(media_m96::mojom::VideoBitrateAllocationDataView data,
                   media_m96::VideoBitrateAllocation* out_bitrate_allocation);
};

template <>
struct UnionTraits<media_m96::mojom::CodecMetadataDataView,
                   media_m96::BitstreamBufferMetadata> {
  static media_m96::mojom::CodecMetadataDataView::Tag GetTag(
      const media_m96::BitstreamBufferMetadata& metadata) {
    if (metadata.h264) {
      return media_m96::mojom::CodecMetadataDataView::Tag::H264;
    } else if (metadata.vp8) {
      return media_m96::mojom::CodecMetadataDataView::Tag::VP8;
    } else if (metadata.vp9) {
      return media_m96::mojom::CodecMetadataDataView::Tag::VP9;
    }
    NOTREACHED();
    return media_m96::mojom::CodecMetadataDataView::Tag::VP8;
  }

  static bool IsNull(const media_m96::BitstreamBufferMetadata& metadata) {
    return !metadata.h264 && !metadata.vp8 && !metadata.vp9;
  }

  static void SetToNull(media_m96::BitstreamBufferMetadata* metadata) {
    metadata->h264.reset();
    metadata->vp8.reset();
    metadata->vp9.reset();
  }

  static const media_m96::H264Metadata& h264(
      const media_m96::BitstreamBufferMetadata& metadata) {
    return *metadata.h264;
  }

  static const media_m96::Vp8Metadata& vp8(
      const media_m96::BitstreamBufferMetadata& metadata) {
    return *metadata.vp8;
  }

  static const media_m96::Vp9Metadata& vp9(
      const media_m96::BitstreamBufferMetadata& metadata) {
    return *metadata.vp9;
  }

  static bool Read(media_m96::mojom::CodecMetadataDataView data,
                   media_m96::BitstreamBufferMetadata* metadata);
};

template <>
class StructTraits<media_m96::mojom::BitstreamBufferMetadataDataView,
                   media_m96::BitstreamBufferMetadata> {
 public:
  static size_t payload_size_bytes(const media_m96::BitstreamBufferMetadata& bbm) {
    return bbm.payload_size_bytes;
  }
  static bool key_frame(const media_m96::BitstreamBufferMetadata& bbm) {
    return bbm.key_frame;
  }
  static base::TimeDelta timestamp(const media_m96::BitstreamBufferMetadata& bbm) {
    return bbm.timestamp;
  }
  static const media_m96::BitstreamBufferMetadata& codec_metadata(
      const media_m96::BitstreamBufferMetadata& bbm) {
    return bbm;
  }

  static bool Read(media_m96::mojom::BitstreamBufferMetadataDataView data,
                   media_m96::BitstreamBufferMetadata* out_metadata);
};

template <>
class StructTraits<media_m96::mojom::H264MetadataDataView, media_m96::H264Metadata> {
 public:
  static uint8_t temporal_idx(const media_m96::H264Metadata& vp8) {
    return vp8.temporal_idx;
  }

  static bool layer_sync(const media_m96::H264Metadata& vp8) {
    return vp8.layer_sync;
  }

  static bool Read(media_m96::mojom::H264MetadataDataView data,
                   media_m96::H264Metadata* out_metadata);
};

template <>
class StructTraits<media_m96::mojom::Vp8MetadataDataView, media_m96::Vp8Metadata> {
 public:
  static bool non_reference(const media_m96::Vp8Metadata& vp8) {
    return vp8.non_reference;
  }

  static uint8_t temporal_idx(const media_m96::Vp8Metadata& vp8) {
    return vp8.temporal_idx;
  }

  static bool layer_sync(const media_m96::Vp8Metadata& vp8) {
    return vp8.layer_sync;
  }

  static bool Read(media_m96::mojom::Vp8MetadataDataView data,
                   media_m96::Vp8Metadata* out_metadata);
};

template <>
class StructTraits<media_m96::mojom::Vp9MetadataDataView, media_m96::Vp9Metadata> {
 public:
  static bool inter_pic_predicted(const media_m96::Vp9Metadata& vp9) {
    return vp9.inter_pic_predicted;
  }
  static bool temporal_up_switch(const media_m96::Vp9Metadata& vp9) {
    return vp9.temporal_up_switch;
  }
  static bool referenced_by_upper_spatial_layers(
      const media_m96::Vp9Metadata& vp9) {
    return vp9.referenced_by_upper_spatial_layers;
  }
  static bool reference_lower_spatial_layers(const media_m96::Vp9Metadata& vp9) {
    return vp9.reference_lower_spatial_layers;
  }
  static bool end_of_picture(const media_m96::Vp9Metadata& vp9) {
    return vp9.end_of_picture;
  }
  static uint8_t temporal_idx(const media_m96::Vp9Metadata& vp9) {
    return vp9.temporal_idx;
  }
  static uint8_t spatial_idx(const media_m96::Vp9Metadata& vp9) {
    return vp9.spatial_idx;
  }
  static const std::vector<gfx::Size>& spatial_layer_resolutions(
      const media_m96::Vp9Metadata& vp9) {
    return vp9.spatial_layer_resolutions;
  }
  static const std::vector<uint8_t>& p_diffs(const media_m96::Vp9Metadata& vp9) {
    return vp9.p_diffs;
  }

  static bool Read(media_m96::mojom::Vp9MetadataDataView data,
                   media_m96::Vp9Metadata* out_metadata);
};

template <>
struct EnumTraits<media_m96::mojom::VideoEncodeAcceleratorConfig_StorageType,
                  media_m96::VideoEncodeAccelerator::Config::StorageType> {
  static media_m96::mojom::VideoEncodeAcceleratorConfig_StorageType ToMojom(
      media_m96::VideoEncodeAccelerator::Config::StorageType input);

  static bool FromMojom(
      media_m96::mojom::VideoEncodeAcceleratorConfig_StorageType,
      media_m96::VideoEncodeAccelerator::Config::StorageType* output);
};

template <>
struct EnumTraits<media_m96::mojom::VideoEncodeAcceleratorConfig_InterLayerPredMode,
                  media_m96::VideoEncodeAccelerator::Config::InterLayerPredMode> {
  static media_m96::mojom::VideoEncodeAcceleratorConfig_InterLayerPredMode ToMojom(
      media_m96::VideoEncodeAccelerator::Config::InterLayerPredMode input);

  static bool FromMojom(
      media_m96::mojom::VideoEncodeAcceleratorConfig_InterLayerPredMode,
      media_m96::VideoEncodeAccelerator::Config::InterLayerPredMode* output);
};

template <>
struct EnumTraits<media_m96::mojom::VideoEncodeAcceleratorConfig_ContentType,
                  media_m96::VideoEncodeAccelerator::Config::ContentType> {
  static media_m96::mojom::VideoEncodeAcceleratorConfig_ContentType ToMojom(
      media_m96::VideoEncodeAccelerator::Config::ContentType input);

  static bool FromMojom(
      media_m96::mojom::VideoEncodeAcceleratorConfig_ContentType,
      media_m96::VideoEncodeAccelerator::Config::ContentType* output);
};

template <>
struct StructTraits<media_m96::mojom::SpatialLayerDataView,
                    media_m96::VideoEncodeAccelerator::Config::SpatialLayer> {
  static int32_t width(
      const media_m96::VideoEncodeAccelerator::Config::SpatialLayer& input) {
    return input.width;
  }

  static int32_t height(
      const media_m96::VideoEncodeAccelerator::Config::SpatialLayer& input) {
    return input.height;
  }

  static uint32_t bitrate_bps(
      const media_m96::VideoEncodeAccelerator::Config::SpatialLayer& input) {
    return input.bitrate_bps;
  }

  static uint32_t framerate(
      const media_m96::VideoEncodeAccelerator::Config::SpatialLayer& input) {
    return input.framerate;
  }

  static uint8_t max_qp(
      const media_m96::VideoEncodeAccelerator::Config::SpatialLayer& input) {
    return input.max_qp;
  }

  static uint8_t num_of_temporal_layers(
      const media_m96::VideoEncodeAccelerator::Config::SpatialLayer& input) {
    return input.num_of_temporal_layers;
  }

  static bool Read(media_m96::mojom::SpatialLayerDataView input,
                   media_m96::VideoEncodeAccelerator::Config::SpatialLayer* output);
};

template <>
struct EnumTraits<media_m96::mojom::Bitrate_Mode, media_m96::Bitrate::Mode> {
  static media_m96::mojom::Bitrate_Mode ToMojom(media_m96::Bitrate::Mode input);

  static bool FromMojom(media_m96::mojom::Bitrate_Mode,
                        media_m96::Bitrate::Mode* output);
};

template <>
struct StructTraits<media_m96::mojom::BitrateDataView, media_m96::Bitrate> {
  static media_m96::Bitrate::Mode mode(const media_m96::Bitrate& input) {
    return input.mode();
  }

  static uint32_t target(const media_m96::Bitrate& input) { return input.target(); }

  static uint32_t peak(const media_m96::Bitrate& input) { return input.peak(); }

  static bool Read(media_m96::mojom::BitrateDataView input, media_m96::Bitrate* output);
};

template <>
struct StructTraits<media_m96::mojom::VideoEncodeAcceleratorConfigDataView,
                    media_m96::VideoEncodeAccelerator::Config> {
  static media_m96::VideoPixelFormat input_format(
      const media_m96::VideoEncodeAccelerator::Config& input) {
    return input.input_format;
  }

  static const gfx::Size& input_visible_size(
      const media_m96::VideoEncodeAccelerator::Config& input) {
    return input.input_visible_size;
  }

  static media_m96::VideoCodecProfile output_profile(
      const media_m96::VideoEncodeAccelerator::Config& input) {
    return input.output_profile;
  }

  static const media_m96::Bitrate& bitrate(
      const media_m96::VideoEncodeAccelerator::Config& input) {
    return input.bitrate;
  }

  static uint32_t initial_framerate(
      const media_m96::VideoEncodeAccelerator::Config& input) {
    return input.initial_framerate.value_or(0);
  }

  static bool has_initial_framerate(
      const media_m96::VideoEncodeAccelerator::Config& input) {
    return input.initial_framerate.has_value();
  }

  static uint32_t gop_length(
      const media_m96::VideoEncodeAccelerator::Config& input) {
    return input.gop_length.value_or(0);
  }

  static bool has_gop_length(
      const media_m96::VideoEncodeAccelerator::Config& input) {
    return input.gop_length.has_value();
  }

  static uint8_t h264_output_level(
      const media_m96::VideoEncodeAccelerator::Config& input) {
    return input.h264_output_level.value_or(0);
  }

  static bool has_h264_output_level(
      const media_m96::VideoEncodeAccelerator::Config& input) {
    return input.h264_output_level.has_value();
  }

  static bool is_constrained_h264(
      const media_m96::VideoEncodeAccelerator::Config& input) {
    return input.is_constrained_h264;
  }

  static media_m96::VideoEncodeAccelerator::Config::StorageType storage_type(
      const media_m96::VideoEncodeAccelerator::Config& input) {
    return input.storage_type.value_or(
        media_m96::VideoEncodeAccelerator::Config::StorageType::kShmem);
  }

  static bool has_storage_type(
      const media_m96::VideoEncodeAccelerator::Config& input) {
    return input.storage_type.has_value();
  }

  static media_m96::VideoEncodeAccelerator::Config::ContentType content_type(
      const media_m96::VideoEncodeAccelerator::Config& input) {
    return input.content_type;
  }

  static const std::vector<media_m96::VideoEncodeAccelerator::Config::SpatialLayer>&
  spatial_layers(const media_m96::VideoEncodeAccelerator::Config& input) {
    return input.spatial_layers;
  }

  static media_m96::VideoEncodeAccelerator::Config::InterLayerPredMode
  inter_layer_pred(const media_m96::VideoEncodeAccelerator::Config& input) {
    return input.inter_layer_pred;
  }

  static bool require_low_delay(
      const media_m96::VideoEncodeAccelerator::Config& input) {
    return input.require_low_delay;
  }

  static bool Read(media_m96::mojom::VideoEncodeAcceleratorConfigDataView input,
                   media_m96::VideoEncodeAccelerator::Config* output);
};

}  // namespace mojo

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_MOJO_MOJOM_VIDEO_ENCODE_ACCELERATOR_MOJOM_TRAITS_H_
