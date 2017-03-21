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

#ifndef COBALT_RENDERER_BACKEND_RENDER_TARGET_H_
#define COBALT_RENDERER_BACKEND_RENDER_TARGET_H_

#include "base/memory/ref_counted.h"
#include "cobalt/math/size.h"

namespace cobalt {
namespace renderer {
namespace backend {

struct SurfaceInfo;

// A RenderTarget is an object that can be rendered to.
// A GraphicsContext will always be associated with a render target, to which
// its graphics commands will render on.  Render targets will typically be
// associated with a display like a TV or monitor for which output should be
// rendered to, but they can also be created out of textures for offscreen
// rendering.  Render targets are reference counted so that shutdown of
// render targets being rendered to asynchronously can be postponed until
// after rendering is completed.
class RenderTarget : public base::RefCountedThreadSafe<RenderTarget> {
 public:
  // Return metadata about the render target such as dimensions and format.
  virtual const math::Size& GetSize() = 0;

  // Returns a platform-specific handle to the render target that can be
  // passed into platform-specific code.
  virtual intptr_t GetPlatformHandle() = 0;

 protected:
  // Concrete child classes should declare their destructors as private.
  friend class base::RefCountedThreadSafe<RenderTarget>;
  virtual ~RenderTarget() {}
};

}  // namespace backend
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_BACKEND_RENDER_TARGET_H_
