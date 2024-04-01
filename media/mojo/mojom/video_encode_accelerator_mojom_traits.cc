// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/video_encode_accelerator_mojom_traits.h"

#include "base/notreached.h"
#include "media/base/video_bitrate_allocation.h"
#include "media/mojo/mojom/video_encode_accelerator.mojom.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace mojo {

// static
bool StructTraits<media::mojom::VideoEncodeAcceleratorSupportedProfileDataView,
                  media::VideoEncodeAccelerator::SupportedProfile>::
    Read(media::mojom::VideoEncodeAcceleratorSupportedProfileDataView data,
         media::VideoEncodeAccelerator::SupportedProfile* out) {
  if (!data.ReadMinResolution(&out->min_resolution) ||
      !data.ReadMaxResolution(&out->max_resolution) ||
      !data.ReadProfile(&out->profile)) {
    return false;
  }

  out->max_framerate_numerator = data.max_framerate_numerator();
  out->max_framerate_denominator = data.max_framerate_denominator();

  std::vector<media::SVCScalabilityMode> scalability_modes;
  if (!data.ReadScalabilityModes(&scalability_modes))
    return false;
  out->scalability_modes = std::move(scalability_modes);

  return true;
}

// static
media::mojom::VideoEncodeAccelerator_Error
EnumTraits<media::mojom::VideoEncodeAccelerator_Error,
           media::VideoEncodeAccelerator::Error>::
    ToMojom(media::VideoEncodeAccelerator::Error error) {
  switch (error) {
    case media::VideoEncodeAccelerator::kIllegalStateError:
      return media::mojom::VideoEncodeAccelerator_Error::ILLEGAL_STATE;
    case media::VideoEncodeAccelerator::kInvalidArgumentError:
      return media::mojom::VideoEncodeAccelerator_Error::INVALID_ARGUMENT;
    case media::VideoEncodeAccelerator::kPlatformFailureError:
      return media::mojom::VideoEncodeAccelerator_Error::PLATFORM_FAILURE;
  }
  NOTREACHED();
  return media::mojom::VideoEncodeAccelerator_Error::INVALID_ARGUMENT;
}

// static
bool EnumTraits<media::mojom::VideoEncodeAccelerator_Error,
                media::VideoEncodeAccelerator::Error>::
    FromMojom(media::mojom::VideoEncodeAccelerator_Error error,
              media::VideoEncodeAccelerator::Error* out) {
  switch (error) {
    case media::mojom::VideoEncodeAccelerator_Error::ILLEGAL_STATE:
      *out = media::VideoEncodeAccelerator::kIllegalStateError;
      return true;
    case media::mojom::VideoEncodeAccelerator_Error::INVALID_ARGUMENT:
      *out = media::VideoEncodeAccelerator::kInvalidArgumentError;
      return true;
    case media::mojom::VideoEncodeAccelerator_Error::PLATFORM_FAILURE:
      *out = media::VideoEncodeAccelerator::kPlatformFailureError;
      return true;
  }
  NOTREACHED();
  return false;
}

// static
std::vector<int32_t> StructTraits<media::mojom::VideoBitrateAllocationDataView,
                                  media::VideoBitrateAllocation>::
    bitrates(const media::VideoBitrateAllocation& bitrate_allocation) {
  std::vector<int32_t> bitrates;
  int sum_bps = 0;
  for (size_t si = 0; si < media::VideoBitrateAllocation::kMaxSpatialLayers;
       ++si) {
    for (size_t ti = 0; ti < media::VideoBitrateAllocation::kMaxTemporalLayers;
         ++ti) {
      if (sum_bps == bitrate_allocation.GetSumBps()) {
        // The rest is all zeros, no need to iterate further.
        return bitrates;
      }
      const int layer_bitrate = bitrate_allocation.GetBitrateBps(si, ti);
      bitrates.emplace_back(layer_bitrate);
      sum_bps += layer_bitrate;
    }
  }
  return bitrates;
}

// static
bool StructTraits<media::mojom::VideoBitrateAllocationDataView,
                  media::VideoBitrateAllocation>::
    Read(media::mojom::VideoBitrateAllocationDataView data,
         media::VideoBitrateAllocation* out_bitrate_allocation) {
  ArrayDataView<int32_t> bitrates;
  data.GetBitratesDataView(&bitrates);
  size_t size = bitrates.size();
  if (size > media::VideoBitrateAllocation::kMaxSpatialLayers *
                 media::VideoBitrateAllocation::kMaxTemporalLayers) {
    return false;
  }
  for (size_t i = 0; i < size; ++i) {
    const int32_t bitrate = bitrates[i];
    const size_t si = i / media::VideoBitrateAllocation::kMaxTemporalLayers;
    const size_t ti = i % media::VideoBitrateAllocation::kMaxTemporalLayers;
    if (!out_bitrate_allocation->SetBitrate(si, ti, bitrate)) {
      return false;
    }
  }
  return true;
}

