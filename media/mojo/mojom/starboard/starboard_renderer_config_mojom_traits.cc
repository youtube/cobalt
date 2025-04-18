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

#include "media/mojo/mojom/starboard/starboard_renderer_config_mojom_traits.h"

#include "base/time/time.h"

namespace mojo {

// static
bool StructTraits<media::mojom::StarboardRendererConfigDataView,
                  media::StarboardRendererConfig>::
    Read(media::mojom::StarboardRendererConfigDataView input,
         media::StarboardRendererConfig* output) {
  base::UnguessableToken overlay_plane_id;
  if (!input.ReadOverlayPlaneId(&overlay_plane_id)) {
    return false;
  }

  base::TimeDelta audio_write_duration_local;
  if (!input.ReadAudioWriteDurationLocal(&audio_write_duration_local)) {
    return false;
  }

  base::TimeDelta audio_write_duration_remote;
  if (!input.ReadAudioWriteDurationRemote(&audio_write_duration_remote)) {
    return false;
  }

  output->Initialize(overlay_plane_id, audio_write_duration_local,
                     audio_write_duration_remote);

  return true;
}

}  // namespace mojo
