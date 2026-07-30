// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_MOJOM_VIDEO_SPATIAL_FORMAT_MOJOM_TRAITS_H_
#define MEDIA_MOJO_MOJOM_VIDEO_SPATIAL_FORMAT_MOJOM_TRAITS_H_

#include "base/notreached.h"
#include "media/base/ipc/media_param_traits.h"
#include "media/base/video_spatial_format.h"
#include "media/mojo/mojom/media_types.mojom-shared.h"

namespace mojo {

template <>
struct EnumTraits<media::mojom::VideoStereoMode, ::media::VideoStereoMode> {
  static media::mojom::VideoStereoMode ToMojom(::media::VideoStereoMode input) {
    switch (input) {
      case ::media::VideoStereoMode::kMono:
        return media::mojom::VideoStereoMode::kMono;
      case ::media::VideoStereoMode::kSideBySideLeftFirst:
        return media::mojom::VideoStereoMode::kSideBySideLeftFirst;
      case ::media::VideoStereoMode::kTopBottomLeftFirst:
        return media::mojom::VideoStereoMode::kTopBottomLeftFirst;
    }

    NOTREACHED();
  }

  static ::media::VideoStereoMode FromMojom(
      media::mojom::VideoStereoMode input) {
    switch (input) {
      case media::mojom::VideoStereoMode::kMono:
        return ::media::VideoStereoMode::kMono;
      case media::mojom::VideoStereoMode::kSideBySideLeftFirst:
        return ::media::VideoStereoMode::kSideBySideLeftFirst;
      case media::mojom::VideoStereoMode::kTopBottomLeftFirst:
        return ::media::VideoStereoMode::kTopBottomLeftFirst;
    }

    NOTREACHED();
  }
};

template <>
struct EnumTraits<media::mojom::VideoProjectionType,
                  ::media::VideoProjectionType> {
  static media::mojom::VideoProjectionType ToMojom(
      ::media::VideoProjectionType input) {
    switch (input) {
      case ::media::VideoProjectionType::kNone:
        return media::mojom::VideoProjectionType::kNone;
      case ::media::VideoProjectionType::kEquirect180:
        return media::mojom::VideoProjectionType::kEquirect180;
      case ::media::VideoProjectionType::kEquirect360:
        return media::mojom::VideoProjectionType::kEquirect360;
    }

    NOTREACHED();
  }

  static ::media::VideoProjectionType FromMojom(
      media::mojom::VideoProjectionType input) {
    switch (input) {
      case media::mojom::VideoProjectionType::kNone:
        return ::media::VideoProjectionType::kNone;
      case media::mojom::VideoProjectionType::kEquirect180:
        return ::media::VideoProjectionType::kEquirect180;
      case media::mojom::VideoProjectionType::kEquirect360:
        return ::media::VideoProjectionType::kEquirect360;
    }

    NOTREACHED();
  }
};

template <>
struct StructTraits<media::mojom::VideoSpatialFormatDataView,
                    media::VideoSpatialFormat> {
  static media::VideoProjectionType projection_type(
      const media::VideoSpatialFormat& input) {
    return input.projection_type;
  }

  static media::VideoStereoMode stereo_mode(
      const media::VideoSpatialFormat& input) {
    return input.stereo_mode;
  }

  static bool Read(media::mojom::VideoSpatialFormatDataView input,
                   media::VideoSpatialFormat* output) {
    return input.ReadProjectionType(&output->projection_type) &&
           input.ReadStereoMode(&output->stereo_mode);
  }
};

}  // namespace mojo

#endif  // MEDIA_MOJO_MOJOM_VIDEO_SPATIAL_FORMAT_MOJOM_TRAITS_H_
