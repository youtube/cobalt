// Copyright 2015 Google Inc. All Rights Reserved.
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

#include <cmath>

#include "base/bind.h"
#include "cobalt/math/matrix3_f.h"
#include "cobalt/math/rect_f.h"
#include "cobalt/math/size_f.h"
#include "cobalt/math/transform_2d.h"
#include "cobalt/math/vector2d_f.h"
#include "cobalt/render_tree/blur_filter.h"
#include "cobalt/render_tree/border.h"
#include "cobalt/render_tree/brush.h"
#include "cobalt/render_tree/color_rgba.h"
#include "cobalt/render_tree/composition_node.h"
#include "cobalt/render_tree/filter_node.h"
#include "cobalt/render_tree/font.h"
#include "cobalt/render_tree/glyph_buffer.h"
#include "cobalt/render_tree/image.h"
#include "cobalt/render_tree/image_node.h"
#include "cobalt/render_tree/matrix_transform_3d_node.h"
#include "cobalt/render_tree/matrix_transform_node.h"
#include "cobalt/render_tree/punch_through_video_node.h"
#include "cobalt/render_tree/rect_node.h"
#include "cobalt/render_tree/rect_shadow_node.h"
#include "cobalt/render_tree/resource_provider.h"
#include "cobalt/render_tree/rounded_corners.h"
#include "cobalt/render_tree/text_node.h"
#include "cobalt/render_tree/typeface.h"
#include "cobalt/renderer/rasterizer/pixel_test_fixture.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/glm/glm/gtc/matrix_transform.hpp"
#include "third_party/glm/glm/gtx/transform.hpp"

#define BILINEAR_FILTERING_SUPPORTED 1
#define NV12_TEXTURE_SUPPORTED 1

#if defined(STARBOARD)
#if !SB_HAS(BILINEAR_FILTERING_SUPPORT)
#undef BILINEAR_FILTERING_SUPPORTED
#define BILINEAR_FILTERING_SUPPORTED 0
#endif
#endif

#if defined(STARBOARD)
#if !SB_HAS(NV12_TEXTURE_SUPPORT)
#undef NV12_TEXTURE_SUPPORTED
#define NV12_TEXTURE_SUPPORTED 0
#endif
#endif

using cobalt::math::Matrix3F;
using cobalt::math::PointF;
using cobalt::math::RectF;
using cobalt::math::RotateMatrix;
using cobalt::math::ScaleMatrix;
using cobalt::math::ScaleSize;
using cobalt::math::Size;
using cobalt::math::SizeF;
using cobalt::math::TranslateMatrix;
using cobalt::math::Vector2dF;
using cobalt::render_tree::AlphaFormat;
using cobalt::render_tree::BlurFilter;
using cobalt::render_tree::Border;
using cobalt::render_tree::BorderSide;
using cobalt::render_tree::Brush;
using cobalt::render_tree::ColorRGBA;
using cobalt::render_tree::ColorStop;
using cobalt::render_tree::ColorStopList;
using cobalt::render_tree::CompositionNode;
using cobalt::render_tree::FilterNode;
using cobalt::render_tree::Font;
using cobalt::render_tree::FontStyle;
using cobalt::render_tree::GlyphBuffer;
using cobalt::render_tree::Image;
using cobalt::render_tree::ImageData;
using cobalt::render_tree::ImageDataDescriptor;
using cobalt::render_tree::ImageNode;
using cobalt::render_tree::LinearGradientBrush;
using cobalt::render_tree::MapToMeshFilter;
using cobalt::render_tree::MatrixTransform3DNode;
using cobalt::render_tree::MatrixTransformNode;
using cobalt::render_tree::Mesh;
using cobalt::render_tree::MultiPlaneImageDataDescriptor;
using cobalt::render_tree::Node;
using cobalt::render_tree::OpacityFilter;
using cobalt::render_tree::PixelFormat;
using cobalt::render_tree::PunchThroughVideoNode;
using cobalt::render_tree::RadialGradientBrush;
using cobalt::render_tree::RawImageMemory;
using cobalt::render_tree::RectNode;
using cobalt::render_tree::RectShadowNode;
using cobalt::render_tree::ResourceProvider;
using cobalt::render_tree::RoundedCorner;
using cobalt::render_tree::RoundedCorners;
using cobalt::render_tree::Shadow;
using cobalt::render_tree::SolidColorBrush;
using cobalt::render_tree::TextNode;
using cobalt::render_tree::ViewportFilter;
using cobalt::renderer::rasterizer::PixelTest;

namespace cobalt {
namespace renderer {
namespace rasterizer {

namespace {

bool SetBounds(bool result, const math::Rect&) { return result; }

}  // namespace

TEST_F(PixelTest, RedFillRectOnEntireSurface) {
  // Create a test render tree that will fill the entire output surface
  // with a solid color rectangle.
  TestTree(new RectNode(
      RectF(output_surface_size()),
      scoped_ptr<Brush>(new SolidColorBrush(ColorRGBA(1.0, 0.0, 0.0, 1)))));
}

TEST_F(PixelTest, RedFillRectOnTopLeftQuarterOfSurface) {
  // Create a test render tree that will fill only a quarter of the surface.
  // This tests that setting the width and height of the rectangle works as
  // expected.  It also tests that we have a consistent value for background
  // pixels that are not rendered to by the rasterizer, which is important
  // for some of these unit tests.
  TestTree(new RectNode(
      RectF(ScaleSize(output_surface_size(), 0.5f, 0.5f)),
      scoped_ptr<Brush>(new SolidColorBrush(ColorRGBA(1.0, 0.0, 0.0, 1)))));
}

TEST_F(PixelTest, GreenFillRectOnEntireSurface) {
  // Create a test render tree that will fill the entire output surface
  // with a solid color rectangle.
  TestTree(new RectNode(
      RectF(output_surface_size()),
      scoped_ptr<Brush>(new SolidColorBrush(ColorRGBA(0.0, 1.0, 0.0, 1)))));
}

TEST_F(PixelTest, GreenFillRectOnTopLeftQuarterOfSurface) {
  // Create a test render tree that will fill only a quarter of the surface.
  // This tests that setting the width and height of the rectangle works as
  // expected.  It also tests that we have a consistent value for background
  // pixels that are not rendered to by the rasterizer, which is important
  // for some of these unit tests.
  TestTree(new RectNode(
      RectF(ScaleSize(output_surface_size(), 0.5f, 0.5f)),
      scoped_ptr<Brush>(new SolidColorBrush(ColorRGBA(0.0, 1.0, 0.0, 1)))));
}

TEST_F(PixelTest, BlueFillRectOnEntireSurface) {
  // Create a test render tree that will fill the entire output surface
  // with a solid color rectangle.
  TestTree(new RectNode(
      RectF(output_surface_size()),
      scoped_ptr<Brush>(new SolidColorBrush(ColorRGBA(0.0, 0.0, 1.0, 1)))));
}

TEST_F(PixelTest, BlueFillRectOnTopLeftQuarterOfSurface) {
  // Create a test render tree that will fill only a quarter of the surface.
  // This tests that setting the width and height of the rectangle works as
  // expected.  It also tests that we have a consistent value for background
  // pixels that are not rendered to by the rasterizer, which is important
  // for some of these unit tests.
  TestTree(new RectNode(
      RectF(ScaleSize(output_surface_size(), 0.5f, 0.5f)),
      scoped_ptr<Brush>(new SolidColorBrush(ColorRGBA(0.0, 0.0, 1.0, 1)))));
}

TEST_F(PixelTest, CircleViaRoundedCorners) {
  RoundedCorner rounded_corner(50, 50);
  scoped_ptr<RoundedCorners> rounded_corners(
      new RoundedCorners(rounded_corner));
  TestTree(new RectNode(RectF(100, 100), scoped_ptr<Brush>(new SolidColorBrush(
                                             ColorRGBA(1.0, 0.0, 0.0, 1))),
                        rounded_corners.Pass()));
}

// These particular rounded corner values were found to cause a crash problem
// with some rasterizers, this test is added to prevent a regression.
TEST_F(PixelTest, AlmostCircleViaRoundedCorners) {
  RoundedCorner rounded_corner(11.9999504f, 11.9999504f);
  scoped_ptr<RoundedCorners> rounded_corners(
      new RoundedCorners(rounded_corner));
  TestTree(new RectNode(
      RectF(24, 24),
      scoped_ptr<Brush>(new SolidColorBrush(ColorRGBA(1.0, 1.0, 0.0, 1.0))),
      rounded_corners.Pass()));
}

TEST_F(PixelTest, OvalViaRoundedCorners) {
  RoundedCorner top_left(50, 25);
  RoundedCorner top_right(50, 25);
  RoundedCorner bottom_right(50, 25);
  RoundedCorner bottom_left(50, 25);
  scoped_ptr<RoundedCorners> rounded_corners(
      new RoundedCorners(top_left, top_right, bottom_right, bottom_left));
  TestTree(new RectNode(RectF(100, 50), scoped_ptr<Brush>(new SolidColorBrush(
                                            ColorRGBA(1.0, 0.0, 0.0, 1))),
                        rounded_corners.Pass()));
}

TEST_F(PixelTest, RotatedOvalViaRoundedCorners) {
  RoundedCorner top_left(50, 25);
  RoundedCorner top_right(50, 25);
  RoundedCorner bottom_right(50, 25);
  RoundedCorner bottom_left(50, 25);

  scoped_ptr<RoundedCorners> rounded_corners(
      new RoundedCorners(top_left, top_right, bottom_right, bottom_left));

  TestTree(new MatrixTransformNode(
      new RectNode(RectF(100, 50), scoped_ptr<Brush>(new SolidColorBrush(
                                       ColorRGBA(1.0, 0.0, 0.0, 1))),
                   rounded_corners.Pass()),
      TranslateMatrix(50, 100) *
          RotateMatrix(static_cast<float>(M_PI) / 6.0f)));
}

TEST_F(PixelTest, ScaledThenRotatedRectWithDifferentRoundedCorners) {
  RoundedCorner top_left(6, 15);
  RoundedCorner top_right(0, 0);
  RoundedCorner bottom_right(6, 25);
  RoundedCorner bottom_left(2, 25);

  scoped_ptr<RoundedCorners> rounded_corners(
      new RoundedCorners(top_left, top_right, bottom_right, bottom_left));

  TestTree(new MatrixTransformNode(
      new RectNode(RectF(-7, -25, 14, 50),
                   scoped_ptr<Brush>(
                       new SolidColorBrush(ColorRGBA(1, 1, 1, 1))),
                   rounded_corners.Pass()),
      TranslateMatrix(100.0f, 100.0f) *
      RotateMatrix(static_cast<float>(M_PI) / 3.0f) *
      ScaleMatrix(-10.0f, 2.0f)));
}

TEST_F(PixelTest, RotatedThenScaledRectWithDifferentRoundedCorners) {
  RoundedCorner top_left(4, 7);
  RoundedCorner top_right(0, 0);
  RoundedCorner bottom_right(10, 2);
  RoundedCorner bottom_left(5, 3);

  scoped_ptr<RoundedCorners> rounded_corners(
      new RoundedCorners(top_left, top_right, bottom_right, bottom_left));

  TestTree(new MatrixTransformNode(
      new RectNode(RectF(-10, -7, 20, 14),
                   scoped_ptr<Brush>(
                       new SolidColorBrush(ColorRGBA(1, 1, 1, 1))),
                   rounded_corners.Pass()),
      TranslateMatrix(100.0f, 100.0f) *
      ScaleMatrix(6.0f, 9.0f) *
      RotateMatrix(static_cast<float>(M_PI) / 6.0f)));
}

TEST_F(PixelTest, RedRectWithDifferentRoundedCornersOnTopLeftOfSurface) {
  RoundedCorner top_left(10, 10);
  RoundedCorner top_right(20, 20);
  RoundedCorner bottom_right(30, 30);
  RoundedCorner bottom_left(40, 40);
  scoped_ptr<RoundedCorners> rounded_corners(
      new RoundedCorners(top_left, top_right, bottom_right, bottom_left));
  TestTree(new RectNode(
      RectF(ScaleSize(output_surface_size(), 0.5f, 0.5f)),
      scoped_ptr<Brush>(new SolidColorBrush(ColorRGBA(1.0, 0.0, 0.0, 1))),
      rounded_corners.Pass()));
}

TEST_F(PixelTest, RedRectWith2DifferentRadiusForEachCornerOnTopLeftOfSurface) {
  RoundedCorner top_left(10, 20);
  RoundedCorner top_right(30, 40);
  RoundedCorner bottom_right(50, 60);
  RoundedCorner bottom_left(70, 80);
  scoped_ptr<RoundedCorners> rounded_corners(
      new RoundedCorners(top_left, top_right, bottom_right, bottom_left));
  math::RectF rect(ScaleSize(output_surface_size(), 0.5f, 0.5f));
  *rounded_corners = rounded_corners->Normalize(rect);
  TestTree(new RectNode(
      rect,
      scoped_ptr<Brush>(new SolidColorBrush(ColorRGBA(1.0, 0.0, 0.0, 1))),
      rounded_corners.Pass()));
}

TEST_F(PixelTest, EmptyRectWithRedBorderOnTopLeftOfSurface) {
  // Create a test render tree that will cover the top left of surface with
  // a rectangle which has a border.
  BorderSide border_side(20, render_tree::kBorderStyleSolid,
                         ColorRGBA(1.0, 0.0, 0.0, 1));
  scoped_ptr<Border> border(new Border(border_side));
  TestTree(new RectNode(RectF(ScaleSize(output_surface_size(), 0.5f, 0.5f)),
                        border.Pass()));
}

TEST_F(PixelTest, RedRectWithBlueBorderOnTopLeftOfSurface) {
  // Create a test render tree that will cover the top left of surface with
  // a rectangle which has a border.
  BorderSide border_side(20, render_tree::kBorderStyleSolid,
                         ColorRGBA(0.0, 0.0, 1.0, 1));
  scoped_ptr<Border> border(new Border(border_side));
  TestTree(new RectNode(
      RectF(ScaleSize(output_surface_size(), 0.5f, 0.5f)),
      scoped_ptr<Brush>(new SolidColorBrush(ColorRGBA(1.0, 0.0, 0.0, 1))),
      border.Pass()));
}

TEST_F(PixelTest, RedRectWith4DifferentBlueBordersOnTopLeftOfSurface) {
  // Create a test render tree that will cover the top left of surface with
  // a rectangle which has a border.
  BorderSide border_left(10, render_tree::kBorderStyleSolid,
                         ColorRGBA(0.0, 0.0, 1.0, 1));
  BorderSide border_right(20, render_tree::kBorderStyleSolid,
                          ColorRGBA(0.0, 0.0, 1.0, 1));
  BorderSide border_top(30, render_tree::kBorderStyleSolid,
                        ColorRGBA(0.0, 0.0, 1.0, 1));
  BorderSide border_bottom(40, render_tree::kBorderStyleSolid,
                           ColorRGBA(0.0, 0.0, 1.0, 1));

  scoped_ptr<Border> border(
      new Border(border_left, border_right, border_top, border_bottom));
  TestTree(new RectNode(
      RectF(ScaleSize(output_surface_size(), 0.5f, 0.5f)),
      scoped_ptr<Brush>(new SolidColorBrush(ColorRGBA(1.0, 0.0, 0.0, 1))),
      border.Pass()));
}

TEST_F(PixelTest, RedRectWith4DifferentColorBorders) {
  // Create a test render tree that will cover the top left of surface with
  // a rectangle which has 4 different color borders.
  BorderSide border_left(20, render_tree::kBorderStyleSolid,
                         ColorRGBA(0.0, 0.0, 1.0, 1));
  BorderSide border_right(20, render_tree::kBorderStyleSolid,
                          ColorRGBA(0.0, 1.0, 1.0, 0.5));
  BorderSide border_top(20, render_tree::kBorderStyleSolid,
                        ColorRGBA(1.0, 1.0, 1.0, 1));
  BorderSide border_bottom(20, render_tree::kBorderStyleSolid,
                           ColorRGBA(0.0, 1.0, 0.0, 0.8f));

  scoped_ptr<Border> border(
      new Border(border_left, border_right, border_top, border_bottom));
  TestTree(new RectNode(
      RectF(ScaleSize(output_surface_size(), 0.5f, 0.5f)),
      scoped_ptr<Brush>(new SolidColorBrush(ColorRGBA(1.0, 0.0, 0.0, 1))),
      border.Pass()));
}

TEST_F(PixelTest, RedRectWith4DifferentColorAndWidthBorders) {
  // Create a test render tree that will cover the top left of surface with
  // a rectangle which has 4 different color borders.
  BorderSide border_left(10, render_tree::kBorderStyleSolid,
                         ColorRGBA(0.0, 0.0, 1.0, 1));
  BorderSide border_right(20, render_tree::kBorderStyleSolid,
                          ColorRGBA(0.0, 1.0, 1.0, 0.5));
  BorderSide border_top(30, render_tree::kBorderStyleSolid,
                        ColorRGBA(1.0, 1.0, 1.0, 1));
  BorderSide border_bottom(40, render_tree::kBorderStyleSolid,
                           ColorRGBA(0.0, 1.0, 0.0, 0.8f));

  scoped_ptr<Border> border(
      new Border(border_left, border_right, border_top, border_bottom));
  TestTree(new RectNode(
      RectF(ScaleSize(output_surface_size(), 0.5f, 0.5f)),
      scoped_ptr<Brush>(new SolidColorBrush(ColorRGBA(1.0, 0.0, 0.0, 1))),
      border.Pass()));
}

TEST_F(PixelTest, DownwardPointingTriangle) {
  // Create a test render tree that will draw a downward pointing triangle.
  BorderSide border_left(70, render_tree::kBorderStyleSolid,
                         ColorRGBA(0.0, 0.0, 0.0, 0));
  BorderSide border_right(70, render_tree::kBorderStyleSolid,
                          ColorRGBA(0.0, 0.0, 0.0, 0.0));
  BorderSide border_top(80, render_tree::kBorderStyleSolid,
                        ColorRGBA(1.0, 0.0, 0.0, 1));
  BorderSide border_bottom(0, render_tree::kBorderStyleNone,
                           ColorRGBA(1.0, 0.0, 0.0, 1));

  scoped_ptr<Border> border(
      new Border(border_left, border_right, border_top, border_bottom));
  TestTree(new RectNode(RectF(140, 80), scoped_ptr<Brush>(new SolidColorBrush(
                                            ColorRGBA(1.0, 0.0, 0.0, 1))),
                        border.Pass()));
}

namespace {

struct RGBAWord {
  RGBAWord(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    rgba[0] = r;
    rgba[1] = g;
    rgba[2] = b;
    rgba[3] = a;
  }
  uint8_t rgba[4];
};

// This function will query the provided ResourceProvider to determine the
// best pixel format to use when generating Images from pixel data.
PixelFormat ChoosePixelFormat(ResourceProvider* resource_provider) {
  if (resource_provider->PixelFormatSupported(render_tree::kPixelFormatRGBA8)) {
    return render_tree::kPixelFormatRGBA8;
  } else if (resource_provider->PixelFormatSupported(
                 render_tree::kPixelFormatBGRA8)) {
    return render_tree::kPixelFormatBGRA8;
  } else {
    return render_tree::kPixelFormatInvalid;
  }
}

// Represents a RGBA channel swizzling relative to the order RGBA.
// e.g. RGBA -> 0123, BGRA -> 2103.
struct RGBASwizzle {
  RGBASwizzle(int r, int g, int b, int a) {
    channel[0] = r;
    channel[1] = g;
    channel[2] = b;
    channel[3] = a;
  }

