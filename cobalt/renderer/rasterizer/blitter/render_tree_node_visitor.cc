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

#include "cobalt/renderer/rasterizer/blitter/render_tree_node_visitor.h"

#include "cobalt/renderer/rasterizer/blitter/image.h"

#include "starboard/blitter.h"

#if SB_HAS(BLITTER)

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace blitter {

using math::Vector2dF;

RenderTreeNodeVisitor::RenderTreeNodeVisitor(
    SbBlitterContext context, SbBlitterRenderTarget render_target)
    : context_(context), render_target_(render_target) {}

void RenderTreeNodeVisitor::Visit(
    render_tree::CompositionNode* composition_node) {
  const render_tree::CompositionNode::Children& children =
      composition_node->data().children();

  if (children.empty()) {
    return;
  }

  transform_.ApplyOffset(composition_node->data().offset());
  for (render_tree::CompositionNode::Children::const_iterator iter =
           children.begin();
       iter != children.end(); ++iter) {
    (*iter)->Accept(this);
  }
  transform_.ApplyOffset(-composition_node->data().offset());
}

void RenderTreeNodeVisitor::Visit(render_tree::FilterNode* filter_node) {
  if (!filter_node->data().opacity_filter ||
      filter_node->data().opacity_filter->opacity() > 0.0f) {
    filter_node->data().source->Accept(this);
  }
}

void RenderTreeNodeVisitor::Visit(render_tree::ImageNode* image_node) {
  Image* blitter_image =
      base::polymorphic_downcast<Image*>(image_node->data().source.get());
  math::Size image_size = blitter_image->GetSize();

  const math::Matrix3F& local_matrix = image_node->data().local_transform;
  if (local_matrix.Get(1, 0) != 0 || local_matrix.Get(0, 1) != 0) {
    NOTIMPLEMENTED()
        << "We don't currently handle non-scale/non-translate matrices.";
    return;
  }

  // Apply the local image coordinate transform to the source rectangle.  Note
  // that the render tree local transform matrix is normalized, but the Blitter
  // API source rectangle is specified in pixel units, so we must multiply the
  // offset by |image_size| in order to get the correct values.
  Transform local_transform;
  local_transform.ApplyScale(math::Vector2dF(1.0f / local_matrix.Get(0, 0),
                                             1.0f / local_matrix.Get(1, 1)));
  local_transform.ApplyOffset(
      math::Vector2dF(-local_matrix.Get(0, 2) * image_size.width(),
                      -local_matrix.Get(1, 2) * image_size.height()));

  // Render the image.
  SbBlitterSetBlending(context_, true);
  SbBlitterSetModulateBlitsWithColor(context_, false);
  SbBlitterBlitRectToRectTiled(
      context_, blitter_image->surface(),
      local_transform.TransformToBlitterRect(math::RectF(image_size)),
      transform_.TransformToBlitterRect(image_node->data().destination_rect));
}

void RenderTreeNodeVisitor::Visit(
    render_tree::MatrixTransformNode* matrix_transform_node) {
  const math::Matrix3F& transform = matrix_transform_node->data().transform;

  if (transform.Get(1, 0) != 0 || transform.Get(0, 1) != 0) {
    NOTIMPLEMENTED()
        << "We don't currently handle non-scale/non-translate matrices.";
    return;
  }

  Transform old_transform(transform_);

  transform_.ApplyOffset(
      math::Vector2dF(transform.Get(0, 2), transform.Get(1, 2)));
  transform_.ApplyScale(
      math::Vector2dF(transform.Get(0, 0), transform.Get(1, 1)));

  matrix_transform_node->data().source->Accept(this);

  transform_ = old_transform;
}

void RenderTreeNodeVisitor::Visit(
    render_tree::PunchThroughVideoNode* punch_through_video_node) {
  NOTIMPLEMENTED();
}

namespace {

class BlitterBrushVisitor : public render_tree::BrushVisitor {
 public:
  explicit BlitterBrushVisitor(SbBlitterContext context) : context_(context) {}

  void Visit(
      const cobalt::render_tree::SolidColorBrush* solid_color_brush) OVERRIDE;
  void Visit(const cobalt::render_tree::LinearGradientBrush*
                 linear_gradient_brush) OVERRIDE;
  void Visit(const cobalt::render_tree::RadialGradientBrush*
                 radial_gradient_brush) OVERRIDE;

 private:
  SbBlitterContext context_;
};

void BlitterBrushVisitor::Visit(
    const cobalt::render_tree::SolidColorBrush* solid_color_brush) {
  const render_tree::ColorRGBA& color = solid_color_brush->color();

  SbBlitterSetColor(context_,
                    SbBlitterColorFromRGBA(color.r() * 255, color.g() * 255,
                                           color.b() * 255, color.a() * 255));
}

void BlitterBrushVisitor::Visit(
    const cobalt::render_tree::LinearGradientBrush* linear_gradient_brush) {
  NOTIMPLEMENTED();
  SbBlitterSetColor(context_, SbBlitterColorFromRGBA(0, 0, 0, 0));
}

void BlitterBrushVisitor::Visit(
    const cobalt::render_tree::RadialGradientBrush* radial_gradient_brush) {
  NOTIMPLEMENTED();
  SbBlitterSetColor(context_, SbBlitterColorFromRGBA(0, 0, 0, 0));
}

}  // namespace

void RenderTreeNodeVisitor::Visit(render_tree::RectNode* rect_node) {
  if (!rect_node->data().background_brush) {
    return;
  }

  SbBlitterSetBlending(context_, true);

  BlitterBrushVisitor brush_visitor(context_);
  rect_node->data().background_brush->Accept(&brush_visitor);

  SbBlitterFillRect(context_,
                    transform_.TransformToBlitterRect(rect_node->data().rect));
}

void RenderTreeNodeVisitor::Visit(
    render_tree::RectShadowNode* rect_shadow_node) {
  NOTIMPLEMENTED();
}

void RenderTreeNodeVisitor::Visit(render_tree::TextNode* text_node) {
  NOTIMPLEMENTED();
}

SbBlitterRect RenderTreeNodeVisitor::Transform::TransformToBlitterRect(
    const math::RectF& rect) const {
  return SbBlitterMakeRect(rect.x() * scale.x() + translate.x(),
                           rect.y() * scale.y() + translate.y(),
                           rect.width() * scale.x(), rect.height() * scale.y());
}

}  // namespace blitter
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // #if SB_HAS(BLITTER)
