// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "cobalt/renderer/rasterizer/skia/shader_preload_tree.h"

#include <utility>
#include <vector>

#include "base/memory/ref_counted.h"
#include "cobalt/math/matrix3_f.h"
#include "cobalt/math/point_f.h"
#include "cobalt/math/rect_f.h"
#include "cobalt/math/vector2d_f.h"
#include "cobalt/render_tree/border.h"
#include "cobalt/render_tree/brush.h"
#include "cobalt/render_tree/color_rgba.h"
#include "cobalt/render_tree/composition_node.h"
#include "cobalt/render_tree/filter_node.h"
#include "cobalt/render_tree/font.h"
#include "cobalt/render_tree/image_node.h"
#include "cobalt/render_tree/matrix_transform_node.h"
#include "cobalt/render_tree/node.h"
#include "cobalt/render_tree/rect_node.h"
#include "cobalt/render_tree/rect_shadow_node.h"
#include "cobalt/render_tree/resource_provider.h"
#include "cobalt/render_tree/rounded_corners.h"
#include "cobalt/render_tree/text_node.h"
#include "cobalt/render_tree/viewport_filter.h"
#include "cobalt/renderer/rasterizer/skia/glyph_buffer.h"
#include "nb/thread_local_boolean.h"
#include "starboard/once.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

namespace {

using math::Matrix3F;
using math::PointF;
using math::RectF;
using math::Vector2dF;
using render_tree::Border;
using render_tree::BorderSide;
using render_tree::BorderStyle;
using render_tree::Brush;
using render_tree::ColorRGBA;
using render_tree::ColorStop;
using render_tree::ColorStopList;
using render_tree::CompositionNode;
using render_tree::FilterNode;
using render_tree::Image;
using render_tree::ImageData;
using render_tree::ImageNode;
using render_tree::kAlphaFormatPremultiplied;
using render_tree::kBorderStyleSolid;
using render_tree::kPixelFormatBGRA8;
using render_tree::kPixelFormatInvalid;
using render_tree::kPixelFormatRGBA8;
using render_tree::LinearGradientBrush;
using render_tree::MatrixTransformNode;
using render_tree::Node;
using render_tree::NodeVisitor;
using render_tree::OpacityFilter;
using render_tree::PixelFormat;
using render_tree::RectNode;
using render_tree::RectShadowNode;
using render_tree::ResourceProvider;
using render_tree::RoundedCorner;
using render_tree::RoundedCorners;
using render_tree::Shadow;
using render_tree::SolidColorBrush;
using render_tree::ViewportFilter;

class NodeGenerator {
 public:
  explicit NodeGenerator(ResourceProvider* provider)
      : resource_provider_(provider), default_rect_(100, 100) {}

  scoped_refptr<Image> CreateImage() {
    scoped_refptr<Image> img = CreateDummyImage(resource_provider_);
    return img;
  }

  ViewportFilter GetViewpointFilterRoundedCorners() {
    RoundedCorners rounded_corners(50, 50);
    ViewportFilter viewport_filter(default_rect_, rounded_corners);
    return viewport_filter;
  }

  ViewportFilter GetViewpointFilter() {
    ViewportFilter viewport_filter(default_rect_);
    return viewport_filter;
  }

  math::Matrix3F GetMatrix() {
    math::Matrix3F mat = math::Matrix3F::Identity();
    mat.SetMatrix(1, 0, 0, 0, 1.34103739f, -0.170518711f, 0, 0, 1);
    return mat;
  }

  scoped_refptr<Node> GetImageNodeMatrix() {
    math::Matrix3F mat = GetMatrix();
    scoped_refptr<ImageNode> img_node(
        new ImageNode(CreateImage(), default_rect_, mat));
    return img_node;
  }

  scoped_ptr<Brush> GetSolidColorBrush() {
    scoped_ptr<SolidColorBrush> brush(
        new SolidColorBrush(ColorRGBA(1, 1, 1, 1)));
    return brush.PassAs<Brush>();
  }

  scoped_ptr<RoundedCorners> GetRoundedCorners() {
    scoped_ptr<RoundedCorners> rounded_corners(new RoundedCorners(50, 50));
    return rounded_corners.Pass();
  }

  scoped_refptr<Node> RectSolidColorBrush() {
    scoped_refptr<Node> rect_node(
        new RectNode(default_rect_, GetSolidColorBrush(),
                     scoped_ptr<Border>(nullptr), GetRoundedCorners()));
    return rect_node;
  }

  BorderSide GetSolidStyleBorderSide() {
    return BorderSide(11.0f, kBorderStyleSolid, ColorRGBA(1, 1, 1, 1));
  }

  scoped_refptr<Node> RectSolidBorderWithRoundedCorners() {
    BorderSide side = GetSolidStyleBorderSide();
    scoped_ptr<Border> border(new Border(side, side, side, side));

    scoped_refptr<Node> rect_node(
        new RectNode(default_rect_, scoped_ptr<Brush>(nullptr), border.Pass(),
                     GetRoundedCorners()));
    return rect_node;
  }

  scoped_refptr<Node> GetDoubleFilteredImageNode() {
    math::Matrix3F mat = GetMatrix();

    scoped_refptr<Node> filt_inner(
        new FilterNode(GetViewpointFilterRoundedCorners(),
                       new ImageNode(CreateImage(), default_rect_, mat)));

    OpacityFilter opacity(0.05f);
    scoped_refptr<Node> filt_outer(new FilterNode(opacity, filt_inner));
    return filt_outer;
  }