  int channel[4];
};

// Returns a RGBA Swizzle relative to the order RGBA, based off of the
// render_tree::PixelFormat.
RGBASwizzle GetRGBASwizzleForPixelFormat(PixelFormat pixel_format) {
  switch (pixel_format) {
    case render_tree::kPixelFormatRGBA8: {
      return RGBASwizzle(0, 1, 2, 3);
    }
    case render_tree::kPixelFormatBGRA8: {
      return RGBASwizzle(2, 1, 0, 3);
    }
    case render_tree::kPixelFormatUYVY:
    case render_tree::kPixelFormatY8:
    case render_tree::kPixelFormatU8:
    case render_tree::kPixelFormatV8:
    case render_tree::kPixelFormatUV8:
    case render_tree::kPixelFormatInvalid: {
      NOTREACHED() << "Invalid pixel format.";
    }
  }
  return RGBASwizzle(0, 1, 2, 3);
}

// Creates a 3x3 checkered image of colors where each cell color is determined
// by adding the corresponding row and column colors (and clamping each
// component to 255).
scoped_refptr<Image> CreateAdditiveColorGridImage(
    ResourceProvider* resource_provider, const SizeF& dimensions,
    const std::vector<RGBAWord>& row_colors,
    const std::vector<RGBAWord>& column_colors, AlphaFormat alpha_format) {
  DCHECK_EQ(3, row_colors.size());
  DCHECK_EQ(3, column_colors.size());
  // Allocate memory to store the image data that we will subsequently generate.
  PixelFormat pixel_format = ChoosePixelFormat(resource_provider);
  scoped_ptr<ImageData> image_data = resource_provider->AllocateImageData(
      Size(static_cast<int>(dimensions.width()),
           static_cast<int>(dimensions.height())),
      pixel_format, alpha_format);

  RGBASwizzle rgba_swizzle = GetRGBASwizzleForPixelFormat(pixel_format);
  for (int i = 0; i < dimensions.height(); ++i) {
    uint8_t* pixel_data = image_data->GetMemory() +
                          image_data->GetDescriptor().pitch_in_bytes * i;
    // Figure out which horizontal stripe we're at at so that we can set the
    // pixel color appropriately.
    int color_row = (i * 3) / dimensions.height();
    for (int j = 0; j < dimensions.width(); ++j) {
      int pixel_offset = j * 4;
      // Figure out which vertical stripe we're at so that we can set the
      // pixel color appropriately.
      int color_column = (j * 3) / dimensions.width();
      for (int c = 0; c < 4; ++c) {
        pixel_data[pixel_offset + rgba_swizzle.channel[c]] =
            std::min(255, row_colors[color_row].rgba[c] +
                              column_colors[color_column].rgba[c]);
      }
    }
  }

  return resource_provider->CreateImage(image_data.Pass());
}

scoped_refptr<Image> CreateColoredCheckersImageForAlphaFormat(
    ResourceProvider* resource_provider, const SizeF& dimensions,
    render_tree::AlphaFormat alpha_format) {
  std::vector<RGBAWord> row_colors;
  row_colors.push_back(RGBAWord(255, 0, 0, 255));
  row_colors.push_back(RGBAWord(0, 255, 0, 255));
  row_colors.push_back(RGBAWord(0, 0, 255, 255));
  return CreateAdditiveColorGridImage(resource_provider, dimensions, row_colors,
                                      row_colors, alpha_format);
}

scoped_refptr<Image> CreateColoredCheckersImage(
    ResourceProvider* resource_provider, const SizeF& dimensions) {
  return CreateColoredCheckersImageForAlphaFormat(
      resource_provider, dimensions, render_tree::kAlphaFormatPremultiplied);
}

}  // namespace

TEST_F(PixelTest, SingleRGBAImageWithSameSizeAsRenderTarget) {
  // Tests that ImageNodes work as expected.
  scoped_refptr<Image> image =
      CreateColoredCheckersImage(GetResourceProvider(), output_surface_size());

  TestTree(new ImageNode(image));
}

TEST_F(PixelTest, SingleRGBAImageWithAlphaFormatOpaque) {
  scoped_refptr<Image> image = CreateColoredCheckersImageForAlphaFormat(
      GetResourceProvider(), output_surface_size(),
      render_tree::kAlphaFormatOpaque);

  TestTree(new ImageNode(image));
}

TEST_F(PixelTest, SingleRGBAImageWithReflection) {
  SizeF half_output_size = ScaleSize(output_surface_size(), 0.5f, 0.5f);
  scoped_refptr<Image> image =
      CreateColoredCheckersImage(GetResourceProvider(), output_surface_size());

  TestTree(new MatrixTransformNode(
      new ImageNode(image),
      TranslateMatrix(half_output_size.width(), half_output_size.height()) *
          ScaleMatrix(1.0f, -1.0f) *
          TranslateMatrix(-half_output_size.width(),
                          -half_output_size.height())));
}

TEST_F(PixelTest, SingleRGBAImageWithAlphaFormatOpaqueAndRoundedCorners) {
  scoped_refptr<Image> image = CreateColoredCheckersImageForAlphaFormat(
      GetResourceProvider(), output_surface_size(),
      render_tree::kAlphaFormatOpaque);

  TestTree(new FilterNode(
      ViewportFilter(RectF(25, 25, 150, 150), RoundedCorners(75, 75)),
      new ImageNode(image)));
}

TEST_F(PixelTest,
       SingleRGBAImageWithAlphaFormatOpaqueAndRoundedCornersOnSolidColor) {
  scoped_refptr<Image> image = CreateColoredCheckersImageForAlphaFormat(
      GetResourceProvider(), output_surface_size(),
      render_tree::kAlphaFormatOpaque);

  CompositionNode::Builder builder;
  builder.AddChild(new RectNode(
      RectF(output_surface_size()),
      scoped_ptr<Brush>(new SolidColorBrush(ColorRGBA(0.0, 1.0, 0.0, 1)))));
  builder.AddChild(new FilterNode(
      ViewportFilter(RectF(25, 25, 150, 150), RoundedCorners(75, 75)),
      new ImageNode(image)));
  TestTree(new CompositionNode(builder.Pass()));
}

TEST_F(PixelTest, RectWithRoundedCornersOnSolidColor) {
  CompositionNode::Builder builder;
  builder.AddChild(new RectNode(
      RectF(output_surface_size()),
      scoped_ptr<Brush>(new SolidColorBrush(ColorRGBA(0.0, 1.0, 0.0, 1)))));
  builder.AddChild(new FilterNode(
      ViewportFilter(RectF(25, 25, 150, 150), RoundedCorners(75, 75)),
      new RectNode(RectF(output_surface_size()),
                   scoped_ptr<Brush>(
                       new SolidColorBrush(ColorRGBA(0.0, 0.0, 1.0, 1))))));
  TestTree(new CompositionNode(builder.Pass()));
}

TEST_F(PixelTest, EmptyRectWithRoundedCornersAnd4DifferentEdgeColorsBorder) {
  // Create a test render tree for a border with rounded corners and 4 different
  // edge colors.
  BorderSide border_left(25, render_tree::kBorderStyleSolid,
                         ColorRGBA(0.0, 0.0, 1.0, 1));
  BorderSide border_right(25, render_tree::kBorderStyleSolid,
                          ColorRGBA(0.0, 1.0, 1.0, 0.5));
  BorderSide border_top(25, render_tree::kBorderStyleSolid,
                        ColorRGBA(1.0, 1.0, 1.0, 1));
  BorderSide border_bottom(25, render_tree::kBorderStyleSolid,
                           ColorRGBA(0.0, 1.0, 0.0, 0.8f));
  scoped_ptr<Border> border(
      new Border(border_left, border_right, border_top, border_bottom));

  math::RectF rect(200, 100);

  RoundedCorner rounded_corner(100, 100);
  scoped_ptr<RoundedCorners> rounded_corners(
      new RoundedCorners(rounded_corner));
  *rounded_corners = rounded_corners->Normalize(rect);

  TestTree(new RectNode(rect, border.Pass(), rounded_corners.Pass()));
}

TEST_F(PixelTest, EmptyRectWith4DifferentRoundedCornersAndEdgeColorsBorder) {
  // Create a test render tree for a border with 4 different rounded corners and
  // edge colors.
  BorderSide border_left(25, render_tree::kBorderStyleSolid,
                         ColorRGBA(0.0, 0.0, 1.0, 1));
  BorderSide border_right(25, render_tree::kBorderStyleSolid,
                          ColorRGBA(0.0, 1.0, 1.0, 0.5));
  BorderSide border_top(25, render_tree::kBorderStyleSolid,
                        ColorRGBA(1.0, 1.0, 1.0, 1));
  BorderSide border_bottom(25, render_tree::kBorderStyleSolid,
                           ColorRGBA(0.0, 1.0, 0.0, 0.8f));
  scoped_ptr<Border> border(
      new Border(border_left, border_right, border_top, border_bottom));

  math::RectF rect(200, 100);

  RoundedCorner top_left(30, 30);
  RoundedCorner top_right(10, 10);
  RoundedCorner bottom_right(50, 50);
  RoundedCorner bottom_left(40, 40);
  scoped_ptr<RoundedCorners> rounded_corners(
      new RoundedCorners(top_left, top_right, bottom_right, bottom_left));
  *rounded_corners = rounded_corners->Normalize(rect);

  TestTree(new RectNode(rect, border.Pass(), rounded_corners.Pass()));
}

TEST_F(PixelTest, EmptyRectWithRoundedCornersAnd4DifferentEdgeWidthsBorder) {
  // Create a test render tree for a border with rounded corners and 4 different
  // edge widths.
  BorderSide border_left(25, render_tree::kBorderStyleSolid,
                         ColorRGBA(0.0, 1.0, 0.0, 0.8f));
  BorderSide border_right(30, render_tree::kBorderStyleSolid,
                          ColorRGBA(0.0, 1.0, 0.0, 0.8f));
  BorderSide border_top(40, render_tree::kBorderStyleSolid,
                        ColorRGBA(0.0, 1.0, 0.0, 0.8f));
  BorderSide border_bottom(10, render_tree::kBorderStyleSolid,
                           ColorRGBA(0.0, 1.0, 0.0, 0.8f));
  scoped_ptr<Border> border(
      new Border(border_left, border_right, border_top, border_bottom));

  math::RectF rect(200, 100);

  RoundedCorner rounded_corner(100, 100);
  scoped_ptr<RoundedCorners> rounded_corners(
      new RoundedCorners(rounded_corner));
  *rounded_corners = rounded_corners->Normalize(rect);

  TestTree(new RectNode(rect, border.Pass(), rounded_corners.Pass()));
}

TEST_F(PixelTest, EmptyRectWith4DifferentRoundedCornersAndEdgeWidthsBorder) {
  // Create a test render tree for a border with 4 different rounded corners and
  // edge widths.
  BorderSide border_left(25, render_tree::kBorderStyleSolid,
                         ColorRGBA(0.0, 1.0, 0.0, 0.8f));
  BorderSide border_right(30, render_tree::kBorderStyleSolid,
                          ColorRGBA(0.0, 1.0, 0.0, 0.8f));
  BorderSide border_top(40, render_tree::kBorderStyleSolid,
                        ColorRGBA(0.0, 1.0, 0.0, 0.8f));
  BorderSide border_bottom(10, render_tree::kBorderStyleSolid,
                           ColorRGBA(0.0, 1.0, 0.0, 0.8f));
  scoped_ptr<Border> border(
      new Border(border_left, border_right, border_top, border_bottom));

  math::RectF rect(200, 100);

  RoundedCorner top_left(50, 50);
  RoundedCorner top_right(10, 10);
  RoundedCorner bottom_right(60, 60);
  RoundedCorner bottom_left(40, 40);
  scoped_ptr<RoundedCorners> rounded_corners(
      new RoundedCorners(top_left, top_right, bottom_right, bottom_left));
  *rounded_corners = rounded_corners->Normalize(rect);

  TestTree(new RectNode(rect, border.Pass(), rounded_corners.Pass()));
}

TEST_F(PixelTest,
       EmptyRectWithRoundedCornersAnd4DifferentEdgeColorsAndEdgeWidthsBorder) {
  // Create a test render tree for a border with rounded corners and 4 different
  // edge colors and edge widths.
  BorderSide border_left(25, render_tree::kBorderStyleSolid,
                         ColorRGBA(0.0, 0.0, 1.0, 1));
  BorderSide border_right(30, render_tree::kBorderStyleSolid,
                          ColorRGBA(0.0, 1.0, 1.0, 0.5));
  BorderSide border_top(40, render_tree::kBorderStyleSolid,
                        ColorRGBA(1.0, 1.0, 1.0, 1));
  BorderSide border_bottom(10, render_tree::kBorderStyleSolid,
                           ColorRGBA(0.0, 1.0, 0.0, 0.8f));
  scoped_ptr<Border> border(
      new Border(border_left, border_right, border_top, border_bottom));

  math::RectF rect(200, 100);

  RoundedCorner rounded_corner(100, 100);
  scoped_ptr<RoundedCorners> rounded_corners(
      new RoundedCorners(rounded_corner));
  *rounded_corners = rounded_corners->Normalize(rect);

  TestTree(new RectNode(rect, border.Pass(), rounded_corners.Pass()));
}

TEST_F(PixelTest,
       EmptyRectWith4DifferentRoundedCornersAndEdgeColorsAndEdgeWidthsBorder) {
  // Create a test render tree for a border with 4 different rounded corners,
  // edge colors and edge widths.
  BorderSide border_left(25, render_tree::kBorderStyleSolid,
                         ColorRGBA(0.0, 0.0, 1.0, 1));
  BorderSide border_right(30, render_tree::kBorderStyleSolid,
                          ColorRGBA(0.0, 1.0, 1.0, 0.5));
  BorderSide border_top(40, render_tree::kBorderStyleSolid,
                        ColorRGBA(1.0, 1.0, 1.0, 1));
  BorderSide border_bottom(10, render_tree::kBorderStyleSolid,
                           ColorRGBA(0.0, 1.0, 0.0, 0.8f));
  scoped_ptr<Border> border(
      new Border(border_left, border_right, border_top, border_bottom));

  math::RectF rect(200, 100);

  RoundedCorner top_left(60, 60);
  RoundedCorner top_right(10, 10);
  RoundedCorner bottom_right(50, 50);
  RoundedCorner bottom_left(40, 40);
  scoped_ptr<RoundedCorners> rounded_corners(
      new RoundedCorners(top_left, top_right, bottom_right, bottom_left));
  *rounded_corners = rounded_corners->Normalize(rect);

  TestTree(new RectNode(rect, border.Pass(), rounded_corners.Pass()));
}

TEST_F(PixelTest, SingleRGBAImageLargerThanRenderTarget) {
  // Tests that rasterizing images that are larger than the render target
  // work as expected (e.g. they are cropped).
  scoped_refptr<Image> image = CreateColoredCheckersImage(
      GetResourceProvider(), ScaleSize(output_surface_size(), 2.0f, 2.0f));

  TestTree(new ImageNode(image));
}

TEST_F(PixelTest, SingleRGBAImageWithShrunkenDestRect) {
  // Tests rasterizing images that are shrunken via setting the
  // destination size.  The output should be a scaled down image that does not
  // occupy the entire output surface.
  scoped_refptr<Image> image =
      CreateColoredCheckersImage(GetResourceProvider(), output_surface_size());

  TestTree(new ImageNode(image,
                         RectF(ScaleSize(output_surface_size(), 0.5f, 0.5f))));
}

TEST_F(PixelTest, SingleRGBAImageWithEnlargedDestRect) {
  // Tests rasterizing images that are enlarged via setting the
  // destination size.  The output should be a scaled up image that is cropped
  // by the output surface rectangle.
  scoped_refptr<Image> image =
      CreateColoredCheckersImage(GetResourceProvider(), output_surface_size());

  TestTree(new ImageNode(image,
                         RectF(ScaleSize(output_surface_size(), 2.0f, 2.0f))));
}

namespace {

// Sets up a composition node with a single RectNode child that is setup to
// rasterize as solid red.  The RectNode will have size equal to half of the
// target surface, and will be added as a child to the composition node with
// the specified transform applied.  This function is used for the multiple
// tests that follow this function definition that ensure that different
// composition node transforms are workign correctly.
scoped_refptr<MatrixTransformNode> MakeTransformedSingleSolidColorRect(
    const SizeF& size, const Matrix3F& transform) {
  return new MatrixTransformNode(
      new RectNode(
          RectF(ScaleSize(size, 0.5f, 0.5f)),
          scoped_ptr<Brush>(new SolidColorBrush(ColorRGBA(1.0, 0.0, 0.0, 1)))),
      transform);
}
}  // namespace

TEST_F(PixelTest, CompositionOfSingleSolidColorRectWithNoTransform) {
  TestTree(MakeTransformedSingleSolidColorRect(output_surface_size(),
                                               Matrix3F::Identity()));
}

TEST_F(PixelTest, CompositionOfSingleSolidColorRectWithTranslation) {
  TestTree(MakeTransformedSingleSolidColorRect(
      output_surface_size(),
      TranslateMatrix(output_surface_size().width() * 0.25f,
                      output_surface_size().height() * 0.25f)));
}

TEST_F(PixelTest, CompositionOfSingleSolidColorRectWithRotation) {
  TestTree(MakeTransformedSingleSolidColorRect(
      output_surface_size(), RotateMatrix(static_cast<float>(M_PI) / 4.0f)));
}

TEST_F(PixelTest, CompositionOfSingleSolidColorRectWithIsoScale) {
  TestTree(MakeTransformedSingleSolidColorRect(output_surface_size(),
                                               ScaleMatrix(1.5f)));
}

TEST_F(PixelTest, CompositionOfSingleSolidColorRectWithAnisoScale) {
  TestTree(MakeTransformedSingleSolidColorRect(output_surface_size(),
                                               ScaleMatrix(1.1f, 1.6f)));
}

TEST_F(PixelTest,
       CompositionOfSingleSolidColorRectWithTranslationRotationAndAnisoScale) {
  TestTree(MakeTransformedSingleSolidColorRect(
      output_surface_size(),
      TranslateMatrix(output_surface_size().width() / 4,
                      output_surface_size().height() / 4) *
          RotateMatrix(static_cast<float>(M_PI) / 4.0f) *
          ScaleMatrix(1.1f, 1.6f)));
}

namespace {
scoped_refptr<CompositionNode> CreateCascadedRectsOfDifferentColors(
    const SizeF& rect_size) {
  // Add multiple rect nodes of different colors to a composition node.  This
  // test ensures that the order of children in a composition node is respected
  // by the rasterizer, and that we support multiple children.
  CompositionNode::Builder composition_builder;

  composition_builder.AddChild(new RectNode(
      RectF(PointF(25, 25), rect_size),
      scoped_ptr<Brush>(new SolidColorBrush(ColorRGBA(1.0, 0.0, 0.0, 1)))));

  composition_builder.AddChild(new RectNode(
      RectF(PointF(50, 50), rect_size),
      scoped_ptr<Brush>(new SolidColorBrush(ColorRGBA(0.0, 1.0, 0.0, 1)))));

  composition_builder.AddChild(new RectNode(
      RectF(PointF(75, 75), rect_size),
      scoped_ptr<Brush>(new SolidColorBrush(ColorRGBA(0.0, 0.0, 1.0, 1)))));

  return new CompositionNode(composition_builder.Pass());
}
}  // namespace

TEST_F(PixelTest, CompositionOfCascadedRectsOfDifferentColors) {
  TestTree(CreateCascadedRectsOfDifferentColors(
      ScaleSize(output_surface_size(), 0.5f, 0.5f)));
}

TEST_F(PixelTest, TransparentRectOverlappingSolidRect) {
  // This test ensures that a transparent solid color RectNode placed on top
  // of an opaque rect node produces the proper visual effect, e.g. we should
  // be able to "see through" the transparent rectangle.
  CompositionNode::Builder composition_builder;

  composition_builder.AddChild(new RectNode(
      RectF(PointF(25, 25), ScaleSize(output_surface_size(), 0.5f, 0.5f)),
      scoped_ptr<Brush>(new SolidColorBrush(ColorRGBA(1.0, 0.0, 0.0, 1)))));

  composition_builder.AddChild(new RectNode(
      RectF(PointF(50, 50), ScaleSize(output_surface_size(), 0.5f, 0.5f)),
      scoped_ptr<Brush>(new SolidColorBrush(ColorRGBA(0.0, 0.0, 1.0, 0.5)))));

  TestTree(new CompositionNode(composition_builder.Pass()));
}

TEST_F(PixelTest, RectDrawOrder) {
  // This test ensures that relative draw order is preserved for overlapping
  // rectangles.

  // Use a linear congruential generator (std::minstd_rand) to generate
  // deterministic pseudo-random numbers.
  struct SimpleRand {
    SimpleRand() : seed(10222016) {}
    int32_t operator()() {
      seed = static_cast<int32_t>(
          (static_cast<int64_t>(seed) * 48271) % 2147483647);
      return seed;
    }
    int32_t seed;
  } simple_rand;

  const int kPositionScale = 10;
  math::Size rand_area(output_surface_size().width() / kPositionScale + 1,
                       output_surface_size().height() / kPositionScale + 1);

  // Add a bunch of random rectangles with varying colors and opacity. Limit
  // opacity to be less than 100% so that previous rectangles are not totally
  // overwritten. Also leave a gap at the edges of the rectangles so that
  // adjacent rects are not considered intersecting.
  CompositionNode::Builder composition_builder;
  for (int i = 0; i < 400; ++i) {
    // The evaluation order of function call parameters is not guaranteed.
    // To maintain determinism, explicitly calculate the parameters before
    // calling the relevant functions.
    int x1 = simple_rand() % rand_area.width();
    int x2 = simple_rand() % rand_area.width();
    int y1 = simple_rand() % rand_area.height();
    int y2 = simple_rand() % rand_area.height();
    float r = (simple_rand() % 256) / 255.0f;
    float g = (simple_rand() % 256) / 255.0f;
    float b = (simple_rand() % 256) / 255.0f;
    float a = (simple_rand() % 5) * 0.1f + 0.1f;
    composition_builder.AddChild(new RectNode(
        math::RectF(
            std::min(x1, x2) * kPositionScale + 0.1f,
            std::min(y1, y2) * kPositionScale + 0.1f,
            std::abs(x1 - x2) * kPositionScale - 0.2f,
            std::abs(y1 - y2) * kPositionScale - 0.2f),
        scoped_ptr<Brush>(new SolidColorBrush(ColorRGBA(r, g, b, a)))));
  }

  TestTree(new CompositionNode(composition_builder.Pass()));
}

namespace {

// Creates a texture containing a 3x3 grid of colors each of the provided
// color but with differing alpha settings.  This is useful for checking
// that image transparency is working properly.
scoped_refptr<Image> CreateTransparencyCheckersUnpremultipliedAlphaImage(
    ResourceProvider* resource_provider, const SizeF& dimensions, uint8_t r,
    uint8_t g, uint8_t b) {
  std::vector<RGBAWord> row_colors;
  row_colors.push_back(RGBAWord(r, g, b, 0));
  row_colors.push_back(RGBAWord(r, g, b, 63));
  row_colors.push_back(RGBAWord(r, g, b, 127));
  return CreateAdditiveColorGridImage(resource_provider, dimensions, row_colors,
                                      row_colors,
                                      render_tree::kAlphaFormatUnpremultiplied);
}

scoped_refptr<Image> CreateTransparencyCheckersPremultipliedAlphaImage(
    ResourceProvider* resource_provider, const SizeF& dimensions, uint8_t r,
    uint8_t g, uint8_t b) {
  std::vector<RGBAWord> row_colors;
  row_colors.push_back(RGBAWord(0, 0, 0, 0));
  row_colors.push_back(
      RGBAWord((r * 63) / 255, (g * 63) / 255, (b * 63) / 255, 63));
  row_colors.push_back(
      RGBAWord((r * 127) / 255, (g * 127) / 255, (b * 127) / 255, 127));
  return CreateAdditiveColorGridImage(resource_provider, dimensions, row_colors,
                                      row_colors,
                                      render_tree::kAlphaFormatPremultiplied);
}

// This function places an image of a grid of differing transparencies on top
// of a solid color rectangle.  It ensures that alpha blending works fine
// with images.  It is factored into its own function so that color can be
// specified as a parameter.
scoped_refptr<Node> CreateTransparencyImageRenderTree(
    ResourceProvider* resource_provider, const SizeF& output_dimensions,
    uint8_t r, uint8_t g, uint8_t b, AlphaFormat alpha_format) {
  CompositionNode::Builder composition_builder;

  composition_builder.AddChild(new RectNode(
      RectF(25, 25, output_dimensions.width() / 2,
            output_dimensions.height() / 2),
      scoped_ptr<Brush>(new SolidColorBrush(ColorRGBA(1.0, 0.0, 0.0, 1)))));

  scoped_refptr<Image> transparency_image;
  if (alpha_format == render_tree::kAlphaFormatPremultiplied) {
    transparency_image = CreateTransparencyCheckersPremultipliedAlphaImage(
        resource_provider,
        SizeF(output_dimensions.width() / 2, output_dimensions.height() / 2),
        r, g, b);
  } else {
    transparency_image = CreateTransparencyCheckersUnpremultipliedAlphaImage(
        resource_provider,
        SizeF(output_dimensions.width() / 2, output_dimensions.height() / 2),
        r, g, b);
  }

  composition_builder.AddChild(
      new ImageNode(transparency_image, Vector2dF(40, 40)));

  return new CompositionNode(composition_builder.Pass());
}

}  // namespace

TEST_F(
    PixelTest,
    ImageOfBlackTransparentGridOverlappingSolidRectUsingUnpremultipliedAlpha) {
  if (GetResourceProvider()->AlphaFormatSupported(
          render_tree::kAlphaFormatUnpremultiplied)) {
    TestTree(CreateTransparencyImageRenderTree(
        GetResourceProvider(), output_surface_size(), 0, 0, 0,
        render_tree::kAlphaFormatUnpremultiplied));
  }
}

TEST_F(PixelTest,
       ImageOfWhiteTransparentGridOverlappingSolidRectUnpremultipliedAlpha) {
  if (GetResourceProvider()->AlphaFormatSupported(
          render_tree::kAlphaFormatUnpremultiplied)) {
    TestTree(CreateTransparencyImageRenderTree(
        GetResourceProvider(), output_surface_size(), 255, 255, 255,
        render_tree::kAlphaFormatUnpremultiplied));
  }
}

TEST_F(PixelTest,
       ImageOfBlackTransparentGridOverlappingSolidRectPremultipliedAlpha) {
  if (GetResourceProvider()->AlphaFormatSupported(
          render_tree::kAlphaFormatPremultiplied)) {
    TestTree(CreateTransparencyImageRenderTree(
        GetResourceProvider(), output_surface_size(), 0, 0, 0,
        render_tree::kAlphaFormatPremultiplied));
  }
}

TEST_F(PixelTest,
       ImageOfWhiteTransparentGridOverlappingSolidRectPremultipliedAlpha) {
  if (GetResourceProvider()->AlphaFormatSupported(
          render_tree::kAlphaFormatPremultiplied)) {
    TestTree(CreateTransparencyImageRenderTree(
        GetResourceProvider(), output_surface_size(), 255, 255, 255,
        render_tree::kAlphaFormatPremultiplied));
  }
}

namespace {

scoped_refptr<GlyphBuffer> CreateGlyphBuffer(
    ResourceProvider* resource_provider, FontStyle font_style, int font_size,
    const std::string& text) {
  scoped_refptr<Font> font =
      resource_provider->GetLocalTypeface("Roboto", font_style)
          ->CreateFontWithSize(font_size);
  return resource_provider->CreateGlyphBuffer(text, font);
}

// Convenience method for creating a compositing node that transforms a single
// child.
scoped_refptr<MatrixTransformNode> TransformRenderTree(
    const scoped_refptr<Node>& node, const Matrix3F& transform) {
  return new MatrixTransformNode(node, transform);
}

// Helper method to create a text node positioned on the surface such that it
// is within the visible area and ready to be tested.
scoped_refptr<Node> CreateTextNodeWithinSurface(
    ResourceProvider* resource_provider, const std::string& text,
    FontStyle font_style, int font_size, const ColorRGBA& color,
    const std::vector<Shadow>& shadows) {
  scoped_refptr<render_tree::GlyphBuffer> glyph_buffer =
      CreateGlyphBuffer(resource_provider, font_style, font_size, text);
  RectF bounds(glyph_buffer->GetBounds());

  TextNode::Builder builder(Vector2dF(-bounds.x(), -bounds.y()), glyph_buffer,
                            color);
  if (!shadows.empty()) {
    builder.shadows.emplace(shadows);
  }
  return new TextNode(builder);
}

scoped_refptr<Node> CreateTextNodeWithinSurface(
    ResourceProvider* resource_provider, const std::string& text,
    FontStyle font_style, int font_size, const ColorRGBA& color) {
  return CreateTextNodeWithinSurface(resource_provider, text, font_style,
                                     font_size, color, std::vector<Shadow>());
}

}  // namespace

TEST_F(PixelTest, SimpleTextIn20PtFont) {
  TestTree(CreateTextNodeWithinSurface(GetResourceProvider(), "Cobalt",
                                       FontStyle(), 20,
                                       ColorRGBA(0, 0, 0, 1.0)));
}

TEST_F(PixelTest, SimpleTextIn40PtFont) {
  TestTree(CreateTextNodeWithinSurface(GetResourceProvider(), "Cobalt",
                                       FontStyle(), 40,
                                       ColorRGBA(0, 0, 0, 1.0)));
}

TEST_F(PixelTest, SimpleTextIn80PtFont) {
  TestTree(CreateTextNodeWithinSurface(GetResourceProvider(), "Cobalt",
                                       FontStyle(), 80,
                                       ColorRGBA(0, 0, 0, 1.0)));
}

TEST_F(PixelTest, SimpleTextIn500PtFont) {
  // The glyphs in this test are usually too large to fit in any atlas.
  TestTree(CreateTextNodeWithinSurface(GetResourceProvider(), "Cobalt",
                                       FontStyle(), 500,
                                       ColorRGBA(0, 0, 0, 1.0)));
}

TEST_F(PixelTest, RedTextIn500PtFont) {
  // The glyphs in this test are usually too large to fit in any atlas.
  // Make sure that this works well with colors other than black.
  TestTree(CreateTextNodeWithinSurface(GetResourceProvider(), "Cobalt",
                                       FontStyle(), 500,
                                       ColorRGBA(1.0f, 0, 0, 1.0)));
}

TEST_F(PixelTest, ShearedText) {
  TestTree(new MatrixTransformNode(
      CreateTextNodeWithinSurface(GetResourceProvider(), "Cobalt", FontStyle(),
                                  80, ColorRGBA(1.0f, 0, 0, 1.0)),
      Matrix3F::FromValues(1, 1, 0, 0, 1, 0, 0, 0, 1)));
}

TEST_F(PixelTest, SimpleText40PtFontWithCharacterLowerThanBaseline) {
  TestTree(CreateTextNodeWithinSurface(GetResourceProvider(), "Berry jam",
                                       FontStyle(), 40,
                                       ColorRGBA(0, 0, 0, 1.0)));
}

TEST_F(PixelTest, SimpleTextInRed40PtFont) {
  TestTree(CreateTextNodeWithinSurface(GetResourceProvider(), "Cobalt",
                                       FontStyle(), 40,
                                       ColorRGBA(1.0, 0, 0, 1.0)));
}

namespace {
scoped_refptr<Node> CreateTextNodeWithBackgroundColor(
    ResourceProvider* resource_provider, const std::string& text,
    FontStyle font_style, int font_size, const ColorRGBA& text_color,
    const ColorRGBA& background_color) {
  scoped_refptr<render_tree::GlyphBuffer> glyph_buffer =
      CreateGlyphBuffer(resource_provider, font_style, font_size, text);
  RectF bounds(glyph_buffer->GetBounds());

  CompositionNode::Builder composition_builder;
  composition_builder.AddChild(
      new RectNode(RectF(bounds.width(), bounds.height()),
                   scoped_ptr<Brush>(new SolidColorBrush(background_color))));
  composition_builder.AddChild(new TextNode(Vector2dF(-bounds.x(), -bounds.y()),
                                            glyph_buffer, text_color));

  return new CompositionNode(composition_builder.Pass());
}
}  // namespace

TEST_F(PixelTest, RedTextOnBlueIn40PtFont) {
  TestTree(CreateTextNodeWithBackgroundColor(
      GetResourceProvider(), "Berry jam", FontStyle(), 40,
      ColorRGBA(1.0, 0, 0, 1.0), ColorRGBA(0, 0, 1.0, 1.0)));
}

TEST_F(PixelTest, WhiteTextOnBlackIn40PtFont) {
  TestTree(CreateTextNodeWithBackgroundColor(
      GetResourceProvider(), "Berry jam", FontStyle(), 40,
      ColorRGBA(1.0, 1.0, 1.0, 1.0), ColorRGBA(0, 0, 0, 1.0)));
}

TEST_F(PixelTest, TransparentBlackTextOnRedIn40PtFont) {
  // Print the string "Berry jam" with transparency infront of a red rectangle.
  TestTree(CreateTextNodeWithBackgroundColor(
      GetResourceProvider(), "Berry jam", FontStyle(), 40,
      ColorRGBA(0, 0, 0, 0.5), ColorRGBA(1.0, 0.0, 0.0, 1.0)));
}

namespace {
// Creates a multiplane YUV image where each channel, Y, U and V are stored
// in their own planes.  The I420 format dictates that the U and V planes have
// half the columns and half the rows of the Y plane.
scoped_refptr<Image> MakeI420Image(ResourceProvider* resource_provider,
                                   const Size& image_size) {
  // The alpha format doesn't really matter here, but we still need to pick one
  // that the resource provider indicates it supports.
  render_tree::AlphaFormat alpha_format =
      render_tree::kAlphaFormatUnpremultiplied;
  if (!resource_provider->AlphaFormatSupported(alpha_format)) {
    alpha_format = render_tree::kAlphaFormatPremultiplied;
  }
  CHECK(resource_provider->AlphaFormatSupported(alpha_format));

  static const int kMaxBytesUsed =
      image_size.width() * image_size.height() * 3 / 2;

  scoped_ptr<RawImageMemory> image_memory =
      resource_provider->AllocateRawImageMemory(kMaxBytesUsed, 256);

  // Setup information about the Y plane.
  ImageDataDescriptor y_plane_descriptor(image_size,
                                         render_tree::kPixelFormatY8,
                                         alpha_format, image_size.width());
  intptr_t y_plane_memory_offset = 0;
  uint8_t* y_plane_memory = image_memory->GetMemory() + y_plane_memory_offset;
  float y_plane_inverse_height = 1.0f / y_plane_descriptor.size.height();
  for (int r = 0; r < y_plane_descriptor.size.height(); ++r) {
    for (int c = 0; c < y_plane_descriptor.size.width(); ++c) {
      y_plane_memory[c] =
          static_cast<uint8_t>(255.0f * r * y_plane_inverse_height);
    }
    y_plane_memory += y_plane_descriptor.pitch_in_bytes;
  }

  // Setup information about the U plane.
  ImageDataDescriptor u_plane_descriptor(
      Size(image_size.width() / 2, image_size.height() / 2),
      render_tree::kPixelFormatU8, alpha_format, image_size.width() / 2);
  intptr_t u_plane_memory_offset =
      y_plane_memory_offset +
      y_plane_descriptor.pitch_in_bytes * y_plane_descriptor.size.height();
  uint8_t* u_plane_memory = image_memory->GetMemory() + u_plane_memory_offset;
  float u_plane_inverse_width = 1.0f / u_plane_descriptor.size.width();
  for (int r = 0; r < u_plane_descriptor.size.height(); ++r) {
    for (int c = 0; c < u_plane_descriptor.size.width(); ++c) {
      u_plane_memory[c] =
          static_cast<uint8_t>(255.0f * c * u_plane_inverse_width);
    }
    u_plane_memory += u_plane_descriptor.pitch_in_bytes;
  }

  // Setup information about the V plane.
  ImageDataDescriptor v_plane_descriptor(
      Size(image_size.width() / 2, image_size.height() / 2),
      render_tree::kPixelFormatV8, alpha_format, image_size.width() / 2);
  intptr_t v_plane_memory_offset =
      u_plane_memory_offset +
      u_plane_descriptor.pitch_in_bytes * u_plane_descriptor.size.height();
  uint8_t* v_plane_memory = image_memory->GetMemory() + v_plane_memory_offset;
  float v_plane_inverse_width = 1.0f / v_plane_descriptor.size.width();
  for (int r = 0; r < v_plane_descriptor.size.height(); ++r) {
    for (int c = 0; c < v_plane_descriptor.size.width(); ++c) {
      v_plane_memory[c] =
          255 - static_cast<uint8_t>(255.0f * c * v_plane_inverse_width);
    }
    v_plane_memory += v_plane_descriptor.pitch_in_bytes;
  }

  MultiPlaneImageDataDescriptor image_data_descriptor(
      render_tree::kMultiPlaneImageFormatYUV3PlaneBT709);
  image_data_descriptor.AddPlane(y_plane_memory_offset, y_plane_descriptor);
  image_data_descriptor.AddPlane(u_plane_memory_offset, u_plane_descriptor);
  image_data_descriptor.AddPlane(v_plane_memory_offset, v_plane_descriptor);

  return resource_provider->CreateMultiPlaneImageFromRawMemory(
      image_memory.Pass(), image_data_descriptor);
}

// The software rasterizer does not support NV12 images.
#if NV12_TEXTURE_SUPPORTED

// Creates a two plane YUV image where the Y channel is stored as a
// single-channel image plane and the U and V channels are interleaved in a
// second image plane. The NV12 format dictates that the UV plane has the same
// number of columns and half the rows of the Y plane.
scoped_refptr<Image> MakeNV12Image(ResourceProvider* resource_provider,
                                   const Size& image_size) {
  // The alpha format doesn't really matter here, but we still need to pick one
  // that the resource provider indicates it supports.
  render_tree::AlphaFormat alpha_format =
      render_tree::kAlphaFormatUnpremultiplied;
  if (!resource_provider->AlphaFormatSupported(alpha_format)) {
    alpha_format = render_tree::kAlphaFormatPremultiplied;
  }
  CHECK(resource_provider->AlphaFormatSupported(alpha_format));

  static const int kMaxBytesUsed =
      image_size.width() * image_size.height() * 3 / 2;

  scoped_ptr<RawImageMemory> image_memory =
      resource_provider->AllocateRawImageMemory(kMaxBytesUsed, 256);

  // Setup information about the Y plane.
  ImageDataDescriptor y_plane_descriptor(image_size,
                                         render_tree::kPixelFormatY8,
                                         alpha_format, image_size.width());
  intptr_t y_plane_memory_offset = 0;
  uint8_t* y_plane_memory = image_memory->GetMemory() + y_plane_memory_offset;
  float y_plane_inverse_height = 1.0f / y_plane_descriptor.size.height();
  for (int r = 0; r < y_plane_descriptor.size.height(); ++r) {
    for (int c = 0; c < y_plane_descriptor.size.width(); ++c) {
      y_plane_memory[c] =
          static_cast<uint8_t>(255.0f * r * y_plane_inverse_height);
    }
    y_plane_memory += y_plane_descriptor.pitch_in_bytes;
  }

  // Setup information about the UV plane.
  ImageDataDescriptor uv_plane_descriptor(
      Size(image_size.width() / 2, image_size.height() / 2),
      render_tree::kPixelFormatUV8, alpha_format, image_size.width());
  intptr_t uv_plane_memory_offset =
      y_plane_memory_offset +
      y_plane_descriptor.pitch_in_bytes * y_plane_descriptor.size.height();
  uint8_t* uv_plane_memory = image_memory->GetMemory() + uv_plane_memory_offset;
  float uv_plane_inverse_width = 1.0f / uv_plane_descriptor.size.width();
  for (int r = 0; r < uv_plane_descriptor.size.height(); ++r) {
    for (int c = 0; c < uv_plane_descriptor.size.width(); ++c) {
      uv_plane_memory[2 * c] =
          static_cast<uint8_t>(255.0f * c * uv_plane_inverse_width);
      uv_plane_memory[2 * c + 1] =
          255 - static_cast<uint8_t>(255.0f * c * uv_plane_inverse_width);
    }
    uv_plane_memory += uv_plane_descriptor.pitch_in_bytes;
  }

  MultiPlaneImageDataDescriptor image_data_descriptor(
      render_tree::kMultiPlaneImageFormatYUV2PlaneBT709);
  image_data_descriptor.AddPlane(y_plane_memory_offset, y_plane_descriptor);
  image_data_descriptor.AddPlane(uv_plane_memory_offset, uv_plane_descriptor);

  return resource_provider->CreateMultiPlaneImageFromRawMemory(
      image_memory.Pass(), image_data_descriptor);
}
#endif  // #if NV12_TEXTURE_SUPPORTED
}  // namespace

scoped_refptr<Image> MakeUYVYImage(ResourceProvider* resource_provider,
                                   const Size& image_size) {
  // Our UYVY image is a different pattern than the other YUV Make*Image()
  // functions in order to better test how all channels change and interpolate
  // in both the horizontal and vertical directions.
  render_tree::AlphaFormat alpha_format = render_tree::kAlphaFormatOpaque;
  CHECK(resource_provider->AlphaFormatSupported(alpha_format));

  Size uv_size = image_size;
  uv_size.set_width(image_size.width() / 2);

  scoped_ptr<ImageData> image_data = resource_provider->AllocateImageData(
      uv_size, render_tree::kPixelFormatUYVY, alpha_format);

  float x_step = 1.0f / (uv_size.width() - 1);
  float y_step = 1.0f / (uv_size.height() - 1);
  for (int i = 0; i < uv_size.height(); ++i) {
    uint8_t* pixel_data = image_data->GetMemory() +
                          image_data->GetDescriptor().pitch_in_bytes * i;
    float y = i * y_step;
    for (int j = 0; j < uv_size.width(); ++j) {
      float x1 = j * x_step;
      float x2 = (j + 0.5f) * x_step;

      int pixel_offset = j * 4;

      float radius1 = math::Vector2dF(x1 - 0.5f, y - 0.5f).Length() * 2;
      float radius2 = math::Vector2dF(x2 - 0.5f, y - 0.5f).Length() * 2;

      pixel_data[pixel_offset + 0] = std::min(255.0f, radius1 * 255.0f);
      pixel_data[pixel_offset + 1] = std::min(255.0f, radius1 * 255.0f);
      pixel_data[pixel_offset + 2] = 255 - std::min(255.0f, radius1 * 255.0f);
      pixel_data[pixel_offset + 3] = std::min(255.0f, radius2 * 255.0f);
    }
  }

  return resource_provider->CreateImage(image_data.Pass());
}

// Makes an image where the Y values alternate between 0 and 255.  This is
// useful for testing y value filtering, especially when a small image of this
// form is scaled up.
scoped_refptr<Image> MakeAlternatingYUYVYImage(
    ResourceProvider* resource_provider, const Size& image_size) {
  // Our UYVY image is a different pattern than the other YUV Make*Image()
  // functions in order to better test how all channels change and interpolate
  // in both the horizontal and vertical directions.
  render_tree::AlphaFormat alpha_format = render_tree::kAlphaFormatOpaque;
  CHECK(resource_provider->AlphaFormatSupported(alpha_format));

  Size uv_size = image_size;
  uv_size.set_width(image_size.width() / 2);

  scoped_ptr<ImageData> image_data = resource_provider->AllocateImageData(
      uv_size, render_tree::kPixelFormatUYVY, alpha_format);

  float x_step = 1.0f / (uv_size.width() - 1);
  float y_step = 1.0f / (uv_size.height() - 1);
  for (int i = 0; i < uv_size.height(); ++i) {
    uint8_t* pixel_data = image_data->GetMemory() +
                          image_data->GetDescriptor().pitch_in_bytes * i;
    float y = i * y_step;
    for (int j = 0; j < uv_size.width(); ++j) {
      float x1 = j * x_step;
      float x2 = (j + 0.5f) * x_step;

      int pixel_offset = j * 4;

      pixel_data[pixel_offset + 0] = std::min(255.0f, 255.0f * y);
      pixel_data[pixel_offset + 1] = 0;
      pixel_data[pixel_offset + 2] = 255 - std::min(255.0f, 255.0f * y);
      pixel_data[pixel_offset + 3] = 255;
    }
  }

  return resource_provider->CreateImage(image_data.Pass());
}

#if !SB_HAS(BLITTER)
TEST_F(PixelTest, ThreePlaneYUVImageSupport) {
  // Tests that an ImageNode hooked up to a 3-plane YUV image works fine.
  scoped_refptr<Image> image =
      MakeI420Image(GetResourceProvider(), output_surface_size());

  TestTree(new ImageNode(image));
}

TEST_F(PixelTest, ThreePlaneYUVImageWithDestSizeDifferentFromImage) {
  // Tests that an ImageNode hooked up to a 3-plane YUV image works fine.
  scoped_refptr<Image> image =
      MakeI420Image(GetResourceProvider(), output_surface_size());

  TestTree(new ImageNode(image, RectF(100.0f, 100.0f)));
}

TEST_F(PixelTest, ThreePlaneYUVImageWithTransform) {
  SizeF half_output_size = ScaleSize(output_surface_size(), 0.5f, 0.5f);
  TestTree(new MatrixTransformNode(
      new ImageNode(
          MakeI420Image(GetResourceProvider(), output_surface_size())),
      TranslateMatrix(half_output_size.width(), half_output_size.height()) *
          ScaleMatrix(0.5f) * RotateMatrix(static_cast<float>(M_PI) / 4) *
          TranslateMatrix(-half_output_size.width(),
                          -half_output_size.height())));
}

TEST_F(PixelTest, ThreePlaneYUVImageWithReflection) {
  SizeF half_output_size = ScaleSize(output_surface_size(), 0.5f, 0.5f);
  TestTree(new MatrixTransformNode(
      new ImageNode(
          MakeI420Image(GetResourceProvider(), output_surface_size())),
      TranslateMatrix(half_output_size.width(), half_output_size.height()) *
          ScaleMatrix(1.0f, -1.0f) *
          TranslateMatrix(-half_output_size.width(),
                          -half_output_size.height())));
}

TEST_F(PixelTest, YUV422UYVYImageSupport) {
  if (!GetResourceProvider()->PixelFormatSupported(
          render_tree::kPixelFormatUYVY)) {
    return;
  }

  // Tests that an ImageNode hooked up to a UYVY image works fine.
  scoped_refptr<Image> image =
      MakeUYVYImage(GetResourceProvider(), output_surface_size());

  TestTree(new ImageNode(image));
}

TEST_F(PixelTest, YUV422UYVYImageScaledUpSupport) {
  if (!GetResourceProvider()->PixelFormatSupported(
          render_tree::kPixelFormatUYVY)) {
    return;
  }

  // Tests that an ImageNode hooked up to a UYVY image and then scaled onto
  // a larger screen space works fine.  This test is particularly important in
  // testing that the somewhat unique UYVY texture interpolation logic is
  // working properly.
  scoped_refptr<Image> image =
      MakeAlternatingYUYVYImage(GetResourceProvider(), math::Size(4, 4));

  TestTree(new ImageNode(image, math::Rect(output_surface_size())));
}
#endif  // !SB_HAS(BLITTER)

// The software rasterizer does not support NV12 images.
#if NV12_TEXTURE_SUPPORTED

TEST_F(PixelTest, TwoPlaneYUVImageSupport) {
  // Tests that an ImageNode hooked up to a 3-plane YUV image works fine.
  scoped_refptr<Image> image =
      MakeNV12Image(GetResourceProvider(), output_surface_size());

  TestTree(new ImageNode(image));
}

TEST_F(PixelTest, TwoPlaneYUVImageWithDestSizeDifferentFromImage) {
  // Tests that an ImageNode hooked up to a 3-plane YUV image works fine.
  scoped_refptr<Image> image =
      MakeNV12Image(GetResourceProvider(), output_surface_size());

  TestTree(new ImageNode(image, RectF(100.0f, 100.0f)));
}

TEST_F(PixelTest, TwoPlaneYUVImageWithTransform) {
  SizeF half_output_size = ScaleSize(output_surface_size(), 0.5f, 0.5f);
  TestTree(new MatrixTransformNode(
      new ImageNode(
          MakeNV12Image(GetResourceProvider(), output_surface_size())),
      TranslateMatrix(half_output_size.width(), half_output_size.height()) *
          ScaleMatrix(0.5f) * RotateMatrix(static_cast<float>(M_PI) / 4) *
          TranslateMatrix(-half_output_size.width(),
                          -half_output_size.height())));
}
#endif  // #if NV12_TEXTURE_SUPPORTED

TEST_F(PixelTest, ImageNodeLocalTransformRotationAndScale) {
  scoped_refptr<Image> image =
      CreateColoredCheckersImage(GetResourceProvider(), output_surface_size());

  TestTree(new ImageNode(
      image, RectF(image->GetSize()),
      RotateMatrix(static_cast<float>(M_PI) / 4.0f) * ScaleMatrix(0.5f)));
}

TEST_F(PixelTest, ImageNodeLocalTransformTranslation) {
  scoped_refptr<Image> image =
      CreateColoredCheckersImage(GetResourceProvider(), output_surface_size());

  TestTree(new ImageNode(image, RectF(image->GetSize()),
                         TranslateMatrix(0.5f, 0.5f)));
}

TEST_F(PixelTest, ImageNodeLocalTransformOfImageSmallerThanSurface) {
  scoped_refptr<Image> image =
      CreateColoredCheckersImage(GetResourceProvider(),
                                 ScaleSize(output_surface_size(), 0.5f, 0.5f));

  TestTree(new ImageNode(
      image, RectF(image->GetSize()),
      RotateMatrix(static_cast<float>(M_PI) / 4.0f) * ScaleMatrix(0.5f)));
}

TEST_F(PixelTest, ImageNodeLocalTransformAndExternalTransform) {
  scoped_refptr<Image> image =
      CreateColoredCheckersImage(GetResourceProvider(), output_surface_size());

  SizeF half_output_size = ScaleSize(output_surface_size(), 0.5f, 0.5f);

  TestTree(new MatrixTransformNode(
      new ImageNode(
          image, RectF(image->GetSize()),
          RotateMatrix(static_cast<float>(M_PI) / 4.0f) * ScaleMatrix(0.5f)),
      TranslateMatrix(half_output_size.width(), half_output_size.height()) *
          ScaleMatrix(0.5f) * RotateMatrix(static_cast<float>(M_PI) / 4) *
          TranslateMatrix(-half_output_size.width(),
                          -half_output_size.height())));
}

TEST_F(PixelTest, OpacityFilterOnRectNodeTest) {
  // Two boxes, a red one and a blue one, where the blue one overlaps with
  // the red one and is passed through an opacity filter with opacity set to
  // 0.5.
  CompositionNode::Builder composition_builder;

  composition_builder.AddChild(new RectNode(
      RectF(PointF(25, 25), ScaleSize(output_surface_size(), 0.5f, 0.5f)),
      scoped_ptr<Brush>(new SolidColorBrush(ColorRGBA(1.0, 0.0, 0.0, 1)))));

  composition_builder.AddChild(new FilterNode(
      OpacityFilter(0.5f),
      new RectNode(
          RectF(PointF(50, 50), ScaleSize(output_surface_size(), 0.5f, 0.5f)),
          scoped_ptr<Brush>(
              new SolidColorBrush(ColorRGBA(0.0, 0.0, 1.0, 1))))));

  TestTree(new CompositionNode(composition_builder.Pass()));
}

TEST_F(PixelTest, OpacityFilterOnImageNodeTest) {
  // Two boxes, a red one and a blue one, where the blue one overlaps with
  // the red one and is passed through an opacity filter with opacity set to
  // 0.5.
  CompositionNode::Builder composition_builder;

  composition_builder.AddChild(new RectNode(
      RectF(PointF(25, 25), ScaleSize(output_surface_size(), 0.5f, 0.5f)),
      scoped_ptr<Brush>(new SolidColorBrush(ColorRGBA(1.0, 0.0, 0.0, 1)))));

  composition_builder.AddChild(new FilterNode(
      OpacityFilter(0.5f),
      new ImageNode(CreateColoredCheckersImage(
                        GetResourceProvider(),
                        ScaleSize(output_surface_size(), 0.5f, 0.5f)),
                    Vector2dF(50, 50))));

  TestTree(new CompositionNode(composition_builder.Pass()));
}

TEST_F(PixelTest, OpacityFilterOnCompositionOfThreeRectsTest) {
  // Two boxes, a red one and a blue one, where the blue one overlaps with
  // the red one and is passed through an opacity filter with opacity set to
  // 0.5.
  CompositionNode::Builder composition_builder;

  composition_builder.AddChild(new RectNode(
      RectF(PointF(25, 25), ScaleSize(output_surface_size(), 0.5f, 0.5f)),
      scoped_ptr<Brush>(new SolidColorBrush(ColorRGBA(1.0, 0.0, 0.0, 1)))));

  composition_builder.AddChild(new MatrixTransformNode(
      new FilterNode(OpacityFilter(0.5f),
                     CreateCascadedRectsOfDifferentColors(
                         ScaleSize(output_surface_size(), 0.5f, 0.5f))),
      TranslateMatrix(10, 10)));

  TestTree(new CompositionNode(composition_builder.Pass()));
}

TEST_F(PixelTest, OpacityFilterWithinOpacityFilter) {
  // Create a cascade of three colored rectangles.  The bottom right and middle
  // rectangle are part of the same subtree, which is attached to a filter node
  // applying an opacity of 0.5.  The bottom right rectangle is itself attached
  // to a filter node applying an opacity of 0.5, so the totally opacity of
  // the bottom right rectangle should be 0.5 * 0.5 = 0.25, the middle rectangle
  // should have 0.5, and the top left rectangle should have an opacity of 1.
  CompositionNode::Builder sub_composition_builder;
  sub_composition_builder.AddChild(new RectNode(
      RectF(ScaleSize(output_surface_size(), 0.5f, 0.5f)),
      scoped_ptr<Brush>(new SolidColorBrush(ColorRGBA(0.0, 1.0, 0.0, 1)))));
  sub_composition_builder.AddChild(new FilterNode(
      OpacityFilter(0.5f),
      new RectNode(
          RectF(PointF(25, 25), ScaleSize(output_surface_size(), 0.5f, 0.5f)),
          scoped_ptr<Brush>(
              new SolidColorBrush(ColorRGBA(0.0, 0.0, 1.0, 1))))));
  scoped_refptr<CompositionNode> sub_composition =
      new CompositionNode(sub_composition_builder.Pass());

  CompositionNode::Builder composition_builder;
  composition_builder.AddChild(new RectNode(
      RectF(PointF(25, 25), ScaleSize(output_surface_size(), 0.5f, 0.5f)),
      scoped_ptr<Brush>(new SolidColorBrush(ColorRGBA(1.0, 0.0, 0.0, 1)))));

  composition_builder.AddChild(new MatrixTransformNode(
      new FilterNode(OpacityFilter(0.5f), sub_composition),
      TranslateMatrix(50, 50)));

  TestTree(new CompositionNode(composition_builder.Pass()));
}

TEST_F(PixelTest, OpacityFilterOnRotatedRectNodeTest) {
  // Two boxes, a red one and a blue one, where the blue one overlaps with
  // the red one and is passed through an opacity filter with opacity set to
  // 0.5.
  CompositionNode::Builder composition_builder;

  const SizeF rect_size(ScaleSize(output_surface_size(), 0.5f, 0.5f));

  composition_builder.AddChild(new RectNode(
      RectF(PointF(25, 25), rect_size),
      scoped_ptr<Brush>(new SolidColorBrush(ColorRGBA(1.0, 0.0, 0.0, 1)))));

  composition_builder.AddChild(new MatrixTransformNode(
      new FilterNode(
          OpacityFilter(0.5f),
          new RectNode(RectF(rect_size), scoped_ptr<Brush>(new SolidColorBrush(
                                             ColorRGBA(0.0, 0.0, 1.0, 1))))),
      TranslateMatrix(50, 50) *
          TranslateMatrix(rect_size.width() / 2, rect_size.height() / 2) *
          RotateMatrix(static_cast<float>(M_PI) / 4) *
          TranslateMatrix(-rect_size.width() / 2, -rect_size.height() / 2)));

  TestTree(new CompositionNode(composition_builder.Pass()));
}

TEST_F(PixelTest, ViewportFilterWithTranslateAndScaleOnRectNodeTest) {
  FilterNode::Builder filter_node_builder(
      ViewportFilter(RectF(50, 50, 50, 50)),
      new RectNode(
          RectF(ScaleSize(output_surface_size(), 0.5f, 0.5f)),
          scoped_ptr<Brush>(new SolidColorBrush(ColorRGBA(0.0, 0.0, 1.0, 1)))));

  TestTree(
      new MatrixTransformNode(new FilterNode(filter_node_builder),
                              TranslateMatrix(-20, -20) * ScaleMatrix(1.5f)));
}

TEST_F(PixelTest, ViewportFilterOnCompositionOfThreeRectsTest) {
  FilterNode::Builder filter_node_builder(
      ViewportFilter(RectF(35, 35, 55, 55)),
      CreateCascadedRectsOfDifferentColors(
          ScaleSize(output_surface_size(), 0.5f, 0.5f)));

  TestTree(new FilterNode(filter_node_builder));
}

TEST_F(PixelTest, ViewportFilterOnTextNodeTest) {
  std::string text("Cobalt");
  scoped_refptr<render_tree::GlyphBuffer> glyph_buffer =
      CreateGlyphBuffer(GetResourceProvider(), FontStyle(), 5, text);
  RectF bounds(glyph_buffer->GetBounds());

  FilterNode::Builder filter_node_builder(
      ViewportFilter(RectF(4, 1, 5, 2)),
      new TextNode(Vector2dF(-bounds.x(), -bounds.y()), glyph_buffer,
                   ColorRGBA(0, 0, 0, 1)));

  TestTree(
      new MatrixTransformNode(new FilterNode(filter_node_builder),
                              TranslateMatrix(-120, 0) * ScaleMatrix(35.0f)));
}

TEST_F(PixelTest, ViewportFilterWithTransformOnTextNodeTest) {
  std::string text("Cobalt");
  scoped_refptr<render_tree::GlyphBuffer> glyph_buffer =
      CreateGlyphBuffer(GetResourceProvider(), FontStyle(), 5, text);
  RectF bounds(glyph_buffer->GetBounds());

  FilterNode::Builder filter_node_builder(
      ViewportFilter(RectF(4, 1, 5, 2)),
      new TextNode(Vector2dF(-bounds.x(), -bounds.y()), glyph_buffer,
                   ColorRGBA(0, 0, 0, 1)));

  TestTree(
      new MatrixTransformNode(new FilterNode(filter_node_builder),
                              TranslateMatrix(-130, 80) * ScaleMatrix(35.0f) *
                                  RotateMatrix(static_cast<float>(M_PI) / 8)));
}

TEST_F(PixelTest, ViewportFilterAndOpacityFilterOnTextNodeTest) {
  std::string text("Cobalt");
  scoped_refptr<render_tree::GlyphBuffer> glyph_buffer =
      CreateGlyphBuffer(GetResourceProvider(), FontStyle(), 5, text);
  RectF bounds(glyph_buffer->GetBounds());

  FilterNode::Builder filter_node_builder(
      new TextNode(Vector2dF(-bounds.x(), -bounds.y()), glyph_buffer,
                   ColorRGBA(0, 0, 0, 1)));
  filter_node_builder.opacity_filter = OpacityFilter(0.5f);
  filter_node_builder.viewport_filter = ViewportFilter(RectF(4, 1, 5, 2));

  TestTree(
      new MatrixTransformNode(new FilterNode(filter_node_builder),
                              TranslateMatrix(-130, 80) * ScaleMatrix(35.0f) *
                                  RotateMatrix(static_cast<float>(M_PI) / 8)));
}

TEST_F(PixelTest, ViewportFilterOnRotatedRectNodeTest) {
  const SizeF rect_size(ScaleSize(output_surface_size(), 0.5f, 0.5f));

  FilterNode::Builder filter_node_builder(
      ViewportFilter(RectF(70, 50, 100, 100)),
      new MatrixTransformNode(
          new RectNode(RectF(rect_size), scoped_ptr<Brush>(new SolidColorBrush(
                                             ColorRGBA(0.0, 0.0, 1.0, 1)))),
          TranslateMatrix(50, 100) *
              RotateMatrix(static_cast<float>(M_PI) / 4)));

  TestTree(new MatrixTransformNode(new FilterNode(filter_node_builder),
                                   TranslateMatrix(-20, 0)));
}

TEST_F(PixelTest, ScalingUpAnOpacityFilterTextDoesNotPixellate) {
  // An implementation of opacity filtering might choose to look only at the
  // subtree and render it as-is into an offscreen surface at its subtree's
  // size.  If it does this but then the filter tree is scaled up, pixellation
  // will occur.  This should not happen, implementations must recognize the
  // subtree's final size and render to an offscreen surface of that same size
  // to avoid pixellation.  While this test can demonstrate extreme aliasing,
  // this kind of problem is typically more subtle, resulting in one-pixel off
  // differences between a filtered render tree versus a non-filtered render
  // tree.
  std::string text("p");
  FontStyle font_style(FontStyle::kBoldWeight, FontStyle::kUprightSlant);
  scoped_refptr<render_tree::GlyphBuffer> glyph_buffer =
      CreateGlyphBuffer(GetResourceProvider(), font_style, 5, text);
  RectF bounds(glyph_buffer->GetBounds());

  TestTree(new MatrixTransformNode(
      new FilterNode(OpacityFilter(0.99f),
                     new TextNode(Vector2dF(-bounds.x(), -bounds.y()),
                                  glyph_buffer, ColorRGBA(0, 0, 0, 1))),
      TranslateMatrix(0, 40) * ScaleMatrix(35)));
}

TEST_F(PixelTest, RectNodeContainsBorderWithTranslation) {
  // RectNode with border can be translated.
  CompositionNode::Builder composition_builder;

  BorderSide border_side_1(20, render_tree::kBorderStyleSolid,
                           ColorRGBA(0.0, 1.0, 0.0, 1));
  scoped_ptr<Border> border_1(new Border(border_side_1));

  BorderSide border_side_2(10, render_tree::kBorderStyleSolid,
                           ColorRGBA(0.5, 0.5, 0.0, 1));
  scoped_ptr<Border> border_2(new Border(border_side_2));

  composition_builder.AddChild(new RectNode(
      RectF(PointF(25, 25), ScaleSize(output_surface_size(), 0.5f, 0.5f)),
      scoped_ptr<Brush>(new SolidColorBrush(ColorRGBA(1.0, 0.0, 0.0, 1))),
      border_1.Pass()));

  composition_builder.AddChild(new RectNode(
      RectF(PointF(50, 50), ScaleSize(output_surface_size(), 0.5f, 0.5f)),
      scoped_ptr<Brush>(new SolidColorBrush(ColorRGBA(0.0, 0.0, 1.0, 0.5))),
      border_2.Pass()));

  TestTree(new CompositionNode(composition_builder.Pass()));
}

TEST_F(PixelTest, RectNodeContainsBorderWithRotation) {
  // RectNode with border can be rotated.
  BorderSide border_side(10, render_tree::kBorderStyleSolid,
                         ColorRGBA(0.0, 1.0, 0.0, 1));
  scoped_ptr<Border> border(new Border(border_side));

  TestTree(new MatrixTransformNode(
      new RectNode(
          RectF(ScaleSize(output_surface_size(), 0.5f, 0.5f)),
          scoped_ptr<Brush>(new SolidColorBrush(ColorRGBA(1.0, 0.0, 0.0, 1))),
          border.Pass()),
      TranslateMatrix(40, 80) * RotateMatrix(static_cast<float>(M_PI) / 4.0f)));
}

TEST_F(PixelTest, RectNodeContainsBorderWithScale) {
  // RectNode with border can be scaled.
  BorderSide border_side(10, render_tree::kBorderStyleSolid,
                         ColorRGBA(0.0, 1.0, 0.0, 1));
  scoped_ptr<Border> border(new Border(border_side));

  TestTree(new MatrixTransformNode(
      new RectNode(
          RectF(ScaleSize(output_surface_size(), 0.3f, 0.3f)),
          scoped_ptr<Brush>(new SolidColorBrush(ColorRGBA(1.0, 0.0, 0.0, 1))),
          border.Pass()),
      ScaleMatrix(1.5f)));
}

TEST_F(PixelTest, RectNodeContainsBorderWithTranslationRotationAndScale) {
  // RectNode with border can be translated, rotated and scaled.
  BorderSide border_side(10, render_tree::kBorderStyleSolid,
                         ColorRGBA(0.0, 1.0, 0.0, 1));
  scoped_ptr<Border> border(new Border(border_side));

  TestTree(new MatrixTransformNode(
      new RectNode(
          RectF(ScaleSize(output_surface_size(), 0.3f, 0.3f)),
          scoped_ptr<Brush>(new SolidColorBrush(ColorRGBA(1.0, 0.0, 0.0, 1))),
          border.Pass()),
      TranslateMatrix(40, 80) * RotateMatrix(static_cast<float>(M_PI) / 4.0f) *
          ScaleMatrix(1.5f)));
}

TEST_F(PixelTest, RectNodeContainsSkinnyBorderWithTranslation) {
  // RectNode can be translated and drawn with skinny borders.
  BorderSide border_side(2, render_tree::kBorderStyleSolid,
                         ColorRGBA(0.0, 1.0, 0.0, 1));
  scoped_ptr<Border> border(new Border(border_side));

  TestTree(new MatrixTransformNode(
      new RectNode(
          RectF(ScaleSize(output_surface_size(), 0.5f, 0.5f)),
          scoped_ptr<Brush>(new SolidColorBrush(ColorRGBA(1.0, 0.0, 0.0, 1))),
          border.Pass()),
      TranslateMatrix(40, 80)));
}

// Creates a white/block checkered image where |dimensions| gives the size
// in pixels of the image to be constructed and |block_size| gives the size
// in pixels of each checker box.
scoped_refptr<Image> CreateCheckerImage(ResourceProvider* resource_provider,
                                        const SizeF& dimensions,
                                        const SizeF& block_size) {
  // Allocate memory to store the image data that we will subsequently generate.
  PixelFormat pixel_format = ChoosePixelFormat(resource_provider);
  scoped_ptr<ImageData> image_data = resource_provider->AllocateImageData(
      Size(static_cast<int>(dimensions.width()),
           static_cast<int>(dimensions.height())),
      pixel_format, render_tree::kAlphaFormatPremultiplied);

  RGBASwizzle rgba_swizzle = GetRGBASwizzleForPixelFormat(pixel_format);
  for (int i = 0; i < dimensions.height(); ++i) {
    uint8_t* pixel_data = image_data->GetMemory() +
                          image_data->GetDescriptor().pitch_in_bytes * i;
    int vertical_parity = (i / static_cast<int>(block_size.height())) & 1;
    for (int j = 0; j < dimensions.width(); ++j) {
      int horizontal_parity = (j / static_cast<int>(block_size.width())) & 1;
      uint8 color_value = (vertical_parity ^ horizontal_parity) ? 255 : 0;

      int pixel_offset = j * 4;
      for (int c = 0; c < 3; ++c) {
        pixel_data[pixel_offset + rgba_swizzle.channel[c]] = color_value;
      }
      pixel_data[pixel_offset + rgba_swizzle.channel[3]] = 255;
    }
  }

  return resource_provider->CreateImage(image_data.Pass());
}

scoped_refptr<Image> CreateCheckerImage(ResourceProvider* resource_provider,
                                        const SizeF& dimensions) {
  return CreateCheckerImage(resource_provider, dimensions,
                            math::ScaleSize(dimensions, 0.5f));
}

scoped_refptr<CompositionNode> CreatePixelEdgeCascade(
    const scoped_refptr<Image>& image, const SizeF& frame_size,
    const PointF& bottom_right_corner_of_top_layer, int num_cascades,
    const Matrix3F& texture_coord_matrix) {
  // Cascade multiple image rectangles by 1 pixel offsets so that we can
  // exagerate the bottom pixel not being grey.
  CompositionNode::Builder builder;
  for (int i = 0; i <= num_cascades; ++i) {
    PointF offset(-frame_size.width() + bottom_right_corner_of_top_layer.x() +
                      num_cascades - i,
                  -frame_size.height() + bottom_right_corner_of_top_layer.y() +
                      num_cascades - i);
    builder.AddChild(
        new ImageNode(image, RectF(offset, frame_size), texture_coord_matrix));
  }

  return new CompositionNode(builder.Pass());
}

scoped_refptr<CompositionNode> CreatePixelEdgeCascade(
    const scoped_refptr<Image>& image, const SizeF& frame_size,
    const PointF& bottom_right_corner_of_top_layer, int num_cascades) {
  return CreatePixelEdgeCascade(image, frame_size,
                                bottom_right_corner_of_top_layer, num_cascades,
                                Matrix3F::Identity());
}

// The following "ImageEdge*" tests verify that pixels at an image's edge are
// not interpolated with the wrapped pixel from the other side of the image.
// These tests all cascade a test ImageNode on top of each other so that each
// element of the cascade adds its 1 pixel edge to the stack, accumulating
// many rows of edge pixels so that any defect in the value of the edge pixel
// is exaggerated.
TEST_F(PixelTest, ImageEdgeNoWrap) {
  // Make sure the edge image pixels don't wrap around.  This might happen if
  // the image data dimensions are much smaller than its on-screen
  // rasterization.  In this case, pixels close to the edge may be assigned
  // texture coordinates that refer beyond the last pixel in the image, which
  // may in-turn result in interpolation with the wrapped texel.
  const SizeF kFrameSize(256, 256);
  const SizeF kImageSize(256, 16);
  const int kNumCascades = 20;

  TestTree(CreatePixelEdgeCascade(
      CreateCheckerImage(GetResourceProvider(), kImageSize), kFrameSize,
      PointF(100, 100), kNumCascades));
}

TEST_F(PixelTest, ImageEdgeNoWrapWithPixelCentersOffset) {
  // the image is rendered with a translation of just enough such that the top
  // left is the next pixel center within the viewport, and so we may
  // potentially interpolate with the wrapped pixel.
  // You should NOT see grey anywhere in this image.
  const SizeF kFrameSize(256, 256);
  const SizeF kImageSize(256, 16);
  const int kNumCascades = 20;

  TestTree(CreatePixelEdgeCascade(
      CreateCheckerImage(GetResourceProvider(), kImageSize), kFrameSize,
      PointF(100.0f, 100.51f), kNumCascades));
}

#if BILINEAR_FILTERING_SUPPORTED

TEST_F(PixelTest, ImagesAreLinearlyInterpolated) {
  // We want to make sure that image pixels are accessed through a bilinear
  // interpolation magnification filter.
  const SizeF kImageSize(2, 2);
  TestTree(new ImageNode(CreateCheckerImage(GetResourceProvider(), kImageSize),
                         RectF(output_surface_size())));
}

TEST_F(PixelTest, ZoomedInImagesDoNotWrapInterpolated) {
  // We want to make sure that image pixels are accessed through a bilinear
  // interpolation magnification filter, but that when we zoom in within an
  // image, we don't interpolate with offscreen texels.
  const SizeF kCheckerBlockSize(1, 1);
  const SizeF kImageSize(4, 4);
  TestTree(new ImageNode(
      CreateCheckerImage(GetResourceProvider(), kImageSize, kCheckerBlockSize),
      RectF(output_surface_size()),
      ScaleMatrix(2) * TranslateMatrix(-0.5f, -0.5f)));
}

#endif  // BILINEAR_FILTERING_SUPPORTED

#if !SB_HAS(BLITTER)
TEST_F(PixelTest, YUV3PlaneImagesAreLinearlyInterpolated) {
  // Tests that three plane YUV images are bilinearly interpolated.
  scoped_refptr<Image> image = MakeI420Image(GetResourceProvider(), Size(8, 8));

  TestTree(new ImageNode(image, RectF(output_surface_size())));
}
#endif  // !SB_HAS(BLITTER)

// The software rasterizer does not support NV12 images.
#if NV12_TEXTURE_SUPPORTED

TEST_F(PixelTest, YUV2PlaneImagesAreLinearlyInterpolated) {
  // Tests that two plane YUV images are bilinearly interpolated.
  scoped_refptr<Image> image = MakeNV12Image(GetResourceProvider(), Size(8, 8));

  TestTree(new ImageNode(image, RectF(output_surface_size())));
}

#endif  // #if NV12_TEXTURE_SUPPORTED

TEST_F(PixelTest, VeryLargeOpacityFilterDoesNotOccupyVeryMuchMemory) {
  // This test ensures that an opacity filter being applied to an extremely
  // large surface works just fine.  This test is itneresting because opacity
  // is likely implemented by rendering to an offscreen surface, but the
  // offscreen surface should never need to be larger than the canvas it will
  // be rendered to.
  const math::SizeF kOpacitySurfaceSize(400000, 400000);

  CompositionNode::Builder composition_builder;

  composition_builder.AddChild(new RectNode(
      RectF(PointF(25, 25), kOpacitySurfaceSize),
      scoped_ptr<Brush>(new SolidColorBrush(ColorRGBA(1.0, 0.0, 0.0, 1)))));

  // Add a small opacity rectangle to hopefully disable any simple optimization
  // that would just apply alpha to the color of the above RectNode.
  composition_builder.AddChild(new FilterNode(
      OpacityFilter(0.5f),
      new RectNode(
          RectF(PointF(50, 50), ScaleSize(output_surface_size(), 0.5f, 0.5f)),
          scoped_ptr<Brush>(
              new SolidColorBrush(ColorRGBA(0.0, 0.0, 1.0, 1))))));

  TestTree(new FilterNode(OpacityFilter(0.5f),
                          new CompositionNode(composition_builder.Pass())));
}

TEST_F(PixelTest, ChildrenOfCompositionThatStartsOffscreenAppear) {
  // This tests watches for regressions in a bug fix where viewport culling
  // of nodes was implemented incorrectly and resulted in children of a parent
  // node that started offscreen to be erroneously clipped.
  // by the rasterizer, and that we support multiple children.
  CompositionNode::Builder inner_composition_builder;

  const math::SizeF kRectSize(25, 25);
  inner_composition_builder.AddChild(new RectNode(
      RectF(PointF(0, 0), kRectSize),
      scoped_ptr<Brush>(new SolidColorBrush(ColorRGBA(1.0, 0.0, 0.0, 1)))));

  inner_composition_builder.AddChild(new RectNode(
      RectF(PointF(50, 50), kRectSize),
      scoped_ptr<Brush>(new SolidColorBrush(ColorRGBA(0.0, 1.0, 0.0, 1)))));

  inner_composition_builder.AddChild(new RectNode(
      RectF(PointF(100, 100), kRectSize),
      scoped_ptr<Brush>(new SolidColorBrush(ColorRGBA(0.0, 0.0, 1.0, 1)))));

  TestTree(new MatrixTransformNode(
      new CompositionNode(inner_composition_builder.Pass()),
      TranslateMatrix(-50, -50)));
}

TEST_F(PixelTest, TextNodesScaleDownSmoothly) {
  // This test checks that we can smoothly scale text from one size to
  // another.  In some font rasterizers, if they don't do sub-pixel font
  // glyph rendering, text glyphs will appear quantized into different
  // scale buckets.  This is undesireable as it makes animating text scale
  // very jittery.
  const std::string kText("ABCDEFGHIJKLMNOPQRSTUVWXYZ");

  FontStyle font_style(FontStyle::kBoldWeight, FontStyle::kUprightSlant);
  scoped_refptr<render_tree::GlyphBuffer> glyph_buffer =
      CreateGlyphBuffer(GetResourceProvider(), font_style, 14, kText);
  RectF bounds(glyph_buffer->GetBounds());

  CompositionNode::Builder text_collection_builder;
  for (int i = 0; i < 20; ++i) {
    text_collection_builder.AddChild(new MatrixTransformNode(
        new TextNode(Vector2dF(0, 0), glyph_buffer,
                     ColorRGBA(0.0f, 0.0f, 0.0f)),
        TranslateMatrix(5, 10 + i * 10) * ScaleMatrix(1 - i * 0.01f)));
  }

  TestTree(new CompositionNode(text_collection_builder.Pass()));
}

TEST_F(PixelTest, CircularViewportOverImage) {
  scoped_refptr<Image> image =
      CreateColoredCheckersImage(GetResourceProvider(), output_surface_size());

  TestTree(new FilterNode(
      ViewportFilter(RectF(25, 25, 150, 150), RoundedCorners(75, 75)),
      new ImageNode(image)));
}

TEST_F(PixelTest, CircularViewportOverWrappingImage) {
  scoped_refptr<Image> image =
      CreateColoredCheckersImage(GetResourceProvider(), output_surface_size());

  TestTree(new FilterNode(
      ViewportFilter(RectF(25, 25, 150, 150), RoundedCorners(75, 75)),
      new ImageNode(image, RectF(image->GetSize()),
                    RotateMatrix(static_cast<float>(M_PI / 6.0f)))));
}

TEST_F(PixelTest, CircularViewportOverZoomedInImage) {
  scoped_refptr<Image> image =
      CreateColoredCheckersImage(GetResourceProvider(), output_surface_size());

  TestTree(new FilterNode(
      ViewportFilter(RectF(25, 25, 150, 150), RoundedCorners(75, 75)),
      new ImageNode(image, RectF(image->GetSize()), ScaleMatrix(1.4f))));
}

TEST_F(PixelTest, TranslatedRightCircularViewportOverImage) {
  scoped_refptr<Image> image =
      CreateColoredCheckersImage(GetResourceProvider(), output_surface_size());

  TestTree(new MatrixTransformNode(
      new FilterNode(
          ViewportFilter(RectF(25, 25, 150, 150), RoundedCorners(75, 75)),
          new ImageNode(image)),
      TranslateMatrix(40, 0)));
}

TEST_F(PixelTest, FractionallyPositionedViewportsRenderCircularImages) {
  scoped_refptr<Image> image =
      CreateColoredCheckersImage(GetResourceProvider(), output_surface_size());

  TestTree(new FilterNode(
      ViewportFilter(RectF(0.3f, 0.3f, 100, 100)),
      new FilterNode(
          ViewportFilter(RectF(0, 0, 100, 100), RoundedCorners(50, 50)),
          new ImageNode(image, RectF(100, 100)))));
}

TEST_F(PixelTest, FractionallyPositionedViewportsRenderOpacityCircle) {
  scoped_refptr<Image> image =
      CreateColoredCheckersImage(GetResourceProvider(), output_surface_size());

  CompositionNode::Builder builder;
  builder.AddChild(new RectNode(
      RectF(RectF(100, 100)),
      scoped_ptr<Brush>(new SolidColorBrush(ColorRGBA(1.0, 0.0, 0.0, 1)))));
  builder.AddChild(new RectNode(
      RectF(RectF(50, 50, 100, 100)),
      scoped_ptr<Brush>(new SolidColorBrush(ColorRGBA(0.0, 1.0, 0.0, 1)))));

  TestTree(new FilterNode(
      ViewportFilter(RectF(0.3f, 0.3f, 100, 100)),
      new FilterNode(
          ViewportFilter(RectF(0, 0, 100, 100), RoundedCorners(50, 50)),
          new FilterNode(OpacityFilter(0.5f),
                         new CompositionNode(builder.Pass())))));
}

TEST_F(PixelTest, EllipticalViewportOverImage) {
  scoped_refptr<Image> image =
      CreateColoredCheckersImage(GetResourceProvider(), output_surface_size());

  TestTree(new FilterNode(
      ViewportFilter(RectF(25, 50, 150, 100), RoundedCorners(75, 50)),
      new ImageNode(image)));
}

TEST_F(PixelTest, LargeEllipticalViewportOverImage) {
  // This image has rounded corners that are just large enough to result in
  // older skia implementations using uniform values that overflow 16-bit float
  // exponents.
  scoped_refptr<Image> image =
      CreateColoredCheckersImage(GetResourceProvider(), math::Size(300, 100));

  TestTree(new FilterNode(
      ViewportFilter(RectF(0, 0, 300, 100), RoundedCorners(150, 50)),
      new ImageNode(image)));
}

TEST_F(PixelTest, EllipticalViewportOverCompositionOfImages) {
  scoped_refptr<Image> image =
      CreateColoredCheckersImage(GetResourceProvider(), output_surface_size());

  CompositionNode::Builder builder(Vector2dF(25, 50));
  builder.AddChild(new ImageNode(image, RectF(0, 0, 75, 50)));
  builder.AddChild(new ImageNode(image, RectF(75, 50, 75, 50)));
  TestTree(new FilterNode(
      ViewportFilter(RectF(25, 50, 150, 100), RoundedCorners(75, 50)),
      new CompositionNode(builder.Pass())));
}

TEST_F(PixelTest, EllipticalViewportOverWrappingImage) {
  scoped_refptr<Image> image =
      CreateColoredCheckersImage(GetResourceProvider(), output_surface_size());

  TestTree(new FilterNode(
      ViewportFilter(RectF(25, 50, 150, 100), RoundedCorners(75, 50)),
      new ImageNode(image, RectF(image->GetSize()),
                    RotateMatrix(static_cast<float>(M_PI / 6.0f)))));
}

TEST_F(PixelTest, EllipticalViewportOverZoomedInImage) {
  scoped_refptr<Image> image =
      CreateColoredCheckersImage(GetResourceProvider(), output_surface_size());

  TestTree(new FilterNode(
      ViewportFilter(RectF(25, 50, 150, 100), RoundedCorners(75, 50)),
      new ImageNode(image, RectF(image->GetSize()), ScaleMatrix(1.4f))));
}

TEST_F(PixelTest, OpacityOnRectAndEllipseMaskedImage) {
  // The opacity in this test will result in the rect and ellipse being rendered
  // to an offscreen surface.  The ellipse masking shaders may make use of
  // |gl_FragCoord|, which can return a flipped y value for at least the PS3
  // when rendering to a texture.  This test ensures that this is handled.
  scoped_refptr<Image> image =
      CreateColoredCheckersImage(GetResourceProvider(), output_surface_size());

  CompositionNode::Builder builder;
  builder.AddChild(new RectNode(
      RectF(SizeF(100, 100)),
      scoped_ptr<Brush>(new SolidColorBrush(ColorRGBA(0.0, 0.0, 0.0, 1)))));
  builder.AddChild(new FilterNode(
      ViewportFilter(RectF(0, 100, 150, 100), RoundedCorners(75, 50)),
      new ImageNode(image)));

  TestTree(
      new FilterNode(OpacityFilter(0.8f), new CompositionNode(builder.Pass())));
}

TEST_F(PixelTest, RoundedCornersViewportOverImage) {
  scoped_refptr<Image> image =
      CreateColoredCheckersImage(GetResourceProvider(), output_surface_size());

  TestTree(new FilterNode(
      ViewportFilter(RectF(25, 50, 150, 100), RoundedCorners(25, 15)),
      new ImageNode(image)));
}

TEST_F(PixelTest, ScaledThenRotatedRoundedCornersViewportOverImage) {
  scoped_refptr<Image> image =
      CreateColoredCheckersImage(GetResourceProvider(), output_surface_size());

  TestTree(new MatrixTransformNode(
      new FilterNode(
          ViewportFilter(RectF(25, 5, 150, 10), RoundedCorners(25, 2)),
          new ImageNode(image)),
      TranslateMatrix(-30, 130) *
      RotateMatrix(static_cast<float>(M_PI / 3.0f)) *
      ScaleMatrix(1.0f, 10.0f)));
}

TEST_F(PixelTest, RoundedCornersViewportOverWrappingImage) {
  scoped_refptr<Image> image =
      CreateColoredCheckersImage(GetResourceProvider(), output_surface_size());

  TestTree(new FilterNode(
      ViewportFilter(RectF(25, 50, 150, 100), RoundedCorners(25, 15)),
      new ImageNode(image, RectF(image->GetSize()),
                    RotateMatrix(static_cast<float>(M_PI / 6.0f)))));
}

TEST_F(PixelTest, RoundedCornersViewportOverZoomedInImage) {
  scoped_refptr<Image> image =
      CreateColoredCheckersImage(GetResourceProvider(), output_surface_size());

  TestTree(new FilterNode(
      ViewportFilter(RectF(25, 50, 150, 100), RoundedCorners(25, 15)),
      new ImageNode(image, RectF(image->GetSize()), ScaleMatrix(1.4f))));
}

TEST_F(PixelTest, RotatedRoundedCornersViewportOverImage) {
  scoped_refptr<Image> image =
      CreateColoredCheckersImage(GetResourceProvider(), output_surface_size());

  TestTree(new MatrixTransformNode(
      new FilterNode(
          ViewportFilter(RectF(25, 50, 150, 100), RoundedCorners(25, 15)),
          new ImageNode(image)),
      TranslateMatrix(-30, 60) *
          RotateMatrix(static_cast<float>(M_PI) / 6.0f)));
}

TEST_F(PixelTest, RoundedCornersViewportOverCascadedRects) {
  TestTree(new FilterNode(
      ViewportFilter(RectF(25, 50, 150, 100), RoundedCorners(25, 15)),
      CreateCascadedRectsOfDifferentColors(
          ScaleSize(output_surface_size(), 0.5f, 0.5f))));
}

TEST_F(PixelTest, StretchedRoundedCornersViewportOverCascadedRects) {
  TestTree(new MatrixTransformNode(
      new FilterNode(
          ViewportFilter(RectF(75, 25, 50, 50), RoundedCorners(20, 20)),
          CreateCascadedRectsOfDifferentColors(
              ScaleSize(output_surface_size(), 0.5f, 0.5f))),
      ScaleMatrix(1.0f, 2.0f)));
}

TEST_F(PixelTest, EllipticalViewportOverCascadeOfRects) {
  TestTree(new FilterNode(
      ViewportFilter(RectF(25, 50, 150, 100), RoundedCorners(75, 50)),
      CreateCascadedRectsOfDifferentColors(
          ScaleSize(output_surface_size(), 0.5f, 0.5f))));
}

TEST_F(PixelTest, CircularViewportOverCascadeOfRects) {
  TestTree(new FilterNode(
      ViewportFilter(RectF(25, 25, 150, 150), RoundedCorners(75, 75)),
      CreateCascadedRectsOfDifferentColors(
          ScaleSize(output_surface_size(), 0.5f, 0.5f))));
}

TEST_F(PixelTest, EllipticalViewportOverCascadeOfRectsWithOpacity) {
  FilterNode::Builder filter_builder(CreateCascadedRectsOfDifferentColors(
      ScaleSize(output_surface_size(), 0.5f, 0.5f)));
  filter_builder.opacity_filter.emplace(0.6);
  filter_builder.viewport_filter.emplace(RectF(25, 50, 150, 100),
                                         RoundedCorners(75, 50));
  TestTree(new FilterNode(filter_builder));
}

TEST_F(PixelTest, RoundedCornersDifferentViewportOverCascadedRects) {
  RoundedCorners rounded_corners(
      RoundedCorner(10.0f, 20.0f), RoundedCorner(30.0f, 20.0f),
      RoundedCorner(30.0f, 40.0f), RoundedCorner(50.0f, 40.0f));
  TestTree(
      new FilterNode(ViewportFilter(RectF(25, 50, 150, 100), rounded_corners),
                     CreateCascadedRectsOfDifferentColors(
                         ScaleSize(output_surface_size(), 0.5f, 0.5f))));
}

TEST_F(PixelTest, RoundedCornersDifferentViewportOverImage) {
  scoped_refptr<Image> image =
      CreateColoredCheckersImage(GetResourceProvider(), output_surface_size());

  RoundedCorners rounded_corners(
      RoundedCorner(10.0f, 20.0f), RoundedCorner(30.0f, 20.0f),
      RoundedCorner(30.0f, 40.0f), RoundedCorner(50.0f, 40.0f));

  TestTree(
      new FilterNode(ViewportFilter(RectF(25, 50, 150, 100), rounded_corners),
                     new ImageNode(image)));
}

namespace {
scoped_refptr<Node> CascadeNode(const Vector2dF& offset,
                                const Vector2dF& spacing,
                                const scoped_refptr<Node>& node,
                                int num_nodes) {
  CompositionNode::Builder builder;
  for (int i = 0; i < num_nodes; ++i) {
    builder.AddChild(new MatrixTransformNode(
        node, TranslateMatrix(offset.x() + i * spacing.x(),
                              offset.y() + i * spacing.y())));
  }
  return new CompositionNode(builder.Pass());
}
}  // namespace

TEST_F(PixelTest, RoundedCornersViewportOverTranslatedImage) {
  scoped_refptr<Image> image =
      CreateColoredCheckersImage(GetResourceProvider(), Size(100, 100));

  TestTree(new FilterNode(
      ViewportFilter(RectF(60.0f, 60.0f, 80.0f, 80.0f), RoundedCorners(25, 15)),
      new ImageNode(image, RectF(50.0f, 50.0f, 100.0f, 100.0f))));
}

TEST_F(PixelTest, RoundedCornersViewportOverCascadeOfImages) {
  scoped_refptr<Image> image =
      CreateColoredCheckersImage(GetResourceProvider(), Size(50, 50));

  TestTree(new FilterNode(
      ViewportFilter(RectF(25, 50, 100, 100), RoundedCorners(25, 15)),
      CascadeNode(Vector2dF(20.0f, 20.0f), Vector2dF(40.0f, 40.0f),
                  new ImageNode(image, RectF(50.0f, 50.0f)), 3)));
}

TEST_F(PixelTest, Width1Image) {
  // Ensure we can construct and render images that have a width of 1.
  scoped_refptr<Image> image =
      CreateColoredCheckersImage(GetResourceProvider(), Size(1, 100));

  TestTree(new ImageNode(image, RectF(100, 200)));
}

TEST_F(PixelTest, Height1Image) {
  // Ensure we can construct and render images that have a height of 1.
  scoped_refptr<Image> image =
      CreateColoredCheckersImage(GetResourceProvider(), Size(100, 1));

  TestTree(new ImageNode(image, RectF(200, 100)));
}

TEST_F(PixelTest, Area1Image) {
  // Ensure we can construct and render images that have an area of 1.
  scoped_refptr<Image> image =
      CreateColoredCheckersImage(GetResourceProvider(), Size(1, 1));

  TestTree(new ImageNode(image, RectF(100, 100)));
}

TEST_F(PixelTest, Width1Opacity) {
  // The rasterization of this render tree usually requires the implementation
  // to create an offscreen render target and draw into that.  This ensures that
  // the implementation can then support render targets of width 1.
  scoped_refptr<Image> image =
      CreateColoredCheckersImage(GetResourceProvider(), output_surface_size());

  FilterNode::Builder builder(
      CreateCascadedRectsOfDifferentColors(output_surface_size()));

  builder.viewport_filter.emplace(RectF(90.0f, 0.0f, 1.0f, 200.0f));
  builder.opacity_filter.emplace(0.5f);

  // Repeat our strip 20 pixels in a row so that the effect is more pronounced.
  TestTree(CascadeNode(Vector2dF(0.0f, 0.0f), Vector2dF(1.0f, 0.0f),
                       new FilterNode(builder), 20));
}

TEST_F(PixelTest, Height1Opacity) {
  // The rasterization of this render tree usually requires the implementation
  // to create an offscreen render target and draw into that.  This ensures that
  // the implementation can then support render targets of height 1.
  scoped_refptr<Image> image =
      CreateColoredCheckersImage(GetResourceProvider(), output_surface_size());

  FilterNode::Builder builder(
      CreateCascadedRectsOfDifferentColors(output_surface_size()));

  builder.viewport_filter.emplace(RectF(0.0f, 90.0f, 200.0f, 1.0f));
  builder.opacity_filter.emplace(0.5f);

  // Repeat our strip 20 pixels in a row so that the effect is more pronounced.
  TestTree(CascadeNode(Vector2dF(0.0f, 0.0f), Vector2dF(0.0f, 1.0f),
                       new FilterNode(builder), 20));
}

TEST_F(PixelTest, Area1Opacity) {
  // The rasterization of this render tree usually requires the implementation
  // to create an offscreen render target and draw into that.  This ensures that
  // the implementation can then support render targets of area 1.
  scoped_refptr<Image> image =
      CreateColoredCheckersImage(GetResourceProvider(), output_surface_size());

  FilterNode::Builder builder(
      CreateCascadedRectsOfDifferentColors(output_surface_size()));

  builder.viewport_filter.emplace(RectF(95.0f, 95.0f, 1.0f, 1.0f));
  builder.opacity_filter.emplace(0.5f);

  // Repeat our single pixel output into a 10x10 pixel grid.
  TestTree(CascadeNode(Vector2dF(0.0f, 0.0f), Vector2dF(0.0f, 1.0f),
                       CascadeNode(Vector2dF(0.0f, 0.0f), Vector2dF(1.0f, 0.0f),
                                   new FilterNode(builder), 10),
                       10));
}

namespace {

scoped_refptr<Node> CreateRoundedBorderRect(const SizeF& size,
                                            float border_width,
                                            const ColorRGBA& color) {
  BorderSide border_side(border_width, render_tree::kBorderStyleSolid, color);
  return new RectNode(RectF(size), make_scoped_ptr(new Border(border_side)),
                      make_scoped_ptr(new RoundedCorners(
                          size.width() / 4.0f, size.height() / 4.0f)));
}

scoped_refptr<Node> CreateEllipticalBorderRect(const SizeF& size,
                                               float border_width,
                                               const ColorRGBA& color) {
  BorderSide border_side(border_width, render_tree::kBorderStyleSolid, color);
  return new RectNode(RectF(size), make_scoped_ptr(new Border(border_side)),
                      make_scoped_ptr(new RoundedCorners(
                          size.width() / 2.0f, size.height() / 2.0f)));
}

}  // namespace

TEST_F(PixelTest, RoundedCornersThickBorder) {
  TestTree(new MatrixTransformNode(
      CreateRoundedBorderRect(ScaleSize(output_surface_size(), 0.5f, 0.5f),
                              15.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 1.0f)),
      TranslateMatrix(30.0f, 30.0f)));
}

TEST_F(PixelTest, RoundedCornersEachDifferentThickBorder) {
  RoundedCorners rounded_corners(
      RoundedCorner(10.0f, 20.0f), RoundedCorner(20.0f, 30.0f),
      RoundedCorner(30.0f, 40.0f), RoundedCorner(50.0f, 40.0f));
  BorderSide border_side(20.0f, render_tree::kBorderStyleSolid,
                         ColorRGBA(1.0f, 0.0f, 0.5f, 1.0f));
  TestTree(new RectNode(RectF(30, 30, 100, 100),
                        make_scoped_ptr(new Border(border_side)),
                        make_scoped_ptr(new RoundedCorners(rounded_corners))));
}

TEST_F(PixelTest, RoundedCornersThickBlueBorder) {
  TestTree(new MatrixTransformNode(
      CreateRoundedBorderRect(ScaleSize(output_surface_size(), 0.5f, 0.5f),
                              15.0f, ColorRGBA(0.0f, 0.0f, 1.0f, 1.0f)),
      TranslateMatrix(30.0f, 30.0f)));
}

TEST_F(PixelTest, CircularThickBorder) {
  TestTree(new MatrixTransformNode(
      CreateEllipticalBorderRect(ScaleSize(output_surface_size(), 0.5f, 0.5f),
                                 15.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 1.0f)),
      TranslateMatrix(30.0f, 30.0f)));
}

TEST_F(PixelTest, EllipticalThickBorder) {
  TestTree(new MatrixTransformNode(
      CreateEllipticalBorderRect(ScaleSize(output_surface_size(), 0.75f, 0.5f),
                                 15.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 1.0f)),
      TranslateMatrix(30.0f, 30.0f)));
}

