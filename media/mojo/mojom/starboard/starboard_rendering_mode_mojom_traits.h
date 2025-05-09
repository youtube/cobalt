// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#ifndef MEDIA_MOJO_MOJOM_STARBOARD_STARBOARD_RENDERING_MODE_MOJOM_TRAITS_H_
#define MEDIA_MOJO_MOJOM_STARBOARD_STARBOARD_RENDERING_MODE_MOJOM_TRAITS_H_

#include "media/base/starboard/starboard_rendering_mode.h"
#include "media/mojo/mojom/renderer_extensions.mojom-shared.h"

template <>
struct mojo::EnumTraits<media::mojom::StarboardRenderingMode,
                        media::StarboardRenderingMode> {
 public:
  static bool FromMojom(media::mojom::StarboardRenderingMode data,
                        media::StarboardRenderingMode* output) {
    switch (data) {
      case media::mojom::StarboardRenderingMode::kDecodeToTexture:
        *output = media::StarboardRenderingMode::kDecodeToTexture;
        return true;
      case media::mojom::StarboardRenderingMode::kPunchOut:
        *output = media::StarboardRenderingMode::kPunchOut;
        return true;
      case media::mojom::StarboardRenderingMode::kInvalid:
        *output = media::StarboardRenderingMode::kInvalid;
        return true;
    }
    NOTREACHED();
    return false;
  }

  static media::mojom::StarboardRenderingMode ToMojom(
      media::StarboardRenderingMode data) {
    switch (data) {
      case media::StarboardRenderingMode::kDecodeToTexture:
        return media::mojom::StarboardRenderingMode::kDecodeToTexture;
      case media::StarboardRenderingMode::kPunchOut:
        return media::mojom::StarboardRenderingMode::kPunchOut;
        break;
      case media::StarboardRenderingMode::kInvalid:
        return media::mojom::StarboardRenderingMode::kInvalid;
        break;
    }
    NOTREACHED();
    return media::mojom::StarboardRenderingMode::kInvalid;
  }
};

#endif  // MEDIA_MOJO_MOJOM_STARBOARD_STARBOARD_RENDERING_MODE_MOJOM_TRAITS_H_
