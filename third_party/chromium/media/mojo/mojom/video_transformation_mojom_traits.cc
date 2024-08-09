// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/chromium/media/mojo/mojom/video_transformation_mojom_traits.h"

#include "third_party/chromium/media/mojo/mojom/media_types.mojom.h"

namespace mojo {

// static
bool StructTraits<media_m96::mojom::VideoTransformationDataView,
                  media_m96::VideoTransformation>::
    Read(media_m96::mojom::VideoTransformationDataView input,
         media_m96::VideoTransformation* output) {
  if (!input.ReadRotation(&output->rotation))
    return false;

  output->mirrored = input.mirrored();

  return true;
}

}  // namespace mojo