  scoped_refptr<Node> GetViewpointFilteredImageNode() {
    math::Matrix3F mat = math::Matrix3F::Identity();
    scoped_refptr<Node> source(
        new FilterNode(GetViewpointFilterRoundedCorners(),
                       new ImageNode(CreateImage(), default_rect_, mat)));

    return new MatrixTransformNode(source, mat);
  }

  scoped_refptr<Node> GetRectShadowNodeWithInsetRoundedCorners() {
    const float spread = 3.95f;
    const bool inset = true;
    const float blur_sigma = 5.91f;
    Shadow shadow(math::Vector2dF(1.95f, 1.95f), blur_sigma,
                  ColorRGBA(0, 0, 0, 1));
    RectShadowNode::Builder rect_shadow_node_builder(default_rect_, shadow,
                                                     inset, spread);

    rect_shadow_node_builder.rounded_corners = RoundedCorners(50, 50);

    scoped_refptr<Node> shadow_node(
        new RectShadowNode(rect_shadow_node_builder));
    return shadow_node;
  }

  scoped_refptr<Node> GetRectShadowNodeWithInset() {
    const float spread = 3.95f;
    const bool inset = true;
    const float blur_sigma = 5.91f;
    Shadow shadow(math::Vector2dF(1.95f, 1.95f), blur_sigma,
                  ColorRGBA(0, 0, 0, 1));
    RectShadowNode::Builder rect_shadow_node_builder(default_rect_, shadow,
                                                     inset, spread);

    scoped_refptr<Node> shadow_node(
        new RectShadowNode(rect_shadow_node_builder));
    return shadow_node;
  }

  scoped_refptr<Node> GetRectShadowNodeWithoutInset() {
    const float spread = 3.95f;
    const bool inset = false;
    const float blur_sigma = 5.91f;
    Shadow shadow(math::Vector2dF(1.95f, 1.95f), blur_sigma,
                  ColorRGBA(0, 0, 0, 1));
    RectShadowNode::Builder rect_shadow_node_builder(default_rect_, shadow,
                                                     inset, spread);

    scoped_refptr<Node> shadow_node(
        new RectShadowNode(rect_shadow_node_builder));
    return shadow_node;
  }

  scoped_refptr<Node> RectNodeLinearGradientOpacity() {
    math::PointF a(0.0f, 0.0f);
    math::PointF b(0.0f, 383.40f);

    OpacityFilter opacity(0.06f);

    ColorStopList color_list;
    color_list.push_back(ColorStop(0, ColorRGBA(0, 0, 0, 0.74902f)));
    color_list.push_back(ColorStop(1, ColorRGBA(0, 0, 0, 0)));

    scoped_refptr<RectNode> rect_node(new RectNode(
        default_rect_,
        scoped_ptr<Brush>(new LinearGradientBrush(a, b, color_list))));

    scoped_refptr<render_tree::Node> node(new FilterNode(opacity, rect_node));
    return node;
  }

 private:
  scoped_refptr<Image> CreateDummyImage(ResourceProvider* resource_provider) {
    // Initialize the image data and store a predictable, testable pattern
    // of image data into it.
    math::Size image_size(16, 16);

    PixelFormat pixel_format = kPixelFormatInvalid;
    if (resource_provider->PixelFormatSupported(kPixelFormatRGBA8)) {
      pixel_format = kPixelFormatRGBA8;
    } else if (resource_provider->PixelFormatSupported(kPixelFormatBGRA8)) {
      pixel_format = kPixelFormatBGRA8;
    } else {
      NOTREACHED() << "Unsupported pixel format.";
    }

    scoped_ptr<ImageData> image_data = resource_provider->AllocateImageData(
        image_size, pixel_format, kAlphaFormatPremultiplied);

    // Write 128 for every pixel component.
    memset(image_data->GetMemory(), 128, image_size.GetArea() * 4);

    // Create and return the new image.
    return resource_provider->CreateImage(image_data.Pass());
  }

  ResourceProvider* resource_provider_;
  RectF default_rect_;
};
}  // namespace.

// Generates a tree which includes common shaders that skia will generate
// and use.
scoped_refptr<Node> GenerateShaderPreloadTree(
    ResourceProvider* resource_provider) {
  NodeGenerator generator(resource_provider);
  CompositionNode::Builder builder;
  builder.AddChild(generator.RectNodeLinearGradientOpacity());
  builder.AddChild(generator.GetImageNodeMatrix());
  builder.AddChild(generator.RectSolidColorBrush());
  builder.AddChild(generator.RectSolidBorderWithRoundedCorners());
  builder.AddChild(generator.GetRectShadowNodeWithInset());
  builder.AddChild(generator.GetRectShadowNodeWithoutInset());
  builder.AddChild(generator.GetRectShadowNodeWithInsetRoundedCorners());
  builder.AddChild(generator.GetDoubleFilteredImageNode());
  builder.AddChild(generator.GetViewpointFilteredImageNode());
  scoped_refptr<Node> composite_node(new CompositionNode(builder.Pass()));
  return composite_node;
}

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
