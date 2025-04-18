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

#ifndef MEDIA_MOJO_MOJOM_STARBOARD_STARBOARD_RENDERER_CONFIG_MOJOM_TRAITS_H_
#define MEDIA_MOJO_MOJOM_STARBOARD_STARBOARD_RENDERER_CONFIG_MOJOM_TRAITS_H_

#include "base/unguessable_token.h"
#include "media/base/video_decoder_config.h"
#include "media/mojo/mojom/starboard/starboard_renderer_config.mojom.h"

namespace mojo {

template <>
struct StructTraits<media::mojom::StarboardRendererConfigDataView,
                    media::StarboardRendererConfig> {
  static const base::UnguessableToken& overlay_plane_id(
      const media::StarboardRendererConfig& input) {
    return input.overlay_plane_id();
  }

  static base::TimeDelta audio_write_duration_local(
      const media::StarboardRendererConfig& input) {
    return input.audio_write_duration_local();
  }

  static base::TimeDelta audio_write_duration_remote(
      const media::StarboardRendererConfig& input) {
    return input.audio_write_duration_remote();
  }

  static bool Read(media::mojom::StarboardRendererConfigDataView input,
                   media::StarboardRendererConfig* output);
};

}  // namespace mojo

#endif  // MEDIA_MOJO_MOJOM_STARBOARD_STARBOARD_RENDERER_CONFIG_MOJOM_TRAITS_H_
