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

#ifndef MEDIA_MOJO_MOJOM_STARBOARD_STARBOARD_DRM_SYSTEM_MOJOM_TRAITS_H_
#define MEDIA_MOJO_MOJOM_STARBOARD_STARBOARD_DRM_SYSTEM_MOJOM_TRAITS_H_

#include "media/mojo/mojom/starboard/starboard_media_types.mojom.h"
#include "starboard/drm.h"

namespace mojo {

template <>
struct StructTraits<media::mojom::SbDrmSystemDataView, SbDrmSystem> {
  static uint64_t ptr_value(SbDrmSystem drm_system) {
    return reinterpret_cast<uint64_t>(drm_system);
  }

  static bool Read(media::mojom::SbDrmSystemDataView data, SbDrmSystem* out) {
    *out = reinterpret_cast<SbDrmSystem>(data.ptr_value());
    return true;
  }
};

}  // namespace mojo

#endif  // MEDIA_MOJO_MOJOM_STARBOARD_STARBOARD_DRM_SYSTEM_MOJOM_TRAITS_H_