TEST_F(PixelTest, SquishedEllipticalThickBorder) {
  // This test makes sure that we can properly render ellipses that are created
  // by squishing circles using a scaling MatrixTransformNode node.
  TestTree(new MatrixTransformNode(
      CreateEllipticalBorderRect(ScaleSize(output_surface_size(), 0.5f, 0.5f),
                                 15.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 1.0f)),
      ScaleMatrix(1.0f, 0.8f) * TranslateMatrix(30.0f, 30.0f)));
}

TEST_F(PixelTest, RoundedCornersSubPixelBorder) {
  // Renderers may take a different rendering path for drawing hairline paths.
  // This test checks that we cover for that.
  const SizeF kRectSize = ScaleSize(output_surface_size(), 0.5f, 0.5f);

  TestTree(CascadeNode(Vector2dF(10.0f, 10.0f), Vector2dF(0.4f, 0.4f),
                       CreateRoundedBorderRect(
                           kRectSize, 0.9f, ColorRGBA(0.0f, 0.0f, 0.0f, 1.0f)),
                       20));
}

TEST_F(PixelTest, RoundedCornersRectangularSubPixelBorder) {
  // Renderers may take a different rendering path for drawing hairline paths.
  // This test checks that we cover for that.
  const SizeF kRectSize = ScaleSize(output_surface_size(), 0.75f, 0.5f);

  TestTree(CascadeNode(Vector2dF(10.0f, 10.0f), Vector2dF(0.4f, 0.4f),
                       CreateRoundedBorderRect(
                           kRectSize, 0.9f, ColorRGBA(0.0f, 0.0f, 0.0f, 1.0f)),
                       20));
}

