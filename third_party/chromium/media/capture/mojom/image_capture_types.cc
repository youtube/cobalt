// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/chromium/media/capture/mojom/image_capture_types.h"

namespace mojo {

media_m96::mojom::PhotoStatePtr CreateEmptyPhotoState() {
  media_m96::mojom::PhotoStatePtr photo_capabilities =
      media_m96::mojom::PhotoState::New();
  photo_capabilities->height = media_m96::mojom::Range::New();
  photo_capabilities->width = media_m96::mojom::Range::New();
  photo_capabilities->exposure_compensation = media_m96::mojom::Range::New();
  photo_capabilities->exposure_time = media_m96::mojom::Range::New();
  photo_capabilities->color_temperature = media_m96::mojom::Range::New();
  photo_capabilities->iso = media_m96::mojom::Range::New();
  photo_capabilities->brightness = media_m96::mojom::Range::New();
  photo_capabilities->contrast = media_m96::mojom::Range::New();
  photo_capabilities->saturation = media_m96::mojom::Range::New();
  photo_capabilities->sharpness = media_m96::mojom::Range::New();
  photo_capabilities->pan = media_m96::mojom::Range::New();
  photo_capabilities->tilt = media_m96::mojom::Range::New();
  photo_capabilities->zoom = media_m96::mojom::Range::New();
  photo_capabilities->focus_distance = media_m96::mojom::Range::New();
  photo_capabilities->torch = false;
  photo_capabilities->red_eye_reduction = media_m96::mojom::RedEyeReduction::NEVER;
  return photo_capabilities;
}

}  // namespace mojo