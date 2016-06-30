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

#include "cobalt/renderer/rasterizer/blitter/hardware_rasterizer.h"

#include "base/debug/trace_event.h"
#include "base/threading/thread_checker.h"
#include "cobalt/render_tree/resource_provider_stub.h"
#include "cobalt/renderer/backend/blitter/graphics_context.h"
#include "cobalt/renderer/backend/blitter/render_target.h"
#include "cobalt/renderer/rasterizer/blitter/render_tree_node_visitor.h"
#include "cobalt/renderer/rasterizer/blitter/resource_provider.h"
#include "cobalt/renderer/rasterizer/skia/software_rasterizer.h"

#if SB_HAS(BLITTER)

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace blitter {

class HardwareRasterizer::Impl {
 public:
  explicit Impl(backend::GraphicsContext* graphics_context);
  ~Impl();

  void Submit(const scoped_refptr<render_tree::Node>& render_tree,
              const scoped_refptr<backend::RenderTarget>& render_target,
              int options);

  render_tree::ResourceProvider* GetResourceProvider();

 private:
  base::ThreadChecker thread_checker_;

  backend::GraphicsContextBlitter* context_;

  skia::SkiaSoftwareRasterizer software_rasterizer_;
  scoped_ptr<render_tree::ResourceProvider> resource_provider_;

  int64 submit_count_;
};

HardwareRasterizer::Impl::Impl(backend::GraphicsContext* graphics_context)
    : context_(base::polymorphic_downcast<backend::GraphicsContextBlitter*>(
          graphics_context)),
      submit_count_(0) {
  resource_provider_ = scoped_ptr<render_tree::ResourceProvider>(
      new ResourceProvider(context_->GetSbBlitterDevice(),
                           software_rasterizer_.GetResourceProvider()));
}

HardwareRasterizer::Impl::~Impl() {}

void HardwareRasterizer::Impl::Submit(
    const scoped_refptr<render_tree::Node>& render_tree,
    const scoped_refptr<backend::RenderTarget>& render_target, int options) {
  TRACE_EVENT0("cobalt::renderer", "Rasterizer::Submit()");

  int width = render_target->GetSize().width();
  int height = render_target->GetSize().height();

  SbBlitterContext context = context_->GetSbBlitterContext();

  backend::RenderTargetBlitter* render_target_blitter =
      base::polymorphic_downcast<backend::RenderTargetBlitter*>(
          render_target.get());

  CHECK(SbBlitterSetRenderTarget(context,
                                 render_target_blitter->GetSbRenderTarget()));

  // Clear the background before proceeding if the clear option is set.
  // We also clear if this is one of the first 3 submits.  This is for security
  // purposes, so that despite the Blitter API implementation, we ensure that
  // if the output buffer is not completely rendered to, data from a previous
  // process cannot leak in.
  if (options & Rasterizer::kSubmitOptions_Clear || submit_count_ < 3) {
    CHECK(SbBlitterSetBlending(context, false));
    CHECK(SbBlitterSetColor(context, SbBlitterColorFromRGBA(0, 0, 0, 0)));
    CHECK(SbBlitterFillRect(context, SbBlitterMakeRect(0, 0, width, height)));
  }

  {
    TRACE_EVENT0("cobalt::renderer", "VisitRenderTree");

    // Visit the render tree with our Blitter API visitor.
    RenderTreeNodeVisitor visitor(
        context_->GetSbBlitterDevice(), context_->GetSbBlitterContext(),
        render_target_blitter->GetSbRenderTarget(),
        render_target_blitter->GetSize(), &software_rasterizer_);
    render_tree->Accept(&visitor);
  }

  // Finally flip the surface to make visible the rendered results.
  CHECK(SbBlitterFlushContext(context));
  render_target_blitter->Flip();

  ++submit_count_;
}

render_tree::ResourceProvider* HardwareRasterizer::Impl::GetResourceProvider() {
  return resource_provider_.get();
}

HardwareRasterizer::HardwareRasterizer(
    backend::GraphicsContext* graphics_context)
    : impl_(new Impl(graphics_context)) {}

HardwareRasterizer::~HardwareRasterizer() {}

void HardwareRasterizer::Submit(
    const scoped_refptr<render_tree::Node>& render_tree,
    const scoped_refptr<backend::RenderTarget>& render_target, int options) {
  return impl_->Submit(render_tree, render_target, options);
}

render_tree::ResourceProvider* HardwareRasterizer::GetResourceProvider() {
  return impl_->GetResourceProvider();
}

}  // namespace blitter
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // #if SB_HAS(BLITTER)