TEST_F(PixelTest, BlueRoundedCornersRectangularSubPixelBorder) {
  // Renderers may take a different rendering path for drawing hairline paths.
  // This test checks that we cover for that.
  const SizeF kRectSize = ScaleSize(output_surface_size(), 0.75f, 0.5f);

  TestTree(CascadeNode(Vector2dF(10.0f, 10.0f), Vector2dF(0.4f, 0.4f),
                       CreateRoundedBorderRect(
                           kRectSize, 0.9f, ColorRGBA(0.0f, 0.0f, 1.0f, 1.0f)),
                       20));
}

TEST_F(PixelTest, CircularSubPixelBorder) {
  // Renderers may take a different rendering path for drawing hairline paths.
  // This test checks that we cover for that.
  const SizeF kRectSize = ScaleSize(output_surface_size(), 0.5f, 0.5f);

  TestTree(CascadeNode(Vector2dF(10.0f, 10.0f), Vector2dF(0.4f, 0.4f),
                       CreateEllipticalBorderRect(
                           kRectSize, 0.9f, ColorRGBA(0.0f, 0.0f, 0.0f, 1.0f)),
                       20));
}

TEST_F(PixelTest, EllipticalSubPixelBorder) {
  // Renderers may take a different rendering path for drawing hairline paths.
  // This test checks that we cover for that.
  const SizeF kRectSize = ScaleSize(output_surface_size(), 0.75f, 0.5f);

  TestTree(CascadeNode(Vector2dF(10.0f, 10.0f), Vector2dF(0.4f, 0.4f),
                       CreateEllipticalBorderRect(
                           kRectSize, 0.9f, ColorRGBA(0.0f, 0.0f, 0.0f, 1.0f)),
                       20));
}

