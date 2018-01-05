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

#include "cobalt/renderer/rasterizer/skia/software_rasterizer.h"

#include "base/debug/trace_event.h"
#include "cobalt/renderer/rasterizer/skia/cobalt_skia_type_conversions.h"
#include "cobalt/renderer/rasterizer/skia/render_tree_node_visitor.h"
#include "cobalt/renderer/rasterizer/skia/software_resource_provider.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

namespace {

SkSurface* CreateScratchSkSurface(const math::Size& size) {
  return SkSurface::NewRaster(
      SkImageInfo::MakeN32Premul(size.width(), size.height()));
}

class SoftwareScratchSurface : public RenderTreeNodeVisitor::ScratchSurface {
 public:
  explicit SoftwareScratchSurface(SkSurface* sk_surface)
      : surface_(sk_surface) {}
  SkSurface* GetSurface() override { return surface_.get(); }

 private:
  SkAutoTUnref<SkSurface> surface_;
};

scoped_ptr<RenderTreeNodeVisitor::ScratchSurface> CreateScratchSurface(
    const math::Size& size) {
  TRACE_EVENT2("cobalt::renderer", "CreateScratchSurface()", "width",
               size.width(), "height", size.height());
  SkSurface* sk_surface = CreateScratchSkSurface(size);
  if (sk_surface) {
    return scoped_ptr<RenderTreeNodeVisitor::ScratchSurface>(
        new SoftwareScratchSurface(sk_surface));
  } else {
    return scoped_ptr<RenderTreeNodeVisitor::ScratchSurface>();
  }
}

void ReturnScratchImage(SkSurface* surface) { surface->unref(); }

}  // namespace

class SoftwareRasterizer::Impl {
 public:
  explicit Impl(bool purge_skia_font_caches_on_destruction);

  // Consume the render tree and output the results to the render target passed
  // into the constructor.
  void Submit(const scoped_refptr<render_tree::Node>& render_tree,
              SkCanvas* render_target);

  render_tree::ResourceProvider* GetResourceProvider();

 private:
  scoped_ptr<render_tree::ResourceProvider> resource_provider_;
};

SoftwareRasterizer::Impl::Impl(bool purge_skia_font_caches_on_destruction)
    : resource_provider_(
          new SoftwareResourceProvider(purge_skia_font_caches_on_destruction)) {
  TRACE_EVENT0("cobalt::renderer", "SoftwareRasterizer::SoftwareRasterizer()");
}

void SoftwareRasterizer::Impl::Submit(
    const scoped_refptr<render_tree::Node>& render_tree,
    SkCanvas* render_target) {
  TRACE_EVENT0("cobalt::renderer", "Rasterizer::Submit()");
  TRACE_EVENT0("cobalt::renderer", "SoftwareRasterizer::Submit()");

  {
    TRACE_EVENT0("cobalt::renderer", "VisitRenderTree");

    // Create the rasterizer and setup its render target to the bitmap we have
    // just created above.
    RenderTreeNodeVisitor::CreateScratchSurfaceFunction
        create_scratch_surface_function = base::Bind(&CreateScratchSurface);
    RenderTreeNodeVisitor visitor(
        render_target, &create_scratch_surface_function, base::Closure(),
        RenderTreeNodeVisitor::RenderImageFallbackFunction(),
        RenderTreeNodeVisitor::RenderImageWithMeshFallbackFunction());

    // Finally, rasterize the render tree to the output canvas using the
    // rasterizer we just created.
    render_tree->Accept(&visitor);
  }
}

render_tree::ResourceProvider* SoftwareRasterizer::Impl::GetResourceProvider() {
  TRACE_EVENT0("cobalt::renderer", "SoftwareRasterizer::GetResourceProvider()");
  return resource_provider_.get();
}

SoftwareRasterizer::SoftwareRasterizer(bool purge_caches_on_destruction)
    : impl_(new Impl(purge_caches_on_destruction)) {}

SoftwareRasterizer::~SoftwareRasterizer() {}

// Consume the render tree and output the results to the render target passed
// into the constructor.
void SoftwareRasterizer::Submit(
    const scoped_refptr<render_tree::Node>& render_tree,
    SkCanvas* render_target) {
  impl_->Submit(render_tree, render_target);
}

render_tree::ResourceProvider* SoftwareRasterizer::GetResourceProvider() {
  return impl_->GetResourceProvider();
}

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
