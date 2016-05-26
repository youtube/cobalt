/*
 * Copyright 2014 Google Inc. All Rights Reserved.
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

#ifndef COBALT_RENDERER_RASTERIZER_RASTERIZER_H_
#define COBALT_RENDERER_RASTERIZER_RASTERIZER_H_

#include "base/memory/ref_counted.h"
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
  static const int kSubmitOptions_Clear = (1 << 0);

  virtual ~Rasterizer() {}

  // Consumes the render tree and rasterizes it to the specified render_target.
  // |options| must be a combination of the |kSubmitOptions_*| constants defined
  // above.
  virtual void Submit(const scoped_refptr<render_tree::Node>& render_tree,
                      const scoped_refptr<backend::RenderTarget>& render_target,
                      int options) = 0;

  void Submit(const scoped_refptr<render_tree::Node>& render_tree,
              const scoped_refptr<backend::RenderTarget>& render_target) {
    Submit(render_tree, render_target, 0);
  }

  // Returns a thread-safe object from which one can produce renderer resources
  // like images and fonts which can be referenced by render trees that are
  // subsequently submitted to this pipeline.  This call must be thread-safe.
  virtual render_tree::ResourceProvider* GetResourceProvider() = 0;
};

}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_RASTERIZER_H_