// static
bool UnionTraits<media::mojom::CodecMetadataDataView,
                 media::BitstreamBufferMetadata>::
    Read(media::mojom::CodecMetadataDataView data,
         media::BitstreamBufferMetadata* out) {
  switch (data.tag()) {
    case media::mojom::CodecMetadataDataView::Tag::H264: {
      return data.ReadH264(&out->h264);
    }
    case media::mojom::CodecMetadataDataView::Tag::VP8: {
      return data.ReadVp8(&out->vp8);
    }
    case media::mojom::CodecMetadataDataView::Tag::VP9: {
      return data.ReadVp9(&out->vp9);
    }
  }
  NOTREACHED();
  return false;
}

// static
bool StructTraits<media::mojom::BitstreamBufferMetadataDataView,
                  media::BitstreamBufferMetadata>::
    Read(media::mojom::BitstreamBufferMetadataDataView data,
         media::BitstreamBufferMetadata* metadata) {
  metadata->payload_size_bytes = data.payload_size_bytes();
  metadata->key_frame = data.key_frame();
  if (!data.ReadTimestamp(&metadata->timestamp)) {
    return false;
  }

  return data.ReadCodecMetadata(metadata);
}

// static
bool StructTraits<media::mojom::H264MetadataDataView, media::H264Metadata>::
    Read(media::mojom::H264MetadataDataView data,
         media::H264Metadata* out_metadata) {
  out_metadata->temporal_idx = data.temporal_idx();
  out_metadata->layer_sync = data.layer_sync();
  return true;
}

// static
bool StructTraits<media::mojom::Vp8MetadataDataView, media::Vp8Metadata>::Read(
    media::mojom::Vp8MetadataDataView data,
    media::Vp8Metadata* out_metadata) {
  out_metadata->non_reference = data.non_reference();
  out_metadata->temporal_idx = data.temporal_idx();
  out_metadata->layer_sync = data.layer_sync();
  return true;
}

// static
bool StructTraits<media::mojom::Vp9MetadataDataView, media::Vp9Metadata>::Read(
    media::mojom::Vp9MetadataDataView data,
    media::Vp9Metadata* out_metadata) {
  out_metadata->inter_pic_predicted = data.inter_pic_predicted();
  out_metadata->temporal_up_switch = data.temporal_up_switch();
  out_metadata->referenced_by_upper_spatial_layers =
      data.referenced_by_upper_spatial_layers();
  out_metadata->reference_lower_spatial_layers =
      data.reference_lower_spatial_layers();
  out_metadata->end_of_picture = data.end_of_picture();
  out_metadata->temporal_idx = data.temporal_idx();
  out_metadata->spatial_idx = data.spatial_idx();
  return data.ReadSpatialLayerResolutions(
             &out_metadata->spatial_layer_resolutions) &&
         data.ReadPDiffs(&out_metadata->p_diffs);
}

// static
media::mojom::VideoEncodeAcceleratorConfig_StorageType
EnumTraits<media::mojom::VideoEncodeAcceleratorConfig_StorageType,
           media::VideoEncodeAccelerator::Config::StorageType>::
    ToMojom(media::VideoEncodeAccelerator::Config::StorageType input) {
  switch (input) {
    case media::VideoEncodeAccelerator::Config::StorageType::kGpuMemoryBuffer:
      return media::mojom::VideoEncodeAcceleratorConfig_StorageType::
          kGpuMemoryBuffer;
    case media::VideoEncodeAccelerator::Config::StorageType::kShmem:
      return media::mojom::VideoEncodeAcceleratorConfig_StorageType::kShmem;
  }
  NOTREACHED();
  return media::mojom::VideoEncodeAcceleratorConfig_StorageType::kShmem;
}

