// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_RENDERER_RASTERIZER_RASTERIZER_H_
#define COBALT_RENDERER_RASTERIZER_RASTERIZER_H_

#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "cobalt/math/rect.h"
#include "cobalt/render_tree/font.h"
#include "cobalt/render_tree/image.h"
#include "cobalt/render_tree/node.h"
#include "cobalt/render_tree/resource_provider.h"
#include "cobalt/renderer/backend/render_target.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {

// This class abstracts the concept of an actor that consumes render trees
// and produces graphics on a given render target.  A rasterizer
// would typically be setup with a render target that ultimately the submitted
// render tree would be rasterized to.  Having this as an abstract class
// allows for flexibility in its implementation, for example we may provide
// a software rasterizer that simply blits the output to a render target
// after it is done being rasterized.  Alternatively, a hardware implementation
// may be implemented so that render tree rasterization is performed on the GPU.
// The rasterizer leaves the above choices to the implementation, as well
// as choices like which rendering library to use (e.g. Skia).  Of course,
// resources such as Images and Fonts that are referenced by the render tree
// must be in a format supported by the rasterizer, and this is enabled by
// having the Rasterizer return a specialized ResourceProvider.
class Rasterizer {
 public:
  // When set, will clear the render target before rasterizing the render tree
  // to it.
  static const int kSubmitFlags_Clear = (1 << 0);

  struct Options {
    Options() : flags(0) {}

    // A bitwise combination of any of the |kSubmitFlags_*| constants defined
    // above.
    int flags;

    // If specified, indicates which region of |render_target| is
    // dirty and needs to be updated.  If animations are playing for example,
    // then |dirty| can be setup to bound the animations.  A rasterizer is free
    // to ignore this value if they wish.
    base::Optional<math::Rect> dirty;
  };

  virtual ~Rasterizer() {}

  // Consumes the render tree and rasterizes it to the specified render_target.
  // |options| must be a combination of the |kSubmitOptions_*| constants defined
  // above.
  virtual void Submit(const scoped_refptr<render_tree::Node>& render_tree,
                      const scoped_refptr<backend::RenderTarget>& render_target,
                      const Options& options) = 0;

  void Submit(const scoped_refptr<render_tree::Node>& render_tree,
              const scoped_refptr<backend::RenderTarget>& render_target) {
    Submit(render_tree, render_target, Options());
  }

  // Returns a thread-safe object from which one can produce renderer resources
  // like images and fonts which can be referenced by render trees that are
  // subsequently submitted to this pipeline.  This call must be thread-safe.
  virtual render_tree::ResourceProvider* GetResourceProvider() = 0;

  // Returns the total number of times Skia was used to render a non-text
  // render tree node. For Blitter and Skia rasterizer, this value should always
  // be 0.
  virtual int64_t GetFallbackRasterizeCount() { return 0; }

  // Helper class to allow one to create a RAII object that will acquire the
  // current context upon construction and release it upon destruction.
  class ScopedMakeCurrent {
   public:
    explicit ScopedMakeCurrent(Rasterizer* rasterizer)
        : rasterizer_(rasterizer) {
      rasterizer_->MakeCurrent();
    }
    ~ScopedMakeCurrent() { rasterizer_->ReleaseContext(); }

   private:
    Rasterizer* rasterizer_;
  };

  // For GL-based rasterizers, some animation updates can require that the
  // rasterizer's GL context be current when they are executed.  This method
  // is essentially a hack to allow GL-based rasterizers a chance to set their
  // context current before we move to update animations.
  virtual void MakeCurrent() = 0;
  virtual void ReleaseContext() = 0;
};

}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_RASTERIZER_H_
