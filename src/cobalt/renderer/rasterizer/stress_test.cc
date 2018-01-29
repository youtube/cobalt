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

#include <vector>

#include "cobalt/render_tree/brush.h"
#include "cobalt/render_tree/composition_node.h"
#include "cobalt/render_tree/filter_node.h"
#include "cobalt/render_tree/image_node.h"
#include "cobalt/render_tree/rect_node.h"
#include "cobalt/renderer/backend/default_graphics_system.h"
#include "cobalt/renderer/backend/graphics_context.h"
#include "cobalt/renderer/backend/graphics_system.h"
#include "cobalt/renderer/backend/render_target.h"
#include "cobalt/renderer/rasterizer/rasterizer.h"
#include "cobalt/renderer/renderer_module.h"
#include "testing/gtest/include/gtest/gtest.h"

using cobalt::math::RectF;
using cobalt::math::Size;
using cobalt::math::SizeF;
using cobalt::math::Vector2dF;
using cobalt::render_tree::Brush;
using cobalt::render_tree::ColorRGBA;
using cobalt::render_tree::CompositionNode;
using cobalt::render_tree::FilterNode;
using cobalt::render_tree::Image;
using cobalt::render_tree::ImageData;
using cobalt::render_tree::ImageNode;
using cobalt::render_tree::Node;
using cobalt::render_tree::OpacityFilter;
using cobalt::render_tree::RectNode;
using cobalt::render_tree::SolidColorBrush;

namespace cobalt {
namespace renderer {
namespace rasterizer {

// The renderer stress tests are intended to simply ensure that no crash occurs.
// They usually create a lot of a certain object type, or attempt to use very
// large surfaces.  The final output doesn't really matter, and it is not
// checked, we just don't want to crash.  For example, some of the tests
// ensure that we don't crash when we request a rasterization that requires
// very large framebuffers that the rasterizer is not likely to be able to
// allocate.
class StressTest : public testing::Test {
 public:
  StressTest();

  void TestTree(const Size& output_size, scoped_refptr<Node> tree);
  render_tree::ResourceProvider* GetResourceProvider() const {
    return rasterizer_->GetResourceProvider();
  }