namespace {
std::pair<PointF, PointF> LinearGradientPointsFromDegrees(
    float ccw_degrees_from_right, const SizeF& frame_size, float frame_scale) {
  float ccw_radians_from_right = static_cast<float>(
      ccw_degrees_from_right * M_PI / 180.0f);

  // Scale |frame_size| by |frame_scale|, and offset the source and destination
  // points for the gradient so their midpoint coincides with the center of
  // the original frame.
  float offset_x = frame_size.width() * (1.0f - frame_scale) * 0.5f;
  float offset_y = frame_size.height() * (1.0f - frame_scale) * 0.5f;
  std::pair<PointF, PointF> source_and_dest =
      render_tree::LinearGradientPointsFromAngle(ccw_radians_from_right,
          ScaleSize(frame_size, frame_scale));
  source_and_dest.first.Offset(offset_x, offset_y);
  source_and_dest.second.Offset(offset_x, offset_y);
  return source_and_dest;
}

RectF ScaledCenteredSurface(const SizeF& frame_size, float scale) {
  math::PointF origin(frame_size.width() * (1.0f - scale) * 0.5f,
                      frame_size.height() * (1.0f - scale) * 0.5f);
  return math::RectF(origin, ScaleSize(frame_size, scale));
}
}  // namespace

TEST_F(PixelTest, LinearGradient2StopsLeftRight) {
  TestTree(new RectNode(
      ScaledCenteredSurface(output_surface_size(), 0.9f),
      scoped_ptr<Brush>(new LinearGradientBrush(
          LinearGradientPointsFromDegrees(0, output_surface_size(), 0.9f),
          ColorRGBA(1.0, 0.0, 0.0, 1), ColorRGBA(0.0, 1.0, 0.0, 1)))));
}

