// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_MOJO_MOJOM_VIDEO_TRANSFORMATION_MOJOM_TRAITS_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_MOJO_MOJOM_VIDEO_TRANSFORMATION_MOJOM_TRAITS_H_

#include "third_party/chromium/media/base/ipc/media_param_traits.h"
#include "third_party/chromium/media/base/video_transformation.h"
#include "third_party/chromium/media/mojo/mojom/media_types.mojom-shared.h"
#include "third_party/chromium/media/mojo/mojom/media_types_enum_mojom_traits.h"

namespace mojo {

template <>
struct StructTraits<media_m96::mojom::VideoTransformationDataView,
                    media_m96::VideoTransformation> {
  static media_m96::VideoRotation rotation(
      const media_m96::VideoTransformation& input) {
    return input.rotation;
  }

  static bool mirrored(const media_m96::VideoTransformation& input) {
    return input.mirrored;
  }

  static bool Read(media_m96::mojom::VideoTransformationDataView input,
                   media_m96::VideoTransformation* output);
};

}  // namespace mojo

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_MOJO_MOJOM_VIDEO_TRANSFORMATION_MOJOM_TRAITS_H_
