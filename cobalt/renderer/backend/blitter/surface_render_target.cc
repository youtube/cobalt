/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/renderer/backend/blitter/surface_render_target.h"

#include "starboard/blitter.h"

#if SB_HAS(BLITTER)

namespace cobalt {
namespace renderer {
namespace backend {

SurfaceRenderTargetBlitter::SurfaceRenderTargetBlitter(
    SbBlitterDevice device, const math::Size& dimensions) {
  surface_ = SbBlitterCreateRenderTargetSurface(device, dimensions.width(),
                                                dimensions.height(),
                                                kSbBlitterSurfaceFormatRGBA8);
  CHECK(SbBlitterIsSurfaceValid(surface_));

  render_target_ = SbBlitterGetRenderTargetFromSurface(surface_);
  CHECK(SbBlitterIsRenderTargetValid(render_target_));

  size_ = dimensions;
}

SurfaceRenderTargetBlitter::~SurfaceRenderTargetBlitter() {
  SbBlitterDestroySurface(surface_);
}

const math::Size& SurfaceRenderTargetBlitter::GetSize() { return size_; }

SbBlitterRenderTarget SurfaceRenderTargetBlitter::GetSbRenderTarget() const {
  return render_target_;
}

}  // namespace backend
}  // namespace renderer
}  // namespace cobalt

#endif  // #if SB_HAS(BLITTER)