TEST_F(PixelTest, LinearGradient2StopsTopBottom) {
  TestTree(new RectNode(
      ScaledCenteredSurface(output_surface_size(), 0.9f),
      scoped_ptr<Brush>(new LinearGradientBrush(
          LinearGradientPointsFromDegrees(-90, output_surface_size(), 0.9f),
          ColorRGBA(1.0, 0.0, 0.0, 1), ColorRGBA(0.0, 1.0, 0.0, 1)))));
}

TEST_F(PixelTest, LinearGradient2Stops45Degrees) {
  TestTree(new RectNode(
      ScaledCenteredSurface(output_surface_size(), 0.9f),
      scoped_ptr<Brush>(new LinearGradientBrush(
          LinearGradientPointsFromDegrees(45, output_surface_size(), 0.9f),
          ColorRGBA(1.0, 0.0, 0.0, 1),
          ColorRGBA(0.0, 1.0, 0.0, 1)))));
}

TEST_F(PixelTest, LinearGradient2Stops315Degrees) {
  TestTree(new RectNode(
      ScaledCenteredSurface(output_surface_size(), 0.9f),
      scoped_ptr<Brush>(new LinearGradientBrush(
          LinearGradientPointsFromDegrees(315, output_surface_size(), 0.9f),
          ColorRGBA(1.0, 0.0, 0.0, 1),
          ColorRGBA(0.0, 1.0, 0.0, 1)))));
}

namespace {
ColorStopList Make3ColorEvenlySpacedStopList() {
  ColorStopList ret;
  ret.push_back(ColorStop(0.0f, ColorRGBA(0.0f, 0.0f, 1.0f, 1.0f)));
  ret.push_back(ColorStop(0.5f, ColorRGBA(0.0f, 1.0f, 0.0f, 1.0f)));
  ret.push_back(ColorStop(1.0f, ColorRGBA(1.0f, 0.0f, 1.0f, 1.0f)));
  return ret;
}
}  // namespace

TEST_F(PixelTest, LinearGradient3StopsLeftRightInset) {
  TestTree(new RectNode(
      ScaledCenteredSurface(output_surface_size(), 0.9f),
      scoped_ptr<Brush>(new LinearGradientBrush(
          LinearGradientPointsFromDegrees(0, output_surface_size(), 0.8f),
          Make3ColorEvenlySpacedStopList()))));
}

TEST_F(PixelTest, LinearGradient3StopsTopBottomInset) {
  TestTree(new RectNode(
      ScaledCenteredSurface(output_surface_size(), 0.9f),
      scoped_ptr<Brush>(new LinearGradientBrush(
          LinearGradientPointsFromDegrees(-90, output_surface_size(), 0.8f),
          Make3ColorEvenlySpacedStopList()))));
}

TEST_F(PixelTest, LinearGradient3Stops30DegreesInset) {
  TestTree(new RectNode(
      ScaledCenteredSurface(output_surface_size(), 0.9f),
      scoped_ptr<Brush>(new LinearGradientBrush(
          LinearGradientPointsFromDegrees(30, output_surface_size(), 0.8f),
          Make3ColorEvenlySpacedStopList()))));
}

TEST_F(PixelTest, LinearGradient3Stops210DegreesInset) {
  TestTree(new RectNode(
      ScaledCenteredSurface(output_surface_size(), 0.9f),
      scoped_ptr<Brush>(new LinearGradientBrush(
          LinearGradientPointsFromDegrees(210, output_surface_size(), 0.8f),
          Make3ColorEvenlySpacedStopList()))));
}

namespace {
ColorStopList Make5ColorEvenlySpacedStopList() {
  ColorStopList ret;
  ret.push_back(ColorStop(0.0f, ColorRGBA(1.0f, 0.0f, 0.0f, 1.0f)));
  ret.push_back(ColorStop(0.25f, ColorRGBA(1.0f, 1.0f, 0.0f, 1.0f)));
  ret.push_back(ColorStop(0.50f, ColorRGBA(0.0f, 1.0f, 1.0f, 1.0f)));
  ret.push_back(ColorStop(0.75f, ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f)));
  ret.push_back(ColorStop(1.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 1.0f)));
  return ret;
}
}  // namespace

TEST_F(PixelTest, LinearGradient5StopsLeftRightOutset) {
  TestTree(new RectNode(
      ScaledCenteredSurface(output_surface_size(), 0.9f),
      scoped_ptr<Brush>(new LinearGradientBrush(
          LinearGradientPointsFromDegrees(0, output_surface_size(), 1.0f),
          Make5ColorEvenlySpacedStopList()))));
}

TEST_F(PixelTest, LinearGradient5StopsTopBottomOutset) {
  TestTree(new RectNode(
      ScaledCenteredSurface(output_surface_size(), 0.9f),
      scoped_ptr<Brush>(new LinearGradientBrush(
          LinearGradientPointsFromDegrees(-90, output_surface_size(), 1.0f),
          Make5ColorEvenlySpacedStopList()))));
}

TEST_F(PixelTest, LinearGradient5Stops150DegreesOutset) {
  TestTree(new RectNode(
      ScaledCenteredSurface(output_surface_size(), 0.9f),
      scoped_ptr<Brush>(new LinearGradientBrush(
          LinearGradientPointsFromDegrees(150, output_surface_size(), 1.0f),
          Make5ColorEvenlySpacedStopList()))));
}

TEST_F(PixelTest, LinearGradient5Stops330DegreesOutset) {
  TestTree(new RectNode(
      ScaledCenteredSurface(output_surface_size(), 0.9f),
      scoped_ptr<Brush>(new LinearGradientBrush(
          LinearGradientPointsFromDegrees(330, output_surface_size(), 1.0f),
          Make5ColorEvenlySpacedStopList()))));
}

TEST_F(PixelTest, LinearGradientWithTransparencyOnWhiteBackground) {
  // This test also ensures that we are interpolating between gradient stops
  // within a premultiplied alpha space.  If this is not the case, you will
  // see a blackish transition between the transparent color stop.
  ColorStopList color_stop_list;
  color_stop_list.push_back(ColorStop(0.0f, ColorRGBA(1.0f, 0.0f, 0.0f, 1.0f)));
  color_stop_list.push_back(ColorStop(0.5f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f)));
  color_stop_list.push_back(ColorStop(1.0f, ColorRGBA(0.0f, 0.0f, 1.0f, 1.0f)));

  CompositionNode::Builder builder;
  builder.AddChild(new RectNode(RectF(output_surface_size()),
                                scoped_ptr<Brush>(new SolidColorBrush(
                                    ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f)))));

  builder.AddChild(
      new RectNode(RectF(output_surface_size()),
                   scoped_ptr<Brush>(new LinearGradientBrush(
                       PointF(0, 0), PointF(output_surface_size().width(), 0),
                       color_stop_list))));
  TestTree(new CompositionNode(builder.Pass()));
}

TEST_F(PixelTest, RadialGradient2Stops) {
  TestTree(new RectNode(
      ScaledCenteredSurface(output_surface_size(), 0.9f),
      scoped_ptr<Brush>(new RadialGradientBrush(
          PointF(75, 100), 75, ColorRGBA(1.0, 0.0, 0.0, 1),
          ColorRGBA(0.0, 1.0, 0.0, 1)))));
}

TEST_F(PixelTest, RadialGradient3Stops) {
  TestTree(new RectNode(
      ScaledCenteredSurface(output_surface_size(), 0.9f),
      scoped_ptr<Brush>(new RadialGradientBrush(
          PointF(75, 100), 75, Make3ColorEvenlySpacedStopList()))));
}

TEST_F(PixelTest, RadialGradient5Stops) {
  TestTree(new RectNode(
      ScaledCenteredSurface(output_surface_size(), 0.9f),
      scoped_ptr<Brush>(new RadialGradientBrush(
          PointF(75, 100), 75, Make5ColorEvenlySpacedStopList()))));
}

TEST_F(PixelTest, VerticalEllipseGradient2Stops) {
  TestTree(new RectNode(
      ScaledCenteredSurface(output_surface_size(), 0.9f),
      scoped_ptr<Brush>(new RadialGradientBrush(PointF(75, 100), 75, 100,
                                                ColorRGBA(1.0, 0.0, 0.0, 1),
                                                ColorRGBA(0.0, 1.0, 0.0, 1)))));
}

TEST_F(PixelTest, VerticalEllipseGradient3Stops) {
  TestTree(new RectNode(
      ScaledCenteredSurface(output_surface_size(), 0.9f),
      scoped_ptr<Brush>(new RadialGradientBrush(
          PointF(75, 100), 75, 100, Make3ColorEvenlySpacedStopList()))));
}

TEST_F(PixelTest, VerticalEllipseGradient5Stops) {
  TestTree(new RectNode(
      ScaledCenteredSurface(output_surface_size(), 0.9f),
      scoped_ptr<Brush>(new RadialGradientBrush(
          PointF(75, 100), 75, 100, Make5ColorEvenlySpacedStopList()))));
}

TEST_F(PixelTest, HorizontalEllipseGradient2Stops) {
  TestTree(new RectNode(
      ScaledCenteredSurface(output_surface_size(), 0.9f),
      scoped_ptr<Brush>(new RadialGradientBrush(PointF(75, 100), 100, 75,
                                                ColorRGBA(1.0, 0.0, 0.0, 1),
                                                ColorRGBA(0.0, 1.0, 0.0, 1)))));
}

TEST_F(PixelTest, HorizontalEllipseGradient3Stops) {
  TestTree(new RectNode(
      ScaledCenteredSurface(output_surface_size(), 0.9f),
      scoped_ptr<Brush>(new RadialGradientBrush(
          PointF(75, 100), 100, 75, Make3ColorEvenlySpacedStopList()))));
}

TEST_F(PixelTest, RadialGradientWithTransparencyOnWhiteBackground) {
  // This test also ensures that we are interpolating between gradient stops
  // within a premultiplied alpha space.  If this is not the case, you will
  // see a blackish transition between the transparent color stop.
  ColorStopList color_stop_list;
  color_stop_list.push_back(ColorStop(0.0f, ColorRGBA(1.0f, 0.0f, 0.0f, 1.0f)));
  color_stop_list.push_back(ColorStop(0.5f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f)));
  color_stop_list.push_back(ColorStop(1.0f, ColorRGBA(0.0f, 0.0f, 1.0f, 1.0f)));

  CompositionNode::Builder builder;
  builder.AddChild(new RectNode(RectF(output_surface_size()),
                                scoped_ptr<Brush>(new SolidColorBrush(
                                    ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f)))));

  builder.AddChild(new RectNode(RectF(output_surface_size()),
                                scoped_ptr<Brush>(new RadialGradientBrush(
                                    PointF(75, 100), 75, color_stop_list))));
  TestTree(new CompositionNode(builder.Pass()));
}

TEST_F(PixelTest, HorizontalEllipseGradient5Stops) {
  TestTree(new RectNode(
      ScaledCenteredSurface(output_surface_size(), 0.9f),
      scoped_ptr<Brush>(new RadialGradientBrush(
          PointF(75, 100), 100, 75, Make5ColorEvenlySpacedStopList()))));
}

TEST_F(PixelTest, DropShadowText) {
  std::vector<Shadow> shadows;
  shadows.push_back(Shadow(Vector2dF(6.0f, 6.0f), 0, ColorRGBA(0, 0, 0, 1.0f)));
  TestTree(CreateTextNodeWithinSurface(
      GetResourceProvider(), "Cobalt",
      FontStyle(FontStyle::kBoldWeight, FontStyle::kUprightSlant), 60,
      ColorRGBA(0.4f, 0, 1.0f, 1.0f), shadows));
}

TEST_F(PixelTest, DropShadowBlurred0Point1PxText) {
  std::vector<Shadow> shadows;
  shadows.push_back(
      Shadow(Vector2dF(6.0f, 6.0f), 0.1f, ColorRGBA(0, 0, 0, 1.0f)));
  TestTree(CreateTextNodeWithinSurface(
      GetResourceProvider(), "Cobalt",
      FontStyle(FontStyle::kBoldWeight, FontStyle::kUprightSlant), 60,
      ColorRGBA(0.4f, 0, 1.0f, 1.0f), shadows));
}

TEST_F(PixelTest, DropShadowBlurred1PxText) {
  std::vector<Shadow> shadows;
  shadows.push_back(
      Shadow(Vector2dF(6.0f, 6.0f), 1.0f, ColorRGBA(0, 0, 0, 1.0f)));
  TestTree(CreateTextNodeWithinSurface(
      GetResourceProvider(), "Cobalt",
      FontStyle(FontStyle::kBoldWeight, FontStyle::kUprightSlant), 60,
      ColorRGBA(0.4f, 0, 1.0f, 1.0f), shadows));
}

TEST_F(PixelTest, DropShadowBlurred2PxText) {
  std::vector<Shadow> shadows;
  shadows.push_back(
      Shadow(Vector2dF(6.0f, 6.0f), 2.0f, ColorRGBA(0, 0, 0, 1.0f)));
  TestTree(CreateTextNodeWithinSurface(
      GetResourceProvider(), "Cobalt",
      FontStyle(FontStyle::kBoldWeight, FontStyle::kUprightSlant), 60,
      ColorRGBA(0.4f, 0, 1.0f, 1.0f), shadows));
}

TEST_F(PixelTest, DropShadowBlurred3PxText) {
  std::vector<Shadow> shadows;
  shadows.push_back(
      Shadow(Vector2dF(6.0f, 6.0f), 3.0f, ColorRGBA(0, 0, 0, 1.0f)));
  TestTree(CreateTextNodeWithinSurface(
      GetResourceProvider(), "Cobalt",
      FontStyle(FontStyle::kBoldWeight, FontStyle::kUprightSlant), 60,
      ColorRGBA(0.4f, 0, 1.0f, 1.0f), shadows));
}

TEST_F(PixelTest, DropShadowBlurred4PxText) {
  std::vector<Shadow> shadows;
  shadows.push_back(
      Shadow(Vector2dF(6.0f, 6.0f), 4.0f, ColorRGBA(0, 0, 0, 1.0f)));
  TestTree(CreateTextNodeWithinSurface(
      GetResourceProvider(), "Cobalt",
      FontStyle(FontStyle::kBoldWeight, FontStyle::kUprightSlant), 60,
      ColorRGBA(0.4f, 0, 1.0f, 1.0f), shadows));
}

TEST_F(PixelTest, DropShadowBlurred5PxText) {
  std::vector<Shadow> shadows;
  shadows.push_back(
      Shadow(Vector2dF(6.0f, 6.0f), 5.0f, ColorRGBA(0, 0, 0, 1.0f)));
  TestTree(CreateTextNodeWithinSurface(
      GetResourceProvider(), "Cobalt",
      FontStyle(FontStyle::kBoldWeight, FontStyle::kUprightSlant), 60,
      ColorRGBA(0.4f, 0, 1.0f, 1.0f), shadows));
}

TEST_F(PixelTest, DropShadowBlurred6PxText) {
  std::vector<Shadow> shadows;
  shadows.push_back(
      Shadow(Vector2dF(6.0f, 6.0f), 6.0f, ColorRGBA(0, 0, 0, 1.0f)));
  TestTree(CreateTextNodeWithinSurface(
      GetResourceProvider(), "Cobalt",
      FontStyle(FontStyle::kBoldWeight, FontStyle::kUprightSlant), 60,
      ColorRGBA(0.4f, 0, 1.0f, 1.0f), shadows));
}

TEST_F(PixelTest, DropShadowBlurred8PxText) {
  std::vector<Shadow> shadows;
  shadows.push_back(
      Shadow(Vector2dF(6.0f, 6.0f), 8.0f, ColorRGBA(0, 0, 0, 1.0f)));
  TestTree(CreateTextNodeWithinSurface(
      GetResourceProvider(), "Cobalt",
      FontStyle(FontStyle::kBoldWeight, FontStyle::kUprightSlant), 60,
      ColorRGBA(0.4f, 0, 1.0f, 1.0f), shadows));
}

TEST_F(PixelTest, DropShadowBlurred20PxText) {
  std::vector<Shadow> shadows;
  shadows.push_back(
      Shadow(Vector2dF(6.0f, 6.0f), 20.0f, ColorRGBA(0, 0, 0, 1.0f)));
  TestTree(CreateTextNodeWithinSurface(
      GetResourceProvider(), "Cobalt",
      FontStyle(FontStyle::kBoldWeight, FontStyle::kUprightSlant), 60,
      ColorRGBA(0.4f, 0, 1.0f, 1.0f), shadows));
}

TEST_F(PixelTest, ColoredDropShadowText) {
  std::vector<Shadow> shadows;
  shadows.push_back(
      Shadow(Vector2dF(6.0f, 6.0f), 0, ColorRGBA(1.0f, 0, 0, 1.0f)));
  TestTree(CreateTextNodeWithinSurface(
      GetResourceProvider(), "Cobalt",
      FontStyle(FontStyle::kBoldWeight, FontStyle::kUprightSlant), 60,
      ColorRGBA(0.4f, 0, 1.0f, 1.0f), shadows));
}

