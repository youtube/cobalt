// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef GPU_COMMAND_BUFFER_COMMON_IN_PROCESS_RASTER_PAYLOAD_H_
#define GPU_COMMAND_BUFFER_COMMON_IN_PROCESS_RASTER_PAYLOAD_H_

#include <stdint.h>
#include <optional>

#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "cc/paint/display_item_list.h"
#include "cc/paint/paint_image.h"
#include "cc/paint/scroll_offset_map.h"
#include "gpu/gpu_export.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace gpu {
namespace raster {

// Encapsulates the parameters and display list for direct in-process
// rasterization. When running in single-process mode, this payload can be
// passed directly from RasterImplementation (client) to RasterDecoder
// (service), completely bypassing PaintOp serialization and deserialization.
struct GPU_EXPORT InProcessRasterPayload {
  InProcessRasterPayload();
  ~InProcessRasterPayload();
  InProcessRasterPayload(InProcessRasterPayload&& other);
  InProcessRasterPayload& operator=(InProcessRasterPayload&& other);

  // The DisplayItemList is finalized and immutable before raster, so it's safe to
  // send it directly to the GPU thread.
  scoped_refptr<const cc::DisplayItemList> display_item_list;
  gfx::Size content_size;
  gfx::Rect full_raster_rect;
  gfx::Rect playback_rect;
  gfx::Vector2dF post_translate;
  gfx::Vector2dF post_scale;
  bool requires_clear = false;
  SkColor4f background_color = SkColors::kTransparent;

  // Dynamic scroll offsets required by DrawScrollingContentsOp.
  std::optional<cc::ScrollOffsetMap> raster_inducing_scroll_offsets;

  // Maps each PaintImage to its pre-uploaded GPU transfer cache entry ID.
  base::flat_map<cc::PaintImage::Id, uint32_t> image_to_transfer_cache_id;
};

// Thread-safe registry for in-process raster payloads. Validates ownership
// before dereferencing on the GPU thread and safely cleans up in-flight
// payloads during context teardown.
class GPU_EXPORT InProcessRasterPayloadRegistry {
 public:
  static InProcessRasterPayloadRegistry& GetInstance();

  InProcessRasterPayloadRegistry(
      const InProcessRasterPayloadRegistry&) = delete;
  InProcessRasterPayloadRegistry& operator=(
      const InProcessRasterPayloadRegistry&) = delete;

  void Register(InProcessRasterPayload* payload);
  bool Take(InProcessRasterPayload* payload);
  void Clear();

 private:
  friend class base::NoDestructor<InProcessRasterPayloadRegistry>;

  InProcessRasterPayloadRegistry();
  ~InProcessRasterPayloadRegistry();

  base::Lock lock_;
  base::circular_deque<InProcessRasterPayload*> entries_ GUARDED_BY(lock_);
};

}  // namespace raster
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_IN_PROCESS_RASTER_PAYLOAD_H_
