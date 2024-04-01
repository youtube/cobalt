// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_MOJOM_VIDEO_FRAME_METADATA_MOJOM_TRAITS_H_
#define MEDIA_MOJO_MOJOM_VIDEO_FRAME_METADATA_MOJOM_TRAITS_H_

#include "base/memory/ref_counted.h"
#include "media/base/ipc/media_param_traits_macros.h"
#include "media/base/video_frame_metadata.h"
#include "media/base/video_transformation.h"
#include "media/mojo/mojom/media_types.mojom-shared.h"
#include "media/mojo/mojom/media_types_enum_mojom_traits.h"
#include "media/mojo/mojom/video_transformation_mojom_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"

namespace mojo {

// Creates a has_foo() and a foo() to serialize a foo absl::optional<>.
#define GENERATE_OPT_SERIALIZATION(type, field, default_value)      \
  static bool has_##field(const media::VideoFrameMetadata& input) { \
    return input.field.has_value();                                 \
  }                                                                 \
                                                                    \
  static type field(const media::VideoFrameMetadata& input) {       \
    return input.field.value_or(default_value);                     \
  }

template <>
struct StructTraits<media::mojom::VideoFrameMetadataDataView,
                    media::VideoFrameMetadata> {
  static bool allow_overlay(const media::VideoFrameMetadata& input) {
    return input.allow_overlay;
  }

  static bool end_of_stream(const media::VideoFrameMetadata& input) {
    return input.end_of_stream;
  }

  static bool texture_owner(const media::VideoFrameMetadata& input) {
    return input.texture_owner;
  }

  static bool wants_promotion_hint(const media::VideoFrameMetadata& input) {
    return input.wants_promotion_hint;
  }

  static bool protected_video(const media::VideoFrameMetadata& input) {
    return input.protected_video;
  }

  static bool hw_protected(const media::VideoFrameMetadata& input) {
    return input.hw_protected;
  }

  static uint32_t hw_protected_validation_id(
      const media::VideoFrameMetadata& input) {
    return input.hw_protected_validation_id;
  }

  static bool power_efficient(const media::VideoFrameMetadata& input) {
    return input.power_efficient;
  }

  static bool read_lock_fences_enabled(const media::VideoFrameMetadata& input) {
    return input.read_lock_fences_enabled;
  }

  static bool interactive_content(const media::VideoFrameMetadata& input) {
    return input.interactive_content;
  }

  static bool texture_origin_is_top_left(
      const media::VideoFrameMetadata& input) {
    return input.texture_origin_is_top_left;
  }

  GENERATE_OPT_SERIALIZATION(int, capture_counter, 0)

  GENERATE_OPT_SERIALIZATION(
      media::VideoFrameMetadata::CopyMode,
      copy_mode,
      media::VideoFrameMetadata::CopyMode::kCopyToNewTexture)

  static absl::optional<media::VideoTransformation> transformation(
      const media::VideoFrameMetadata& input) {
    return input.transformation;
  }

  GENERATE_OPT_SERIALIZATION(double, device_scale_factor, 0.0)
  GENERATE_OPT_SERIALIZATION(double, page_scale_factor, 0.0)
  GENERATE_OPT_SERIALIZATION(double, root_scroll_offset_x, 0.0)
  GENERATE_OPT_SERIALIZATION(double, root_scroll_offset_y, 0.0)
  GENERATE_OPT_SERIALIZATION(double, top_controls_visible_height, 0.0)
  GENERATE_OPT_SERIALIZATION(double, frame_rate, 0.0)
  GENERATE_OPT_SERIALIZATION(double, rtp_timestamp, 0.0)

  static absl::optional<gfx::Rect> capture_update_rect(
      const media::VideoFrameMetadata& input) {
    return input.capture_update_rect;
  }

  static absl::optional<base::UnguessableToken> overlay_plane_id(
      const media::VideoFrameMetadata& input) {
    return input.overlay_plane_id;
  }

  static absl::optional<base::TimeTicks> receive_time(
      const media::VideoFrameMetadata& input) {
    return input.receive_time;
  }

  static absl::optional<base::TimeTicks> capture_begin_time(
      const media::VideoFrameMetadata& input) {
    return input.capture_begin_time;
  }

  static absl::optional<base::TimeTicks> capture_end_time(
      const media::VideoFrameMetadata& input) {
    return input.capture_end_time;
  }

  static absl::optional<base::TimeTicks> decode_begin_time(
      const media::VideoFrameMetadata& input) {
    return input.decode_begin_time;
  }

  static absl::optional<base::TimeTicks> decode_end_time(
      const media::VideoFrameMetadata& input) {
    return input.decode_end_time;
  }

  static absl::optional<base::TimeTicks> reference_time(
      const media::VideoFrameMetadata& input) {
    return input.reference_time;
  }

  static absl::optional<base::TimeDelta> processing_time(
      const media::VideoFrameMetadata& input) {
    return input.processing_time;
  }

  static absl::optional<base::TimeDelta> frame_duration(
      const media::VideoFrameMetadata& input) {
    return input.frame_duration;
  }

  static absl::optional<base::TimeDelta> wallclock_frame_duration(
      const media::VideoFrameMetadata& input) {
    return input.wallclock_frame_duration;
  }

  static bool Read(media::mojom::VideoFrameMetadataDataView input,
                   media::VideoFrameMetadata* output);
};

}  // namespace mojo

#endif  // MEDIA_MOJO_MOJOM_VIDEO_FRAME_METADATA_MOJOM_TRAITS_H_