TEST_F(PixelTest, ColoredDropShadowBlurredText) {
  std::vector<Shadow> shadows;
  shadows.push_back(
      Shadow(Vector2dF(6.0f, 6.0f), 6.0f, ColorRGBA(1.0f, 0, 0, 1.0f)));
  TestTree(CreateTextNodeWithinSurface(
      GetResourceProvider(), "Cobalt",
      FontStyle(FontStyle::kBoldWeight, FontStyle::kUprightSlant), 60,
      ColorRGBA(0.4f, 0, 1.0f, 1.0f), shadows));
}

TEST_F(PixelTest, MultipleColoredDropShadowText) {
  std::vector<Shadow> shadows;
  shadows.push_back(
      Shadow(Vector2dF(6.0f, 6.0f), 0.0f, ColorRGBA(1.0f, 0, 0, 1.0f)));
  shadows.push_back(
      Shadow(Vector2dF(12.0f, 12.0f), 0.0f, ColorRGBA(0, 1.0f, 0, 1.0f)));
  shadows.push_back(
      Shadow(Vector2dF(18.0f, 18.0f), 0.0f, ColorRGBA(0, 0, 1.0f, 1.0f)));
  TestTree(CreateTextNodeWithinSurface(
      GetResourceProvider(), "Cobalt",
      FontStyle(FontStyle::kBoldWeight, FontStyle::kUprightSlant), 60,
      ColorRGBA(0.4f, 0, 1.0f, 1.0f), shadows));
}

TEST_F(PixelTest, MultipleColoredDropShadowBlurredText) {
  std::vector<Shadow> shadows;
  shadows.push_back(
      Shadow(Vector2dF(6.0f, 6.0f), 6.0f, ColorRGBA(1.0f, 0, 0, 1.0f)));
  shadows.push_back(
      Shadow(Vector2dF(12.0f, 12.0f), 6.0f, ColorRGBA(0, 1.0f, 0, 1.0f)));
  shadows.push_back(
      Shadow(Vector2dF(18.0f, 18.0f), 6.0f, ColorRGBA(0, 0, 1.0f, 1.0f)));
  TestTree(CreateTextNodeWithinSurface(
      GetResourceProvider(), "Cobalt",
      FontStyle(FontStyle::kBoldWeight, FontStyle::kUprightSlant), 60,
      ColorRGBA(0.4f, 0, 1.0f, 1.0f), shadows));
}

namespace {
scoped_refptr<Node> CreateShadowRectWithBackground(
    const SizeF& background_size, const ColorRGBA& background_color,
    const ColorRGBA& shadow_rect_color, const RectF& shadow_rect,
    const Shadow& shadow, bool inset, float spread,
    const RoundedCorners& shadow_rounded_corners) {
  CompositionNode::Builder builder;
  builder.AddChild(
      new RectNode(RectF(background_size),
                   scoped_ptr<Brush>(new SolidColorBrush(background_color))));
  builder.AddChild(new RectNode(
      shadow_rect, scoped_ptr<Brush>(new SolidColorBrush(shadow_rect_color)),
      scoped_ptr<Border>(),
      make_scoped_ptr(new RoundedCorners(shadow_rounded_corners))));
  RectShadowNode::Builder rect_shadow_node_builder(shadow_rect, shadow, inset,
                                                   spread);
  if (!shadow_rounded_corners.AreSquares()) {
    rect_shadow_node_builder.rounded_corners = shadow_rounded_corners;
  }
  builder.AddChild(new RectShadowNode(rect_shadow_node_builder));

  return new CompositionNode(builder.Pass());
}

scoped_refptr<Node> CreateShadowRectWithBackground(
    const SizeF& background_size, const ColorRGBA& background_color,
    const ColorRGBA& shadow_rect_color, const RectF& shadow_rect,
    const Shadow& shadow, bool inset, float spread) {
  return CreateShadowRectWithBackground(background_size, background_color,
                                        shadow_rect_color, shadow_rect, shadow,
                                        inset, spread, RoundedCorners(0, 0));
}

scoped_refptr<Node> CreateShadowRectWithBackground(
    const SizeF& background_size, const ColorRGBA& background_color,
    const ColorRGBA& shadow_rect_color, const RectF& shadow_rect,
    const Shadow& shadow) {
  return CreateShadowRectWithBackground(background_size, background_color,
                                        shadow_rect_color, shadow_rect, shadow,
                                        false, 0.0f, RoundedCorners(0, 0));
}

scoped_refptr<Node> CreateRoundedShadowRectWithBackground(
    const SizeF& background_size, const ColorRGBA& background_color,
    const ColorRGBA& shadow_rect_color, const RectF& shadow_rect,
    const Shadow& shadow, bool inset, float spread,
    const RoundedCorners& shadow_rounded_corners) {
  return CreateShadowRectWithBackground(background_size, background_color,
                                        shadow_rect_color, shadow_rect, shadow,
                                        inset, spread, shadow_rounded_corners);
}
}  // namespace

TEST_F(PixelTest, GreyBoxShadowBottomRight) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(50, 50, 100, 100),
      Shadow(Vector2dF(8.0f, 8.0f), 0.0f, ColorRGBA(0, 0, 0, 1))));
}

TEST_F(PixelTest, TransparentBoxShadowOnGreenBackgroundBottomRight) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(0.3f, 0.8f, 0.3f, 1.0f),
      ColorRGBA(0, 0, 0, 0), RectF(50, 50, 100, 100),
      Shadow(Vector2dF(8.0f, 8.0f), 0.0f, ColorRGBA(0, 0, 0, 1))));
}

TEST_F(PixelTest, GreyBoxShadowBottomLeft) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(50, 50, 100, 100),
      Shadow(Vector2dF(-8.0f, 8.0f), 0.0f, ColorRGBA(0, 0, 0, 1))));
}

TEST_F(PixelTest, GreyBoxShadowTopLeft) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(50, 50, 100, 100),
      Shadow(Vector2dF(-8.0f, -8.0f), 0.0f, ColorRGBA(0, 0, 0, 1))));
}

TEST_F(PixelTest, GreyBoxShadowTopRight) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(50, 50, 100, 100),
      Shadow(Vector2dF(8.0f, -8.0f), 0.0f, ColorRGBA(0, 0, 0, 1))));
}

TEST_F(PixelTest, BoxShadowBlur1PxCentered) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(50, 50, 100, 100),
      Shadow(Vector2dF(0.0f, 0.0f), 1.0f, ColorRGBA(0, 0, 0, 1))));
}

TEST_F(PixelTest, BoxShadowBlur2PxCentered) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(50, 50, 100, 100),
      Shadow(Vector2dF(0.0f, 0.0f), 2.0f, ColorRGBA(0, 0, 0, 1))));
}

TEST_F(PixelTest, BoxShadowBlur3PxCentered) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(50, 50, 100, 100),
      Shadow(Vector2dF(0.0f, 0.0f), 3.0f, ColorRGBA(0, 0, 0, 1))));
}

TEST_F(PixelTest, BoxShadowBlur4PxCentered) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(50, 50, 100, 100),
      Shadow(Vector2dF(0.0f, 0.0f), 4.0f, ColorRGBA(0, 0, 0, 1))));
}

TEST_F(PixelTest, BoxShadowBlur5PxCentered) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(50, 50, 100, 100),
      Shadow(Vector2dF(0.0f, 0.0f), 5.0f, ColorRGBA(0, 0, 0, 1))));
}

TEST_F(PixelTest, BoxShadowBlur6PxCentered) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(50, 50, 100, 100),
      Shadow(Vector2dF(0.0f, 0.0f), 6.0f, ColorRGBA(0, 0, 0, 1))));
}

TEST_F(PixelTest, BoxShadowBlur8PxCentered) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(50, 50, 100, 100),
      Shadow(Vector2dF(0.0f, 0.0f), 8.0f, ColorRGBA(0, 0, 0, 1))));
}

TEST_F(PixelTest, BoxShadowBlur100pxCentered) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(50, 50, 100, 100),
      Shadow(Vector2dF(0.0f, 0.0f), 100.0f, ColorRGBA(0, 0, 0, 1))));
}

TEST_F(PixelTest, ScaledBoxShadowWithSpreadAndBlurCentered) {
  TestTree(new MatrixTransformNode(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(50, 8, 100, 4),
      Shadow(Vector2dF(0.0f, 0.0f), 3.0f, ColorRGBA(0, 0, 0, 1)), false, 5.0f),
      ScaleMatrix(1.0f, 10.0f)));
}

TEST_F(PixelTest, TransparentBoxShadowBlurOnGreenBackgroundCentered) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(0.3f, 0.8f, 0.3f, 1.0f),
      ColorRGBA(0, 0, 0, 0), RectF(50, 50, 100, 100),
      Shadow(Vector2dF(0.0f, 0.0f), 8.0f, ColorRGBA(0, 0, 0, 1))));
}

TEST_F(PixelTest, BoxShadowBlurBottomRight) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(50, 50, 100, 100),
      Shadow(Vector2dF(8.0f, 8.0f), 8.0f, ColorRGBA(0, 0, 0, 1))));
}

TEST_F(PixelTest, BoxShadowBlurBottomLeft) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(50, 50, 100, 100),
      Shadow(Vector2dF(-8.0f, 8.0f), 8.0f, ColorRGBA(0, 0, 0, 1))));
}

TEST_F(PixelTest, BoxShadowBlurTopLeft) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(50, 50, 100, 100),
      Shadow(Vector2dF(-8.0f, -8.0f), 8.0f, ColorRGBA(0, 0, 0, 1))));
}

TEST_F(PixelTest, BoxShadowBlurTopRight) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(50, 50, 100, 100),
      Shadow(Vector2dF(8.0f, -8.0f), 8.0f, ColorRGBA(0, 0, 0, 1))));
}

TEST_F(PixelTest, BoxShadowBlurCenteredOffscreenTopLeft) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(-50, -50, 100, 100),
      Shadow(Vector2dF(0.0f, 0.0f), 8.0f, ColorRGBA(0, 0, 0, 1))));
}

TEST_F(PixelTest, BoxShadowBlurCenteredOffscreenBottomRight) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(150, 150, 100, 100),
      Shadow(Vector2dF(0.0f, 0.0f), 8.0f, ColorRGBA(0, 0, 0, 1))));
}

TEST_F(PixelTest, BoxShadowWithSpread) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(50, 50, 100, 100),
      Shadow(Vector2dF(8.0f, 8.0f), 0.0f, ColorRGBA(0, 0, 0, 1)), false,
      20.0f));
}

TEST_F(PixelTest, BoxShadowWithInset) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(50, 50, 100, 100),
      Shadow(Vector2dF(8.0f, 8.0f), 0.0f, ColorRGBA(0, 0, 0, 1)), true, 0.0f));
}

TEST_F(PixelTest, BoxShadowWithInset25pxSpreadAndBlur) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(50, 50, 100, 100),
      Shadow(Vector2dF(8.0f, 8.0f), 8.0f, ColorRGBA(0, 0, 0, 1)), true, 25.0f));
}

TEST_F(PixelTest, BoxShadowCircleWithInset25pxSpreadAndRoundedCorners) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(50, 50, 50, 50),
      Shadow(Vector2dF(8.0f, 8.0f), 0.0f, ColorRGBA(0, 0, 0, 1)), true, 5.0f,
      RoundedCorners(25, 25)));
}

TEST_F(PixelTest, BoxShadowCircleWithOutset25pxSpreadAndRoundedCorners) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(50, 50, 50, 50),
      Shadow(Vector2dF(8.0f, 8.0f), 0.0f, ColorRGBA(0, 0, 0, 1)), false, 5.0f,
      RoundedCorners(25, 25)));
}

TEST_F(PixelTest, ScaledBoxShadowEllipseWithOutset5pxSpreadAndRoundedCorners) {
  TestTree(new MatrixTransformNode(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(6, 25, 2, 100),
      Shadow(Vector2dF(0.0f, 0.0f), 0.0f, ColorRGBA(0, 0, 0, 1)), false, 5.0f,
      RoundedCorners(1, 50)),
      ScaleMatrix(15.0f, 1.0f)));
}

TEST_F(PixelTest, BoxShadowCircleWithInset25pxSpread1pxBlurAndRoundedCorners) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(50, 50, 50, 50),
      Shadow(Vector2dF(8.0f, 8.0f), 1.0f, ColorRGBA(0, 0, 0, 1)), true, 5.0f,
      RoundedCorners(25, 25)));
}

TEST_F(PixelTest,
       BoxShadowCircleWithInset25pxSpread1pxBlurRoundedCornersAndNoOffset) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(50, 50, 50, 50),
      Shadow(Vector2dF(0.0f, 0.0f), 1.0f, ColorRGBA(0, 0, 0, 1)), true, 5.0f,
      RoundedCorners(25, 25)));
}

TEST_F(PixelTest, BoxShadowCircleWithOutset25pxSpread1pxBlurAndRoundedCorners) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(50, 50, 50, 50),
      Shadow(Vector2dF(8.0f, 8.0f), 1.0f, ColorRGBA(0, 0, 0, 1)), false, 5.0f,
      RoundedCorners(25, 25)));
}

TEST_F(PixelTest, BoxShadowCircleWithInset25pxSpread8pxBlurAndRoundedCorners) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(50, 50, 50, 50),
      Shadow(Vector2dF(8.0f, 8.0f), 8.0f, ColorRGBA(0, 0, 0, 1)), true, 5.0f,
      RoundedCorners(25, 25)));
}

TEST_F(PixelTest, BoxShadowCircleWithOutset25pxSpread8pxBlurAndRoundedCorners) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(50, 50, 50, 50),
      Shadow(Vector2dF(8.0f, 8.0f), 8.0f, ColorRGBA(0, 0, 0, 1)), false, 5.0f,
      RoundedCorners(25, 25)));
}

TEST_F(PixelTest, BoxShadowCircleWithInset25pxSpread50pxBlurAndRoundedCorners) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(50, 50, 50, 50),
      Shadow(Vector2dF(8.0f, 8.0f), 50.0f, ColorRGBA(0, 0, 0, 1)), true, 5.0f,
      RoundedCorners(25, 25)));
}

TEST_F(PixelTest,
       BoxShadowBigCircleWithInset25pxSpread50pxBlurAndRoundedCorners) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(25, 25, 150, 150),
      Shadow(Vector2dF(8.0f, 8.0f), 50.0f, ColorRGBA(0, 0, 0, 1)), true, 5.0f,
      RoundedCorners(75, 75)));
}

TEST_F(PixelTest,
       BoxShadowCircleWithOutset25pxSpread50pxBlurAndRoundedCorners) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(50, 50, 50, 50),
      Shadow(Vector2dF(8.0f, 8.0f), 50.0f, ColorRGBA(0, 0, 0, 1)), false, 5.0f,
      RoundedCorners(25, 25)));
}

TEST_F(PixelTest, BoxShadowEllipseWithInset25pxSpreadAndRoundedCorners) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(50, 50, 140, 100),
      Shadow(Vector2dF(8.0f, 8.0f), 0.0f, ColorRGBA(0, 0, 0, 1)), true, 25.0f,
      RoundedCorners(70, 50)));
}

TEST_F(PixelTest, BoxShadowEllipseWithOutset25pxSpreadAndRoundedCorners) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(50, 50, 140, 100),
      Shadow(Vector2dF(8.0f, 8.0f), 0.0f, ColorRGBA(0, 0, 0, 1)), false, 25.0f,
      RoundedCorners(70, 50)));
}

TEST_F(PixelTest, BoxShadowEllipseWithInset25pxSpread1pxBlurAndRoundedCorners) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(50, 50, 140, 100),
      Shadow(Vector2dF(8.0f, 8.0f), 1.0f, ColorRGBA(0, 0, 0, 1)), true, 25.0f,
      RoundedCorners(70, 50)));
}

TEST_F(PixelTest,
       BoxShadowEllipseWithOutset25pxSpread1pxBlurAndRoundedCorners) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(50, 50, 140, 100),
      Shadow(Vector2dF(8.0f, 8.0f), 1.0f, ColorRGBA(0, 0, 0, 1)), false, 25.0f,
      RoundedCorners(70, 50)));
}

TEST_F(PixelTest, BoxShadowEllipseWithInset25pxSpread8pxBlurAndRoundedCorners) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(50, 50, 140, 100),
      Shadow(Vector2dF(8.0f, 8.0f), 8.0f, ColorRGBA(0, 0, 0, 1)), true, 25.0f,
      RoundedCorners(70, 50)));
}

TEST_F(PixelTest,
       BoxShadowEllipseWithOutset25pxSpread8pxBlurAndRoundedCorners) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(50, 50, 140, 100),
      Shadow(Vector2dF(8.0f, 8.0f), 8.0f, ColorRGBA(0, 0, 0, 1)), false, 25.0f,
      RoundedCorners(70, 50)));
}

TEST_F(PixelTest,
       ScaledBoxShadowEllipseWithOutset25pxSpread3pxBlurAndRoundedCorners) {
  TestTree(new MatrixTransformNode(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(20, 5, 140, 10),
      Shadow(Vector2dF(8.0f, 1.0f), 3.0f, ColorRGBA(0, 0, 0, 1)), false, 4.0f,
      RoundedCorners(70, 5)),
      ScaleMatrix(1.0f, 10.0f)));
}

TEST_F(PixelTest,
       BoxShadowEllipseWithInset25pxSpread50pxBlurAndRoundedCorners) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(50, 50, 140, 100),
      Shadow(Vector2dF(8.0f, 8.0f), 50.0f, ColorRGBA(0, 0, 0, 1)), true, 25.0f,
      RoundedCorners(70, 50)));
}

TEST_F(PixelTest,
       BoxShadowBigEllipseWithInset25pxSpread50pxBlurAndRoundedCorners) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(25, 25, 190, 150),
      Shadow(Vector2dF(8.0f, 8.0f), 50.0f, ColorRGBA(0, 0, 0, 1)), true, 25.0f,
      RoundedCorners(95, 75)));
}

TEST_F(PixelTest,
       BoxShadowEllipseWithOutset25pxSpread50pxBlurAndRoundedCorners) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(50, 50, 140, 100),
      Shadow(Vector2dF(8.0f, 8.0f), 50.0f, ColorRGBA(0, 0, 0, 1)), false, 25.0f,
      RoundedCorners(70, 50)));
}

TEST_F(PixelTest, BoxShadowWithInset5pxSpreadAndSameRoundedCorners) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(25, 50, 150, 100),
      Shadow(Vector2dF(10.0f, 10.0f), 0.0f, ColorRGBA(0, 0, 0, 1)), true, 5.0f,
      RoundedCorners(20, 20)));
}

TEST_F(PixelTest, BoxShadowWithOutset5pxSpreadAndSameRoundedCorners) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(25, 50, 150, 100),
      Shadow(Vector2dF(10.0f, 10.0f), 0.0f, ColorRGBA(0, 0, 0, 1)), false, 5.0f,
      RoundedCorners(20, 20)));
}

TEST_F(PixelTest, BoxShadowWithInset25pxSpreadAndSameRoundedCorners) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(25, 50, 150, 100),
      Shadow(Vector2dF(10.0f, 10.0f), 0.0f, ColorRGBA(0, 0, 0, 1)), true, 25.0f,
      RoundedCorners(20, 20)));
}

TEST_F(PixelTest, BoxShadowWithOutset25pxSpreadAndSameRoundedCorners) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(25, 50, 150, 100),
      Shadow(Vector2dF(10.0f, 10.0f), 0.0f, ColorRGBA(0, 0, 0, 1)), false,
      25.0f, RoundedCorners(20, 20)));
}

TEST_F(PixelTest, BoxShadowWithInset5pxSpread5pxBlurAndSameRoundedCorners) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(25, 50, 150, 100),
      Shadow(Vector2dF(0.0f, 0.0f), 5.0f, ColorRGBA(0, 0, 0, 1)), false, 5.0f,
      RoundedCorners(10, 10)));
}

TEST_F(PixelTest, BoxShadowWithOutset5pxSpread5pxBlurAndSameRoundedCorners) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(25, 50, 150, 100),
      Shadow(Vector2dF(0.0f, 0.0f), 5.0f, ColorRGBA(0, 0, 0, 1)), false, 5.0f,
      RoundedCorners(10, 10)));
}

TEST_F(PixelTest, BoxShadowWithInset5pxSpreadAndDifferentRoundedCorners) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(25, 50, 150, 100),
      Shadow(Vector2dF(10.0f, 10.0f), 0.0f, ColorRGBA(0, 0, 0, 1)), true, 5.0f,
      RoundedCorners(20, 10)));
}

TEST_F(PixelTest, BoxShadowWithOutset5pxSpreadAndDifferentRoundedCorners) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(25, 50, 150, 100),
      Shadow(Vector2dF(10.0f, 10.0f), 0.0f, ColorRGBA(0, 0, 0, 1)), false, 5.0f,
      RoundedCorners(20, 10)));
}

TEST_F(PixelTest, BoxShadowWithInset5pxSpread25pxBlurAndSameRoundedCorners) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(25, 50, 150, 100),
      Shadow(Vector2dF(10.0f, 10.0f), 25.0f, ColorRGBA(0, 0, 0, 1)), true, 5.0f,
      RoundedCorners(20, 20)));
}

TEST_F(PixelTest, BoxShadowWithOutset5pxSpread25pxBlurAndSameRoundedCorners) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(25, 50, 150, 100),
      Shadow(Vector2dF(10.0f, 10.0f), 25.0f, ColorRGBA(0, 0, 0, 1)), false,
      5.0f, RoundedCorners(20, 20)));
}

TEST_F(PixelTest,
       BoxShadowWithInset5pxSpread25pxBlurAndDifferentRoundedCorners) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(25, 50, 150, 100),
      Shadow(Vector2dF(10.0f, 10.0f), 25.0f, ColorRGBA(0, 0, 0, 1)), true, 5.0f,
      RoundedCorners(20, 10)));
}

TEST_F(PixelTest,
       BoxShadowWithOutset5pxSpread25pxBlurAndDifferentRoundedCorners) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(25, 50, 150, 100),
      Shadow(Vector2dF(10.0f, 10.0f), 25.0f, ColorRGBA(0, 0, 0, 1)), false,
      5.0f, RoundedCorners(20, 10)));
}

TEST_F(PixelTest, BoxShadowWithInset25pxSpreadAndRoundedCorners) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(25, 50, 150, 100),
      Shadow(Vector2dF(8.0f, 8.0f), 0.0f, ColorRGBA(0, 0, 0, 1)), true, 25.0f,
      RoundedCorners(5, 10)));
}

TEST_F(PixelTest, BoxShadowWithOutset25pxSpreadAndRoundedCorners) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(25, 50, 150, 100),
      Shadow(Vector2dF(8.0f, 8.0f), 0.0f, ColorRGBA(0, 0, 0, 1)), false, 25.0f,
      RoundedCorners(5, 10)));
}

