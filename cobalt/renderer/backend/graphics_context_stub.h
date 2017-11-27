// Copyright 2014 Google Inc. All Rights Reserved.
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

#ifndef COBALT_RENDERER_BACKEND_GRAPHICS_CONTEXT_STUB_H_
#define COBALT_RENDERER_BACKEND_GRAPHICS_CONTEXT_STUB_H_

#include "base/memory/ref_counted.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/renderer/backend/graphics_context.h"
#include "cobalt/renderer/backend/render_target_stub.h"

namespace cobalt {
namespace renderer {
namespace backend {

// An implementation of GraphicsContext for the stub graphics system.
// Most methods are simply stubbed out and so this object does very little
// besides fight off compiler errors.
class GraphicsContextStub : public GraphicsContext {
 public:
  explicit GraphicsContextStub(GraphicsSystem* system)
      : GraphicsContext(system) {}

  scoped_refptr<RenderTarget> CreateOffscreenRenderTarget(
      const math::Size& dimensions) override {
    return scoped_refptr<RenderTarget>(new RenderTargetStub(dimensions));
  }

  scoped_array<uint8_t> DownloadPixelDataAsRGBA(
      const scoped_refptr<RenderTarget>& render_target) override {
    // Since we're a stub, just return garbage data of the right size.
    return scoped_array<uint8_t>(
        new uint8_t[render_target->GetSize().GetArea() * 4]);
  }

  void Finish() override {}
};

}  // namespace backend
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_BACKEND_GRAPHICS_CONTEXT_STUB_H_