 protected:
  scoped_ptr<backend::GraphicsSystem> graphics_system_;
  scoped_ptr<backend::GraphicsContext> graphics_context_;
  scoped_ptr<rasterizer::Rasterizer> rasterizer_;
  scoped_refptr<backend::RenderTarget> render_target_;
};

StressTest::StressTest() {
  graphics_system_ = backend::CreateDefaultGraphicsSystem();
  graphics_context_ = graphics_system_->CreateGraphicsContext();
  // Create the rasterizer using the platform default RenderModule options.
  RendererModule::Options render_module_options;
  rasterizer_ = render_module_options.create_rasterizer_function.Run(
      graphics_context_.get(), render_module_options);
}

void StressTest::TestTree(const Size& output_size, scoped_refptr<Node> tree) {
  // Reuse render targets to avoid some rasterizers from thrashing their
  // render target cache.
  if (!render_target_ || render_target_->GetSize() != output_size) {
    render_target_ = graphics_context_->CreateDownloadableOffscreenRenderTarget(
        output_size);
  }

  if (!render_target_) {
    LOG(WARNING)
        << "Failed to create render target, no rasterization will take place.";
  } else {
    rasterizer::Rasterizer::Options rasterizer_options;
    rasterizer_options.flags = rasterizer::Rasterizer::kSubmitFlags_Clear;
    rasterizer_->Submit(tree, render_target_, rasterizer_options);
  }
}

// Test that we can create a large framebuffer, but not so large that it
// will fail on all platforms.
TEST_F(StressTest, LargeFramebuffer) {
  Size kLargeFramebufferSize(8000, 8000);
  TestTree(kLargeFramebufferSize,
           new RectNode(RectF(kLargeFramebufferSize),
                        scoped_ptr<Brush>(new SolidColorBrush(
                            ColorRGBA(1.0f, 0.0f, 0.0f, 1.0f)))));
}

// Test that we can create a very large framebuffer that may fail to create
// on most platforms.
TEST_F(StressTest, VeryLargeFramebuffer) {
  Size kLargeFramebufferSize(20000, 20000);
  TestTree(kLargeFramebufferSize,
           new RectNode(RectF(kLargeFramebufferSize),
                        scoped_ptr<Brush>(new SolidColorBrush(
                            ColorRGBA(1.0f, 0.0f, 0.0f, 1.0f)))));
}

namespace {
// Creates a composition of |cascade_amount| render trees layed out in a
// cascade on top of each other.
scoped_refptr<Node> CreateCascadedRenderTrees(Node* node, int cascade_amount,
                                              float offset_amount) {
  CompositionNode::Builder composition_builder;
  for (int i = 0; i < cascade_amount; ++i) {
    composition_builder.AddChild(new CompositionNode(
        node, Vector2dF(i * offset_amount, i * offset_amount)));
  }

  return new CompositionNode(composition_builder.Pass());
}

// Creates and returns a render tree with |num_layers| opacity objects overlaid
// on top of themselves, each with the specified size.  Each opacity layer
// contains a set of 3 rectangles, with an opacity filter applied on top of
// all 3 of them.
scoped_refptr<Node> CreateOpacityLayers(int num_layers, const Size& size) {
  const float kCascadeOffset = 25.0f;
  const int kNumCascadedRects = 3;
  int rect_size_inset = (kNumCascadedRects - 1) * kCascadeOffset;
  Size rect_size(size.width() - rect_size_inset,
                 size.height() - rect_size_inset);

  FilterNode* opacity_layer =
      new FilterNode(OpacityFilter(0.9f),
                     CreateCascadedRenderTrees(
                         new RectNode(RectF(rect_size),
                                      scoped_ptr<Brush>(new SolidColorBrush(
                                          ColorRGBA(1.0f, 0.0f, 0.0f, 1.0f)))),
                         kNumCascadedRects, kCascadeOffset));

  return CreateCascadedRenderTrees(opacity_layer, num_layers, 0.0f);
}
}  // namespace

// Test that we can create many (8^3) medium-sized opacity layers (which, in
// most rasterizers, imply the creation of offscreen surfaces).
TEST_F(StressTest, ManyOpacityLayers) {
  Size kFramebufferSize(1920, 1080);
  TestTree(kFramebufferSize, CreateOpacityLayers(250, Size(200, 200)));
}

TEST_F(StressTest, FewLargeOpacityLayers) {
  Size kFramebufferSize(1920, 1080);
  TestTree(kFramebufferSize, CreateOpacityLayers(50, kFramebufferSize));
}

TEST_F(StressTest, FewVeryLargeOpacityLayers) {
  Size kFramebufferSize(1920, 1080);
  TestTree(kFramebufferSize, CreateOpacityLayers(50, Size(9000, 9000)));
}

TEST_F(StressTest, TooManyTextures) {
  // Try to use all the available texture memory in order to test failure to
  // allocate a texture.
  const int kTextureMemoryMb = 2 * 1024;

  const Size kFramebufferSize(1920, 1080);
  const Size kTextureSize(2048, 2048);
  const int kTextureSizeMb = kTextureSize.GetArea() * 4 / (1024 * 1024);
  const int kNumTextures = kTextureMemoryMb / kTextureSizeMb + 1;

  render_tree::PixelFormat pixel_format = render_tree::kPixelFormatRGBA8;
  if (!GetResourceProvider()->PixelFormatSupported(pixel_format)) {
    pixel_format = render_tree::kPixelFormatBGRA8;
  }

  std::vector<scoped_refptr<Image>> images;
  for (int i = 0; i < kNumTextures; ++i) {
    // On most platforms, AllocateImageData allocates CPU memory and
    // CreateImage will allocate GPU memory (and release the CPU memory) when
    // the image is needed. To exercise running out of GPU memory, try
    // rendering each new image so the CPU allocation can be released before
    // the next image is allocated.
    scoped_ptr<ImageData> image_data = GetResourceProvider()->AllocateImageData(
        kTextureSize, pixel_format, render_tree::kAlphaFormatOpaque);
    if (!image_data) {
      break;
    }
    images.emplace_back(GetResourceProvider()->CreateImage(
        image_data.Pass()));
    TestTree(kFramebufferSize, new ImageNode(images.back()));
  }
}

}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