TEST_F(PixelTest, BoxShadowWithInset25pxSpread1pxBlurAndRoundedCorners) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(25, 50, 150, 100),
      Shadow(Vector2dF(8.0f, 8.0f), 1.0f, ColorRGBA(0, 0, 0, 1)), true, 25.0f,
      RoundedCorners(5, 10)));
}

TEST_F(PixelTest, BoxShadowWithOutset25pxSpread1pxBlurAndRoundedCorners) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(25, 50, 150, 100),
      Shadow(Vector2dF(8.0f, 8.0f), 1.0f, ColorRGBA(0, 0, 0, 1)), false, 25.0f,
      RoundedCorners(5, 10)));
}

TEST_F(PixelTest, BoxShadowWithInset25pxSpread8pxBlurAndRoundedCorners) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(25, 50, 150, 100),
      Shadow(Vector2dF(8.0f, 8.0f), 8.0f, ColorRGBA(0, 0, 0, 1)), true, 25.0f,
      RoundedCorners(5, 10)));
}

TEST_F(PixelTest, BoxShadowWithOutset25pxSpread8pxBlurAndRoundedCorners) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(25, 50, 150, 100),
      Shadow(Vector2dF(8.0f, 8.0f), 8.0f, ColorRGBA(0, 0, 0, 1)), false, 25.0f,
      RoundedCorners(5, 10)));
}

TEST_F(PixelTest, BoxShadowWithInset25pxSpread50pxBlurAndRoundedCorners) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(25, 50, 150, 100),
      Shadow(Vector2dF(8.0f, 8.0f), 50.0f, ColorRGBA(0, 0, 0, 1)), true, 25.0f,
      RoundedCorners(5, 10)));
}

TEST_F(PixelTest, BoxShadowWithOutset25pxSpread50pxBlurAndRoundedCorners) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(25, 50, 150, 100),
      Shadow(Vector2dF(8.0f, 8.0f), 50.0f, ColorRGBA(0, 0, 0, 1)), false, 25.0f,
      RoundedCorners(5, 10)));
}

TEST_F(PixelTest, BoxShadowWithInset25pxSpreadAndIsometricRoundedCorners) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(50, 50, 100, 100),
      Shadow(Vector2dF(8.0f, 8.0f), 0.0f, ColorRGBA(0, 0, 0, 1)), true, 25.0f,
      RoundedCorners(10, 10)));
}

TEST_F(PixelTest, BoxShadowWithOutset25pxSpreadAndIsometricRoundedCorners) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(50, 50, 100, 100),
      Shadow(Vector2dF(8.0f, 8.0f), 0.0f, ColorRGBA(0, 0, 0, 1)), false, 25.0f,
      RoundedCorners(10, 10)));
}

TEST_F(PixelTest,
       BoxShadowWithInset25pxSpread1pxBlurAndIsometricRoundedCorners) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(50, 50, 100, 100),
      Shadow(Vector2dF(8.0f, 8.0f), 1.0f, ColorRGBA(0, 0, 0, 1)), true, 25.0f,
      RoundedCorners(10, 10)));
}

TEST_F(PixelTest,
       BoxShadowWithOutset25pxSpread1pxBlurAndIsometricRoundedCorners) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(50, 50, 100, 100),
      Shadow(Vector2dF(8.0f, 8.0f), 1.0f, ColorRGBA(0, 0, 0, 1)), false, 25.0f,
      RoundedCorners(10, 10)));
}

TEST_F(PixelTest,
       BoxShadowWithInset25pxSpread8pxBlurAndIsometricRoundedCorners) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(50, 50, 100, 100),
      Shadow(Vector2dF(8.0f, 8.0f), 8.0f, ColorRGBA(0, 0, 0, 1)), true, 25.0f,
      RoundedCorners(10, 10)));
}

TEST_F(PixelTest,
       BoxShadowWithOutset25pxSpread8pxBlurAndIsometricRoundedCorners) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(50, 50, 100, 100),
      Shadow(Vector2dF(8.0f, 8.0f), 8.0f, ColorRGBA(0, 0, 0, 1)), false, 25.0f,
      RoundedCorners(10, 10)));
}

TEST_F(PixelTest,
       BoxShadowWithInset25pxSpread50pxBlurAndIsometricRoundedCorners) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(50, 50, 100, 100),
      Shadow(Vector2dF(8.0f, 8.0f), 50.0f, ColorRGBA(0, 0, 0, 1)), true, 25.0f,
      RoundedCorners(10, 10)));
}

TEST_F(PixelTest,
       BoxShadowWithOutset25pxSpread50pxBlurAndIsometricRoundedCorners) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(50, 50, 100, 100),
      Shadow(Vector2dF(8.0f, 8.0f), 50.0f, ColorRGBA(0, 0, 0, 1)), false, 25.0f,
      RoundedCorners(10, 10)));
}

TEST_F(PixelTest, BoxShadowWithInsetNeg10pxSpreadAnd10pxBlurAndRoundedCorners) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(50, 50, 100, 100),
      Shadow(Vector2dF(-15.0f, 20.0f), 10.0f, ColorRGBA(0, 0, 0, 1)), true,
      -10.0f, RoundedCorners(10, 10)));
}

TEST_F(PixelTest,
       BoxShadowWithOutsetNeg10pxSpreadAnd10pxBlurAndRoundedCorners) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(50, 50, 100, 100),
      Shadow(Vector2dF(-15.0f, 20.0f), 10.0f, ColorRGBA(0, 0, 0, 1)), false,
      -10.0f, RoundedCorners(10, 10)));
}

TEST_F(PixelTest, BoxShadowWithInsetNeg10pxSpreadAnd2pxBlurAndRoundedCorners) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(50, 50, 100, 100),
      Shadow(Vector2dF(-15.0f, 20.0f), 2.0f, ColorRGBA(0, 0, 0, 1)), true,
      -10.0f, RoundedCorners(10, 10)));
}

TEST_F(PixelTest, BoxShadowWithOutsetNeg10pxSpreadAnd2pxBlurAndRoundedCorners) {
  TestTree(CreateShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(50, 50, 100, 100),
      Shadow(Vector2dF(-15.0f, 20.0f), 2.0f, ColorRGBA(0, 0, 0, 1)), false,
      -10.0f, RoundedCorners(10, 10)));
}

TEST_F(PixelTest, FilterBlurred0Point1PxText) {
  TestTree(new FilterNode(
      BlurFilter(0.1f),
      CreateTextNodeWithinSurface(
          GetResourceProvider(), "Cobalt",
          FontStyle(FontStyle::kBoldWeight, FontStyle::kUprightSlant), 60,
          ColorRGBA(0.4f, 0, 1.0f, 1.0f))));
}

TEST_F(PixelTest, FilterBlurred1PxText) {
  TestTree(new FilterNode(
      BlurFilter(1.0f),
      CreateTextNodeWithinSurface(
          GetResourceProvider(), "Cobalt",
          FontStyle(FontStyle::kBoldWeight, FontStyle::kUprightSlant), 60,
          ColorRGBA(0.4f, 0, 1.0f, 1.0f))));
}

TEST_F(PixelTest, FilterBlurred2PxText) {
  TestTree(new FilterNode(
      BlurFilter(2.0f),
      CreateTextNodeWithinSurface(
          GetResourceProvider(), "Cobalt",
          FontStyle(FontStyle::kBoldWeight, FontStyle::kUprightSlant), 60,
          ColorRGBA(0.4f, 0, 1.0f, 1.0f))));
}

TEST_F(PixelTest, FilterBlurred3PxText) {
  TestTree(new FilterNode(
      BlurFilter(3.0f),
      CreateTextNodeWithinSurface(
          GetResourceProvider(), "Cobalt",
          FontStyle(FontStyle::kBoldWeight, FontStyle::kUprightSlant), 60,
          ColorRGBA(0.4f, 0, 1.0f, 1.0f))));
}

TEST_F(PixelTest, FilterBlurred4PxText) {
  TestTree(new FilterNode(
      BlurFilter(4.0f),
      CreateTextNodeWithinSurface(
          GetResourceProvider(), "Cobalt",
          FontStyle(FontStyle::kBoldWeight, FontStyle::kUprightSlant), 60,
          ColorRGBA(0.4f, 0, 1.0f, 1.0f))));
}

TEST_F(PixelTest, FilterBlurred5PxText) {
  TestTree(new FilterNode(
      BlurFilter(5.0f),
      CreateTextNodeWithinSurface(
          GetResourceProvider(), "Cobalt",
          FontStyle(FontStyle::kBoldWeight, FontStyle::kUprightSlant), 60,
          ColorRGBA(0.4f, 0, 1.0f, 1.0f))));
}

TEST_F(PixelTest, FilterBlurred6PxText) {
  TestTree(new FilterNode(
      BlurFilter(6.0f),
      CreateTextNodeWithinSurface(
          GetResourceProvider(), "Cobalt",
          FontStyle(FontStyle::kBoldWeight, FontStyle::kUprightSlant), 60,
          ColorRGBA(0.4f, 0, 1.0f, 1.0f))));
}

TEST_F(PixelTest, FilterBlurred8PxText) {
  TestTree(new FilterNode(
      BlurFilter(8.0f),
      CreateTextNodeWithinSurface(
          GetResourceProvider(), "Cobalt",
          FontStyle(FontStyle::kBoldWeight, FontStyle::kUprightSlant), 60,
          ColorRGBA(0.4f, 0, 1.0f, 1.0f))));
}

TEST_F(PixelTest, FilterBlurred20PxText) {
  TestTree(new FilterNode(
      BlurFilter(20.0f),
      CreateTextNodeWithinSurface(
          GetResourceProvider(), "Cobalt",
          FontStyle(FontStyle::kBoldWeight, FontStyle::kUprightSlant), 60,
          ColorRGBA(0.4f, 0, 1.0f, 1.0f))));
}

TEST_F(PixelTest, FilterBlurred100PxText) {
  TestTree(new FilterNode(
      BlurFilter(100.0f),
      CreateTextNodeWithinSurface(
          GetResourceProvider(), "Cobalt",
          FontStyle(FontStyle::kBoldWeight, FontStyle::kUprightSlant), 60,
          ColorRGBA(0.4f, 0, 1.0f, 1.0f))));
}

TEST_F(PixelTest, BoxShadowCircleBottomRight) {
  TestTree(CreateRoundedShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(50, 50, 100, 100),
      Shadow(Vector2dF(8.0f, 8.0f), 0.0f, ColorRGBA(0, 0, 0, 1)), false, 0.0f,
      RoundedCorners(50, 50)));
}

TEST_F(PixelTest, BoxShadowUnderTransparentCircleBottomRightBlueBackground) {
  TestTree(CreateRoundedShadowRectWithBackground(
      output_surface_size(), ColorRGBA(0.4f, 0.4f, 1.0f, 1.0f),
      ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f), RectF(50, 50, 100, 100),
      Shadow(Vector2dF(8.0f, 8.0f), 0.0f, ColorRGBA(0, 0, 0, 1)), false, 0.0f,
      RoundedCorners(50, 50)));
}

TEST_F(PixelTest, BoxShadowCircleSpread) {
  TestTree(CreateRoundedShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(50, 50, 100, 100),
      Shadow(Vector2dF(0.0f, 0.0f), 0.0f, ColorRGBA(0, 0, 0, 1)), false, 10.0f,
      RoundedCorners(50, 50)));
}

TEST_F(PixelTest, BoxShadowInsetCircleBottomRight) {
  TestTree(CreateRoundedShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(50, 50, 100, 100),
      Shadow(Vector2dF(8.0f, 8.0f), 0.0f, ColorRGBA(0, 0, 0, 1)), true, 0.0f,
      RoundedCorners(50, 50)));
}

TEST_F(PixelTest,
       BoxShadowInsetUnderTransparentCircleBottomRightBlueBackground) {
  TestTree(CreateRoundedShadowRectWithBackground(
      output_surface_size(), ColorRGBA(0.4f, 0.4f, 1.0f, 1.0f),
      ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f), RectF(50, 50, 100, 100),
      Shadow(Vector2dF(8.0f, 8.0f), 0.0f, ColorRGBA(0, 0, 0, 1)), true, 0.0f,
      RoundedCorners(50, 50)));
}

TEST_F(PixelTest, BoxShadowInsetCircleSpread) {
  TestTree(CreateRoundedShadowRectWithBackground(
      output_surface_size(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
      ColorRGBA(0.5f, 0.5f, 0.5f, 1.0f), RectF(50, 50, 100, 100),
      Shadow(Vector2dF(0.0f, 0.0f), 0.0f, ColorRGBA(0, 0, 0, 1)), true, 10.0f,
      RoundedCorners(50, 50)));
}

// PunchThroughVideoNode should trigger the painting of a solid rectangle with
// RGBA(0, 0, 0, 0) regardless of whether SetBoundsCB returns true or false.
TEST_F(PixelTest, PunchThroughVideoNodePunchesThroughSetBoundsCBReturnsFalse) {
  CompositionNode::Builder builder;
  builder.AddChild(new RectNode(RectF(25, 25, 150, 150),
                                scoped_ptr<Brush>(new SolidColorBrush(
                                    ColorRGBA(0.5f, 0.5f, 1.0f, 1.0f)))));

  builder.AddChild(new PunchThroughVideoNode(PunchThroughVideoNode::Builder(
      RectF(50, 50, 100, 100), base::Bind(SetBounds, false))));

  TestTree(new CompositionNode(builder.Pass()));
}

TEST_F(PixelTest, PunchThroughVideoNodePunchesThroughSetBoundsCBReturnsTrue) {
  CompositionNode::Builder builder;
  builder.AddChild(new RectNode(RectF(25, 25, 150, 150),
                                scoped_ptr<Brush>(new SolidColorBrush(
                                    ColorRGBA(0.5f, 0.5f, 1.0f, 1.0f)))));

  builder.AddChild(new PunchThroughVideoNode(PunchThroughVideoNode::Builder(
      RectF(50, 50, 100, 100), base::Bind(SetBounds, true))));

  TestTree(new CompositionNode(builder.Pass()));
}

TEST_F(PixelTest, DrawOffscreenImage) {
  scoped_refptr<Node> root = CreateCascadedRectsOfDifferentColors(
      ScaleSize(output_surface_size(), 0.5f, 0.5f));
  scoped_refptr<Image> offscreen_image =
      GetResourceProvider()->DrawOffscreenImage(root);
  TestTree(new ImageNode(offscreen_image));
}

#if !SB_HAS(BLITTER)
// Tests that offscreen rendering works fine with YUV images.
TEST_F(PixelTest, DrawOffscreenYUVImage) {
  scoped_refptr<Image> image =
      MakeI420Image(GetResourceProvider(), output_surface_size());

  scoped_refptr<Image> offscreen_rendered_image =
      GetResourceProvider()->DrawOffscreenImage(new ImageNode(image));

  TestTree(new ImageNode(offscreen_rendered_image));
}
#endif  // !SB_HAS(BLITTER)

#if defined(ENABLE_MAP_TO_MESH)

namespace {
scoped_refptr<Mesh> CreateCubeMesh(ResourceProvider* resource_provider) {
  // Defines a cube mesh where each face faces inward.  Each face has the entire
  // texture mapped over it.
  scoped_ptr<std::vector<Mesh::Vertex> > vertices(
      new std::vector<Mesh::Vertex>);

  vertices->push_back(Mesh::Vertex(-1.0f, -1.0f, -1.0f, 0.0f, 0.0f));
  vertices->push_back(Mesh::Vertex(-1.0f, 1.0f, -1.0f, 1.0f, 0.0f));
  vertices->push_back(Mesh::Vertex(-1.0f, 1.0f, 1.0f, 1.0f, 1.0f));
  vertices->push_back(Mesh::Vertex(-1.0f, -1.0f, -1.0f, 0.0f, 0.0f));
  vertices->push_back(Mesh::Vertex(-1.0f, 1.0f, 1.0f, 1.0f, 1.0f));
  vertices->push_back(Mesh::Vertex(-1.0f, -1.0f, 1.0f, 0.0f, 1.0f));

  vertices->push_back(Mesh::Vertex(1.0f, -1.0f, -1.0f, 0.0f, 0.0f));
  vertices->push_back(Mesh::Vertex(1.0f, 1.0f, 1.0f, 1.0f, 1.0f));
  vertices->push_back(Mesh::Vertex(1.0f, 1.0f, -1.0f, 1.0f, 0.0f));
  vertices->push_back(Mesh::Vertex(1.0f, -1.0f, -1.0f, 0.0f, 0.0f));
  vertices->push_back(Mesh::Vertex(1.0f, -1.0f, 1.0f, 0.0f, 1.0f));
  vertices->push_back(Mesh::Vertex(1.0f, 1.0f, 1.0f, 1.0f, 1.0f));

  vertices->push_back(Mesh::Vertex(-1.0f, 1.0f, -1.0f, 0.0f, 0.0f));
  vertices->push_back(Mesh::Vertex(1.0f, 1.0f, -1.0f, 1.0f, 0.0f));
  vertices->push_back(Mesh::Vertex(1.0f, 1.0f, 1.0f, 1.0f, 1.0f));
  vertices->push_back(Mesh::Vertex(-1.0f, 1.0f, -1.0f, 0.0f, 0.0f));
  vertices->push_back(Mesh::Vertex(1.0f, 1.0f, 1.0f, 1.0f, 1.0f));
  vertices->push_back(Mesh::Vertex(-1.0f, 1.0f, 1.0f, 0.0f, 1.0f));

  vertices->push_back(Mesh::Vertex(-1.0f, -1.0f, -1.0f, 0.0f, 0.0f));
  vertices->push_back(Mesh::Vertex(1.0f, -1.0f, 1.0f, 1.0f, 1.0f));
  vertices->push_back(Mesh::Vertex(1.0f, -1.0f, -1.0f, 1.0f, 0.0f));
  vertices->push_back(Mesh::Vertex(-1.0f, -1.0f, -1.0f, 0.0f, 0.0f));
  vertices->push_back(Mesh::Vertex(-1.0f, -1.0f, 1.0f, 0.0f, 1.0f));
  vertices->push_back(Mesh::Vertex(1.0f, -1.0f, 1.0f, 1.0f, 1.0f));

  vertices->push_back(Mesh::Vertex(-1.0f, -1.0f, -1.0f, 0.0f, 0.0f));
  vertices->push_back(Mesh::Vertex(1.0f, -1.0f, -1.0f, 1.0f, 0.0f));
  vertices->push_back(Mesh::Vertex(1.0f, 1.0f, -1.0f, 1.0f, 1.0f));
  vertices->push_back(Mesh::Vertex(-1.0f, -1.0f, -1.0f, 0.0f, 0.0f));
  vertices->push_back(Mesh::Vertex(1.0f, 1.0f, -1.0f, 1.0f, 1.0f));
  vertices->push_back(Mesh::Vertex(-1.0f, 1.0f, -1.0f, 0.0f, 1.0f));

  vertices->push_back(Mesh::Vertex(-1.0f, -1.0f, 1.0f, 0.0f, 0.0f));
  vertices->push_back(Mesh::Vertex(1.0f, 1.0f, 1.0f, 1.0f, 1.0f));
  vertices->push_back(Mesh::Vertex(1.0f, -1.0f, 1.0f, 1.0f, 0.0f));
  vertices->push_back(Mesh::Vertex(-1.0f, -1.0f, 1.0f, 0.0f, 0.0f));
  vertices->push_back(Mesh::Vertex(-1.0f, 1.0f, 1.0f, 0.0f, 1.0f));
  vertices->push_back(Mesh::Vertex(1.0f, 1.0f, 1.0f, 1.0f, 1.0f));

  return resource_provider->CreateMesh(
      vertices.Pass(), Mesh::kDrawModeTriangles);
}

// Creates a cube mesh with a perspective transform applied to it such that the
// camera is facing a corner of the cube, from the inside of the cube.
scoped_refptr<Node> CreateMapToMeshTestRenderTree(
    ResourceProvider* resource_provider, const scoped_refptr<Image>& texture) {
  scoped_refptr<Mesh> cube_mesh = CreateCubeMesh(resource_provider);
  MapToMeshFilter::Builder map_to_mesh_builder;
  map_to_mesh_builder.SetDefaultMeshes(cube_mesh, cube_mesh);
  scoped_refptr<FilterNode> map_to_mesh_filter(
      new FilterNode(MapToMeshFilter(render_tree::kMono, map_to_mesh_builder),
                     new ImageNode(texture)));

  glm::mat4 model_view =
      glm::rotate(static_cast<float>(M_PI / 4.0f), glm::vec3(1.0f, 0, 0)) *
      glm::rotate(static_cast<float>(M_PI / 4.0f), glm::vec3(0, 1.0f, 0));

  const float kNearZ = 0.01f;
  const float kFarZ = 1000.0f;
  const float kVerticalFOV = static_cast<float>(M_PI / 2.0f);
  const float kAspectRatio = 1.0f;
  glm::mat4 projection =
      glm::perspectiveRH(kVerticalFOV, kAspectRatio, kNearZ, kFarZ);

  return new MatrixTransform3DNode(map_to_mesh_filter, projection * model_view);
}
}  // namespace

TEST_F(PixelTest, MapToMeshRGBTest) {
  // Tests that MapToMesh filter works as expected with an RGBA texture.
  scoped_refptr<Image> image =
      CreateColoredCheckersImage(GetResourceProvider(), Size(200, 200));
  TestTree(CreateMapToMeshTestRenderTree(GetResourceProvider(), image));
}

#if NV12_TEXTURE_SUPPORTED

TEST_F(PixelTest, MapToMeshNV12Test) {
  // Tests that MapToMesh filter works as expected with a NV12 YUV texture.
  scoped_refptr<Image> image =
      MakeNV12Image(GetResourceProvider(), Size(200, 200));
  TestTree(CreateMapToMeshTestRenderTree(GetResourceProvider(), image));
}

#endif  // #if NV12_TEXTURE_SUPPORTED

TEST_F(PixelTest, MapToMeshI420Test) {
  // Tests that MapToMesh filter works as expected with a I420 YUV texture.
  scoped_refptr<Image> image =
      MakeI420Image(GetResourceProvider(), Size(200, 200));
  TestTree(CreateMapToMeshTestRenderTree(GetResourceProvider(), image));
}

TEST_F(PixelTest, MapToMeshUYVYTest) {
  if (!GetResourceProvider()->PixelFormatSupported(
          render_tree::kPixelFormatUYVY)) {
    return;
  }

  // Tests that MapToMesh filter works as expected with a I420 YUV texture.
  scoped_refptr<Image> image =
      MakeUYVYImage(GetResourceProvider(), Size(200, 200));
  TestTree(CreateMapToMeshTestRenderTree(GetResourceProvider(), image));
}

#endif  // defined(ENABLE_MAP_TO_MESH)

TEST_F(PixelTest, DrawNullImage) {
  // An ImageNode with no source is legal, though it should result in nothing
  // being drawn.
  TestTree(new ImageNode(NULL, math::RectF(output_surface_size())));
}

}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
