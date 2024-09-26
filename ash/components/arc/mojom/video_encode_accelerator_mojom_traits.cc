// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/mojom/video_encode_accelerator_mojom_traits.h"

#include "ash/components/arc/mojom/video_accelerator_mojom_traits.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace mojo {

// Make sure values in arc::mojom::VideoEncodeAccelerator::Error and
// media::VideoEncodeAccelerator::Error match.
#define CHECK_ERROR_ENUM(value)                                            \
  static_assert(                                                           \
      static_cast<int>(arc::mojom::VideoEncodeAccelerator_Error::value) == \
          media::VideoEncodeAccelerator::Error::value,                     \
      "enum ##value mismatch")

CHECK_ERROR_ENUM(kIllegalStateError);
CHECK_ERROR_ENUM(kInvalidArgumentError);
CHECK_ERROR_ENUM(kPlatformFailureError);
CHECK_ERROR_ENUM(kErrorMax);

#undef CHECK_ERROR_ENUM

// static
arc::mojom::VideoFrameStorageType
EnumTraits<arc::mojom::VideoFrameStorageType,
           media::VideoEncodeAccelerator::Config::StorageType>::
    ToMojom(media::VideoEncodeAccelerator::Config::StorageType input) {
  NOTIMPLEMENTED();
  return arc::mojom::VideoFrameStorageType::SHMEM;
}

bool EnumTraits<arc::mojom::VideoFrameStorageType,
                media::VideoEncodeAccelerator::Config::StorageType>::
    FromMojom(arc::mojom::VideoFrameStorageType input,
              media::VideoEncodeAccelerator::Config::StorageType* output) {
  switch (input) {
    case arc::mojom::VideoFrameStorageType::SHMEM:
      *output = media::VideoEncodeAccelerator::Config::StorageType::kShmem;
      return true;
    case arc::mojom::VideoFrameStorageType::DMABUF:
      *output =
          media::VideoEncodeAccelerator::Config::StorageType::kGpuMemoryBuffer;
      return true;
  }
  return false;
}

// static
arc::mojom::VideoEncodeAccelerator_Error
EnumTraits<arc::mojom::VideoEncodeAccelerator_Error,
           media::VideoEncodeAccelerator::Error>::
    ToMojom(media::VideoEncodeAccelerator::Error input) {
  return static_cast<arc::mojom::VideoEncodeAccelerator_Error>(input);
}

// static
bool EnumTraits<arc::mojom::VideoEncodeAccelerator_Error,
                media::VideoEncodeAccelerator::Error>::
    FromMojom(arc::mojom::VideoEncodeAccelerator_Error input,
              media::VideoEncodeAccelerator::Error* output) {
  NOTIMPLEMENTED();
  return false;
}

// static
uint32_t
StructTraits<arc::mojom::ConstantBitrateDataView, arc::mojom::ConstantBitrate>::
    target(const arc::mojom::ConstantBitrate& input) {
  return input.target;
}

// static
uint32_t
StructTraits<arc::mojom::VariableBitrateDataView, arc::mojom::VariableBitrate>::
    target(const arc::mojom::VariableBitrate& input) {
  return input.target;
}

// static
uint32_t
StructTraits<arc::mojom::VariableBitrateDataView, arc::mojom::VariableBitrate>::
    peak(const arc::mojom::VariableBitrate& input) {
  return input.peak;
}

// static
arc::mojom::BitrateDataView::Tag
UnionTraits<arc::mojom::BitrateDataView, media::Bitrate>::GetTag(
    const media::Bitrate& input) {
  switch (input.mode()) {
    case media::Bitrate::Mode::kConstant:
      return arc::mojom::BitrateDataView::Tag::kConstant;
    case media::Bitrate::Mode::kVariable:
      return arc::mojom::BitrateDataView::Tag::kVariable;
    case media::Bitrate::Mode::kExternal:
      // Ash encoder doesn't need to support external rate control.
      NOTREACHED();
      return arc::mojom::BitrateDataView::Tag::kConstant;
  }
  NOTREACHED();
  return arc::mojom::BitrateDataView::Tag::kConstant;
}

// static
arc::mojom::ConstantBitrate
UnionTraits<arc::mojom::BitrateDataView, media::Bitrate>::constant(
    const media::Bitrate& input) {
  arc::mojom::ConstantBitrate constant_bitrate;
  constant_bitrate.target = input.target_bps();
  return constant_bitrate;
}

// static
arc::mojom::VariableBitrate
UnionTraits<arc::mojom::BitrateDataView, media::Bitrate>::variable(
    const media::Bitrate& input) {
  arc::mojom::VariableBitrate variable_bitrate;
  variable_bitrate.target = input.target_bps();
  variable_bitrate.peak = input.peak_bps();
  return variable_bitrate;
}

// static
bool UnionTraits<arc::mojom::BitrateDataView, media::Bitrate>::Read(
    arc::mojom::BitrateDataView input,
    media::Bitrate* output) {
  switch (input.tag()) {
    case arc::mojom::BitrateDataView::Tag::kConstant: {
      arc::mojom::ConstantBitrateDataView constant_bitrate;
      input.GetConstantDataView(&constant_bitrate);
      *output = media::Bitrate::ConstantBitrate(constant_bitrate.target());
      return true;
    }
    case arc::mojom::BitrateDataView::Tag::kVariable: {
      arc::mojom::VariableBitrateDataView variable_bitrate;
      input.GetVariableDataView(&variable_bitrate);
      *output = media::Bitrate::VariableBitrate(variable_bitrate.target(),
                                                variable_bitrate.peak());
      return true;
    }
    default:
      NOTREACHED();
      return false;
  }
}

// static
bool StructTraits<arc::mojom::VideoEncodeAcceleratorConfigDataView,
                  media::VideoEncodeAccelerator::Config>::
    Read(arc::mojom::VideoEncodeAcceleratorConfigDataView input,
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

  absl::optional<uint32_t> initial_framerate;
  if (input.has_initial_framerate()) {
    initial_framerate = input.initial_framerate();
  }

  absl::optional<uint8_t> h264_output_level;
  if (input.has_h264_output_level()) {
    h264_output_level = input.h264_output_level();
  }

  media::VideoEncodeAccelerator::Config::StorageType storage_type;
  if (!input.ReadStorageType(&storage_type))
    return false;

  absl::optional<media::Bitrate> bitrate;
  if (!input.ReadBitrate(&bitrate))
    return false;
  if (bitrate.has_value()) {
    DCHECK((bitrate->mode() == media::Bitrate::Mode::kVariable) ||
           (bitrate->peak_bps() == 0u));
    DCHECK((bitrate->mode() == media::Bitrate::Mode::kConstant) ||
           (bitrate->peak_bps() >= bitrate->target_bps()));
  } else {
    bitrate =
        media::Bitrate::ConstantBitrate(input.initial_bitrate_deprecated());
  }

  *output = media::VideoEncodeAccelerator::Config(
      input_format, input_visible_size, output_profile, *bitrate,
      initial_framerate, absl::nullopt, h264_output_level, false, storage_type);
  return true;
}

}  // namespace mojo
