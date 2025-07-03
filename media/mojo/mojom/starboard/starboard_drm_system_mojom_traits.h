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
