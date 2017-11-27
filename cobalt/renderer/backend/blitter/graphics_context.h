// Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef COBALT_RENDERER_BACKEND_BLITTER_GRAPHICS_CONTEXT_H_
#define COBALT_RENDERER_BACKEND_BLITTER_GRAPHICS_CONTEXT_H_

#include "base/memory/ref_counted.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/renderer/backend/blitter/render_target.h"
#include "cobalt/renderer/backend/graphics_context.h"
#include "starboard/blitter.h"

#if SB_HAS(BLITTER)

namespace cobalt {
namespace renderer {
namespace backend {

class GraphicsContextBlitter : public GraphicsContext {
 public:
  explicit GraphicsContextBlitter(GraphicsSystem* system);
  ~GraphicsContextBlitter() override;

  scoped_refptr<RenderTarget> CreateOffscreenRenderTarget(
      const math::Size& dimensions) override;
  scoped_array<uint8_t> DownloadPixelDataAsRGBA(
      const scoped_refptr<RenderTarget>& render_target) override;
  void Finish() override;

  SbBlitterContext GetSbBlitterContext() const { return context_; }
  SbBlitterDevice GetSbBlitterDevice() const { return device_; }

 private:
  SbBlitterDevice device_;
  SbBlitterContext context_;
};

}  // namespace backend
}  // namespace renderer
}  // namespace cobalt

#endif  // #if SB_HAS(BLITTER)

#endif  // COBALT_RENDERER_BACKEND_BLITTER_GRAPHICS_CONTEXT_H_
