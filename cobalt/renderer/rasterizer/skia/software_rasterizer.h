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

#ifndef COBALT_RENDERER_RASTERIZER_SKIA_SOFTWARE_RASTERIZER_H_
#define COBALT_RENDERER_RASTERIZER_SKIA_SOFTWARE_RASTERIZER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/render_tree/node.h"
#include "cobalt/render_tree/resource_provider.h"

class SkCanvas;

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

// While this class does not implement the rasterizer::Rasterizer interface,
// it can be used within a wrapper class that does implement it.
// This class focuses on rendering a render tree to a SkCanvas object,
// so a platform-specific rasterizer::Rasterizer implementation could wrap
// this object and after calling SoftwareRasterizer::Submit(), the wrapper
// class can send the results to a display or render target.
class SoftwareRasterizer {
 public:
  explicit SoftwareRasterizer(bool purge_skia_font_caches_on_destruction);
  ~SoftwareRasterizer();

  // Consume the render tree and output the results to the render target passed
  // into the constructor.
  void Submit(const scoped_refptr<render_tree::Node>& render_tree,
              SkCanvas* render_target);

  render_tree::ResourceProvider* GetResourceProvider();

 private:
  class Impl;
  scoped_ptr<Impl> impl_;
};

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_SKIA_SOFTWARE_RASTERIZER_H_