// static
bool EnumTraits<media::mojom::VideoEncodeAcceleratorConfig_StorageType,
                media::VideoEncodeAccelerator::Config::StorageType>::
    FromMojom(media::mojom::VideoEncodeAcceleratorConfig_StorageType input,
              media::VideoEncodeAccelerator::Config::StorageType* output) {
  switch (input) {
    case media::mojom::VideoEncodeAcceleratorConfig_StorageType::kShmem:
      *output = media::VideoEncodeAccelerator::Config::StorageType::kShmem;
      return true;
    case media::mojom::VideoEncodeAcceleratorConfig_StorageType::
        kGpuMemoryBuffer:
      *output =
          media::VideoEncodeAccelerator::Config::StorageType::kGpuMemoryBuffer;
      return true;
  }
  NOTREACHED();
  return false;
}

// static
media::mojom::VideoEncodeAcceleratorConfig_ContentType
EnumTraits<media::mojom::VideoEncodeAcceleratorConfig_ContentType,
           media::VideoEncodeAccelerator::Config::ContentType>::
    ToMojom(media::VideoEncodeAccelerator::Config::ContentType input) {
  switch (input) {
    case media::VideoEncodeAccelerator::Config::ContentType::kDisplay:
      return media::mojom::VideoEncodeAcceleratorConfig_ContentType::kDisplay;
    case media::VideoEncodeAccelerator::Config::ContentType::kCamera:
      return media::mojom::VideoEncodeAcceleratorConfig_ContentType::kCamera;
  }
  NOTREACHED();
  return media::mojom::VideoEncodeAcceleratorConfig_ContentType::kCamera;
}

// static
bool EnumTraits<media::mojom::VideoEncodeAcceleratorConfig_ContentType,
                media::VideoEncodeAccelerator::Config::ContentType>::
    FromMojom(media::mojom::VideoEncodeAcceleratorConfig_ContentType input,
              media::VideoEncodeAccelerator::Config::ContentType* output) {
  switch (input) {
    case media::mojom::VideoEncodeAcceleratorConfig_ContentType::kCamera:
      *output = media::VideoEncodeAccelerator::Config::ContentType::kCamera;
      return true;
    case media::mojom::VideoEncodeAcceleratorConfig_ContentType::kDisplay:
      *output = media::VideoEncodeAccelerator::Config::ContentType::kDisplay;
      return true;
  }
  NOTREACHED();
  return false;
}

// static
media::mojom::VideoEncodeAcceleratorConfig_InterLayerPredMode
EnumTraits<media::mojom::VideoEncodeAcceleratorConfig_InterLayerPredMode,
           media::VideoEncodeAccelerator::Config::InterLayerPredMode>::
    ToMojom(media::VideoEncodeAccelerator::Config::InterLayerPredMode input) {
  switch (input) {
    case media::VideoEncodeAccelerator::Config::InterLayerPredMode::kOff:
      return media::mojom::VideoEncodeAcceleratorConfig_InterLayerPredMode::
          kOff;
    case media::VideoEncodeAccelerator::Config::InterLayerPredMode::kOn:
      return media::mojom::VideoEncodeAcceleratorConfig_InterLayerPredMode::kOn;
    case media::VideoEncodeAccelerator::Config::InterLayerPredMode::kOnKeyPic:
      return media::mojom::VideoEncodeAcceleratorConfig_InterLayerPredMode::
          kOnKeyPic;
  }
  NOTREACHED();
  return media::mojom::VideoEncodeAcceleratorConfig_InterLayerPredMode::kOff;
}

// static
bool EnumTraits<media::mojom::VideoEncodeAcceleratorConfig_InterLayerPredMode,
                media::VideoEncodeAccelerator::Config::InterLayerPredMode>::
    FromMojom(
        media::mojom::VideoEncodeAcceleratorConfig_InterLayerPredMode input,
        media::VideoEncodeAccelerator::Config::InterLayerPredMode* output) {
  switch (input) {
    case media::mojom::VideoEncodeAcceleratorConfig_InterLayerPredMode::kOff:
      *output = media::VideoEncodeAccelerator::Config::InterLayerPredMode::kOff;
      return true;
    case media::mojom::VideoEncodeAcceleratorConfig_InterLayerPredMode::kOn:
      *output = media::VideoEncodeAccelerator::Config::InterLayerPredMode::kOn;
      return true;
    case media::mojom::VideoEncodeAcceleratorConfig_InterLayerPredMode::
        kOnKeyPic:
      *output =
          media::VideoEncodeAccelerator::Config::InterLayerPredMode::kOnKeyPic;
      return true;
  }
  NOTREACHED();
  return false;
}

