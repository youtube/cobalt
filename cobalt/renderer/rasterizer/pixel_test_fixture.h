// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_RENDERER_RASTERIZER_PIXEL_TEST_FIXTURE_H_
#define COBALT_RENDERER_RASTERIZER_PIXEL_TEST_FIXTURE_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "cobalt/math/size_f.h"
#include "cobalt/render_tree/node.h"
#include "cobalt/renderer/backend/graphics_context.h"
#include "cobalt/renderer/backend/graphics_system.h"
#include "cobalt/renderer/render_tree_pixel_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {

// The PixelTest class is mostly a wrapper around RenderTreePixelTester.
// Please see documentation for that class.
class PixelTest : public testing::Test {
 public:
  PixelTest();
  ~PixelTest();

  static void SetUpTestCase();
  static void TearDownTestCase();

 protected:
  // This function will be executed by individual tests.  The current test's
  // name will be used to identify and load the relevant test data expected
  // output image from disk.  Additionally the current test's name will be used
  // to determine the names of output files when a rebase is requested, or when
  // an error mask image is requested.
  void TestTree(const scoped_refptr<render_tree::Node>& test_tree);
  void TestTree(const scoped_refptr<render_tree::Node>& test_tree,
                const base::FilePath& expected_base_filename);

  const math::Size& output_surface_size() const { return output_surface_size_; }

  render_tree::ResourceProvider* GetResourceProvider() const {
    return pixel_tester_->GetResourceProvider();
  }

  static backend::GraphicsSystem* graphics_system_;
  static backend::GraphicsContext* graphics_context_;

  bool IsMapToMeshEnabled() { return pixel_tester_->IsMapToMeshEnabled(); }

  std::vector<uint8> GetFileData(const base::FilePath& file_path);

 private:
  base::Optional<RenderTreePixelTester> pixel_tester_;
  math::Size output_surface_size_;
};

}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_PIXEL_TEST_FIXTURE_H_
