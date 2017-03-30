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

#ifndef COBALT_RENDERER_BACKEND_GRAPHICS_CONTEXT_H_
#define COBALT_RENDERER_BACKEND_GRAPHICS_CONTEXT_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/math/size.h"
#include "cobalt/renderer/backend/render_target.h"

namespace cobalt {
namespace renderer {
namespace backend {

class GraphicsSystem;

// The GraphicsContext captures the concept of a data channel to the GPU.
// The above definition implies that all graphics commands must eventually
// be issued through a graphics context.  Platform-specific graphics context
// objects must be acquired from a platform's concrete GraphicsContext object
// and then issued to that directly. A graphics context will always be
// associated with a render target on which all rendering output will appear.
class GraphicsContext {
 public:
  explicit GraphicsContext(GraphicsSystem* system) : system_(system) {}
  virtual ~GraphicsContext() {}

  GraphicsSystem* system() const { return system_; }

  // Creates an offscreen render target that can be targeted by a
  // GraphicsContext::Frame.  This might be used in unit tests where a
  // display render target is not needed and we simply wish to render to an
  // offscreen buffer and analyze the results offline.
  // This method allows platforms to choose their most compatible format that
  // can express RGBA.
  virtual scoped_refptr<RenderTarget> CreateOffscreenRenderTarget(
      const math::Size& dimensions) = 0;

  // Similar to CreateOffscreenRenderTarget, but this should be used when
  // the target will be used with DownloadPixelDataAsRGBA. This render target
  // is more compatible for reading.
  virtual scoped_refptr<RenderTarget> CreateDownloadableOffscreenRenderTarget(
      const math::Size& dimensions) {
    return CreateOffscreenRenderTarget(dimensions);
  }

  // Initializes any values that are needed in a debugging context.
  virtual void InitializeDebugContext() {}

  // Saves render target pixels to CPU memory returned as a scoped_array.
  // This function will wait for the pipeline to flush, so it is slow and should
  // only be used in a debug context.  One use case for the returned image
  // memory is to encode it and serialize it as a PNG file for offline viewing.
  // The dimensions of the returned memory can be found by examining the results
  // of render_target->GetSize().  The pitch in pixels is equal to the width.
  // The pixel format of the returned data is always RGBA8, in that order.
  // Each pixel is 4 bytes.  The output alpha format is always premultiplied
  // alpha.
  virtual scoped_array<uint8_t> DownloadPixelDataAsRGBA(
      const scoped_refptr<RenderTarget>& render_target) = 0;

  // Waits until all drawing is finished.
  virtual void Finish() = 0;

 private:
  GraphicsSystem* system_;
};

}  // namespace backend
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_BACKEND_GRAPHICS_CONTEXT_H_