// static
bool StructTraits<media::mojom::SpatialLayerDataView,
                  media::VideoEncodeAccelerator::Config::SpatialLayer>::
    Read(media::mojom::SpatialLayerDataView input,
         media::VideoEncodeAccelerator::Config::SpatialLayer* output) {
  output->width = input.width();
  output->height = input.height();
  output->bitrate_bps = input.bitrate_bps();
  output->framerate = input.framerate();
  output->max_qp = input.max_qp();
  output->num_of_temporal_layers = input.num_of_temporal_layers();
  return true;
}

// static
media::mojom::Bitrate_Mode
EnumTraits<media::mojom::Bitrate_Mode, media::Bitrate::Mode>::ToMojom(
    media::Bitrate::Mode input) {
  switch (input) {
    case media::Bitrate::Mode::kConstant:
      return media::mojom::Bitrate_Mode::kConstant;
    case media::Bitrate::Mode::kVariable:
      return media::mojom::Bitrate_Mode::kVariable;
  }
  NOTREACHED();
  return media::mojom::Bitrate_Mode::kConstant;
}

// static
bool EnumTraits<media::mojom::Bitrate_Mode, media::Bitrate::Mode>::FromMojom(
    media::mojom::Bitrate_Mode input,
    media::Bitrate::Mode* output) {
  switch (input) {
    case media::mojom::Bitrate_Mode::kConstant:
      *output = media::Bitrate::Mode::kConstant;
      return true;
    case media::mojom::Bitrate_Mode::kVariable:
      *output = media::Bitrate::Mode::kVariable;
      return true;
  }
  NOTREACHED();
  return false;
}

// static
bool StructTraits<media::mojom::BitrateDataView, media::Bitrate>::Read(
    media::mojom::BitrateDataView input,
    media::Bitrate* output) {
  switch (input.mode()) {
    case media::mojom::Bitrate_Mode::kConstant:
      if (input.peak() != 0u)
        return false;
      *output = media::Bitrate::ConstantBitrate(input.target());
      return true;
    case media::mojom::Bitrate_Mode::kVariable:
      *output = media::Bitrate::VariableBitrate(input.target(), input.peak());
      return true;
  }

  NOTREACHED();
  return false;
}

// static
bool StructTraits<media::mojom::VideoEncodeAcceleratorConfigDataView,
                  media::VideoEncodeAccelerator::Config>::
    Read(media::mojom::VideoEncodeAcceleratorConfigDataView input,
         media::VideoEncodeAccelerator::Config* output) {
  media::VideoPixelFormat input_format;
  if (!input.ReadInputFormat(&input_format))
    return false;

  gfx::Size input_visible_size;
  if (!input.ReadInputVisibleSize(&input_visible_size))
    return false;

  media::VideoCodecProfile output_profile;
  if (!input.ReadOutputProfile(&output_profile))
    return false;

  media::Bitrate bitrate;
  if (!input.ReadBitrate(&bitrate))
    return false;
  if (bitrate.mode() == media::Bitrate::Mode::kConstant &&
      bitrate.peak() != 0u) {
    return false;
  }

  absl::optional<uint32_t> initial_framerate;
  if (input.has_initial_framerate())
    initial_framerate = input.initial_framerate();

  absl::optional<uint32_t> gop_length;
  if (input.has_gop_length())
    gop_length = input.gop_length();

  absl::optional<uint8_t> h264_output_level;
  if (input.has_h264_output_level())
    h264_output_level = input.h264_output_level();

  bool is_constrained_h264 = input.is_constrained_h264();

  absl::optional<media::VideoEncodeAccelerator::Config::StorageType>
      storage_type;
  if (input.has_storage_type()) {
    if (!input.ReadStorageType(&storage_type))
      return false;
  }

  media::VideoEncodeAccelerator::Config::ContentType content_type;
  if (!input.ReadContentType(&content_type))
    return false;

  std::vector<media::VideoEncodeAccelerator::Config::SpatialLayer>
      spatial_layers;
  if (!input.ReadSpatialLayers(&spatial_layers))
    return false;

  media::VideoEncodeAccelerator::Config::InterLayerPredMode inter_layer_pred;
  if (!input.ReadInterLayerPred(&inter_layer_pred))
    return false;

  *output = media::VideoEncodeAccelerator::Config(
      input_format, input_visible_size, output_profile, bitrate,
      initial_framerate, gop_length, h264_output_level, is_constrained_h264,
      storage_type, content_type, spatial_layers, inter_layer_pred);

  output->require_low_delay = input.require_low_delay();

  return true;
}

}  // namespace mojo
