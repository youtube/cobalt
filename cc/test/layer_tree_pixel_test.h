// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_LAYER_TREE_PIXEL_TEST_H_
#define CC_TEST_LAYER_TREE_PIXEL_TEST_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "cc/test/layer_tree_test.h"
#include "cc/trees/clip_node.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/scroll_node.h"
#include "cc/trees/transform_node.h"
#include "ui/gl/gl_implementation.h"

class SkBitmap;

namespace base {
namespace test {
class ScopedFeatureList;
}
}  // namespace base

namespace viz {
class CopyOutputRequest;
class CopyOutputResult;
}

namespace cc {
class PixelComparator;
class SolidColorLayer;
class TextureLayer;

class LayerTreePixelTest : public LayerTreeTest {
 protected:
  explicit LayerTreePixelTest(viz::RendererType renderer_type);
  ~LayerTreePixelTest() override;

  // LayerTreeTest overrides.
  std::unique_ptr<TestLayerTreeFrameSink> CreateLayerTreeFrameSink(
      const viz::RendererSettings& renderer_settings,
      double refresh_rate,
      scoped_refptr<viz::ContextProvider> compositor_context_provider,
      scoped_refptr<viz::RasterContextProvider> worker_context_provider)
      override;
  std::unique_ptr<viz::DisplayCompositorMemoryAndTaskController>
  CreateDisplayControllerOnThread() override;
  std::unique_ptr<viz::SkiaOutputSurface> CreateSkiaOutputSurfaceOnThread(
      viz::DisplayCompositorMemoryAndTaskController*) override;
  std::unique_ptr<viz::OutputSurface> CreateSoftwareOutputSurfaceOnThread()
      override;
  void DrawLayersOnThread(LayerTreeHostImpl* host_impl) override;
  void InitializeSettings(LayerTreeSettings* settings) override;

  virtual std::unique_ptr<viz::CopyOutputRequest> CreateCopyOutputRequest();

  void ReadbackResult(std::unique_ptr<viz::CopyOutputResult> result);

  void BeginTest() override;
  void SetupTree() override;
  void AfterTest() override;
  void EndTest() override;

  void TryEndTest();

  scoped_refptr<SolidColorLayer> CreateSolidColorLayer(const gfx::Rect& rect,
                                                       SkColor color);
  scoped_refptr<SolidColorLayer> CreateSolidColorLayerWithBorder(
      const gfx::Rect& rect,
      SkColor color,
      int border_width,
      SkColor border_color);

  void CreateSolidColorLayerPlusBorders(
      const gfx::Rect& rect,
      SkColor color,
      int border_width,
      SkColor border_color,
      std::vector<scoped_refptr<SolidColorLayer>>&);

  void RunPixelTest(scoped_refptr<Layer> content_root,
                    base::FilePath file_name);

  void RunPixelTest(scoped_refptr<Layer> content_root,
                    const SkBitmap& expected_bitmap);

  void RunPixelTestWithLayerList(base::FilePath file_name);

  void RunSingleThreadedPixelTest(scoped_refptr<Layer> content_root,
                                  base::FilePath file_name);

  void RunPixelTestWithReadbackTarget(scoped_refptr<Layer> content_root,
                                      Layer* target,
                                      base::FilePath file_name);

  // Allow tests to enlarge the backing texture for a non-root render pass, to
  // simulate reusing a larger texture from a previous frame for a new
  // render pass. This should be called before the output surface is bound.
  void set_enlarge_texture_amount(const gfx::Size& enlarge_texture_amount) {
    enlarge_texture_amount_ = enlarge_texture_amount;
  }

  // Gpu rasterization is not used in pixel tests by default, and OOP
  // rasterization is used by default only for Vulkan and Skia Dawn. Tests may
  // opt into using a different raster mode.
  void set_raster_type(TestRasterType raster_type) {
    raster_type_ = raster_type;
  }

  TestRasterType raster_type() const { return raster_type_; }
  bool use_accelerated_raster() const {
    return raster_type_ == TestRasterType::kGpu;
  }

  // Common CSS colors defined for tests to use.
  static const SkColor kCSSOrange = 0xffffa500;
  static const SkColor kCSSBrown = 0xffa52a2a;
  static const SkColor kCSSGreen = 0xff008000;
  static const SkColor kCSSLime = 0xff00ff00;
  static const SkColor kCSSBlack = 0xff000000;

  TestRasterType raster_type_;
  gl::DisableNullDrawGLBindings enable_pixel_output_;
  std::unique_ptr<PixelComparator> pixel_comparator_;
  scoped_refptr<Layer> content_root_;  // Not used in layer list mode.
  raw_ptr<Layer> readback_target_;
  base::FilePath ref_file_;
  SkBitmap expected_bitmap_;
  std::unique_ptr<SkBitmap> result_bitmap_;
  std::vector<scoped_refptr<TextureLayer>> texture_layers_;
  int pending_texture_mailbox_callbacks_;
  gfx::Size enlarge_texture_amount_;
  int max_texture_size_ = 0;

  // Used to create SkiaOutputSurfaceImpl.
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
};

}  // namespace cc

#endif  // CC_TEST_LAYER_TREE_PIXEL_TEST_H_
