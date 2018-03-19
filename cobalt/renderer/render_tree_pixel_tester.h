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

#ifndef COBALT_RENDERER_RENDER_TREE_PIXEL_TESTER_H_
#define COBALT_RENDERER_RENDER_TREE_PIXEL_TESTER_H_

#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/math/size.h"
#include "cobalt/render_tree/node.h"
#include "cobalt/renderer/backend/graphics_context.h"
#include "cobalt/renderer/backend/graphics_system.h"
#include "cobalt/renderer/backend/render_target.h"
#include "cobalt/renderer/rasterizer/rasterizer.h"

namespace cobalt {
namespace renderer {

// The render tree pixel tester allows one to compare the output of a rasterized
// render tree with a saved expected ground truth version of the output image.
// The ground truth images are stored in filenames based off of the root
// expected results directory passed into the constructor, and a relative path
// provided on each call to TestTree().
class RenderTreePixelTester {
 public:
  struct Options {
    Options();

    // Determines the size of the Gaussian that will be convolved
    // with the actual and expected image before they are compared.
    float gaussian_blur_sigma;

    // Determines the range of error for each color channel that we are willing
    // to accept when we declare a actual pixel is equal to an expected pixel.
    float acceptable_channel_range;

    // If enabled, it will result in the output of the actual image, expected
    // output image, and a diff image that highlights any differing regions for
    // all tests that fail.
    bool output_failed_test_details;

    // Similar to output_failed_test_details, but if this flag is enabled, even
    // passing tests will have their details output.  This might be useful to
    // inspect if there are differences that are not caught by the tests because
    // of the image fuzzing (Gaussian blur + error range) that occurs within
    // these tests.
    bool output_all_test_details;
  };

  // Setups up a render tree pixel tester context.  The pixel tests will occur
  // on a test surface of test_surface_dimensions dimensions.  All expected
  // output will be seeked for within the expected_results_directory directory.
  // If rebaselining or outputting test details, this output will appear in
  // output_directory.
  RenderTreePixelTester(const math::Size& test_surface_dimensions,
                        const FilePath& expected_results_directory,
                        const FilePath& output_directory,
                        const Options& options);
  ~RenderTreePixelTester();

  // Given a render tree and a base filename, rasterizes the render tree and
  // saves it as the expected output file in the directory rooted at
  // output_directory passed into the constructor.
  void Rebaseline(const scoped_refptr<cobalt::render_tree::Node>& test_tree,
                  const FilePath& expected_base_filename) const;

  // Given the passed in render tree and based on the expected output
  // found in a file based on expected_base_filename and
  // expected_results_directory (passed in via the constructor), returns if
  // the rasterized render tree matches the expected output.  Depending
  // on options passed in to the constructor (e.g. output_failed_test_details),
  // this method may have the side-effect of outputting files that describe the
  // details of the comparison (e.g. expected and actual output).
  bool TestTree(const scoped_refptr<cobalt::render_tree::Node>& test_tree,
                const FilePath& expected_base_filename) const;

  render_tree::ResourceProvider* GetResourceProvider() const;

  scoped_array<uint8_t> RasterizeRenderTree(
      const scoped_refptr<cobalt::render_tree::Node>& test_tree) const;
  const math::Size& GetTargetSize() const { return test_surface_->GetSize(); }

 private:
  scoped_ptr<cobalt::renderer::backend::GraphicsSystem> graphics_system_;
  scoped_ptr<cobalt::renderer::backend::GraphicsContext> graphics_context_;
  scoped_ptr<cobalt::renderer::rasterizer::Rasterizer> rasterizer_;
  scoped_refptr<cobalt::renderer::backend::RenderTarget> test_surface_;

  FilePath expected_results_directory_;
  FilePath output_directory_;

  Options options_;
};

}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RENDER_TREE_PIXEL_TESTER_H_
