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

#include <ostream>
#include <vector>

#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "cobalt/base/cobalt_paths.h"
#include "cobalt/layout_tests/layout_snapshot.h"
#include "cobalt/layout_tests/test_parser.h"
#include "cobalt/layout_tests/test_utils.h"
#include "cobalt/math/size.h"
#include "cobalt/render_tree/animations/animate_node.h"
#include "cobalt/renderer/render_tree_pixel_tester.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace layout_tests {

namespace switches {
// If enabled, running the tests will result in the generation of expected
// test output from the actual output to the output directory.
const char kRebaseline[] = "rebaseline";

// If enabled, will run all tests and display which ones passed and which
// ones failed, as usual, however it will also output new expected output
// images for ONLY the tests that failed.
const char kRebaselineFailedTests[] = "rebaseline-failed-tests";

// If enabled, will output details (in the form of files placed in the output
// directory) for all tests that fail.
const char kOutputFailedTestDetails[] = "output-failed-test-details";

// Like kOutputFailedTestDetails, but outputs details for tests that
// succeed as well.
const char kOutputAllTestDetails[] = "output-all-test-details";
}  // namespace switches

class LayoutTest : public ::testing::TestWithParam<TestInfo> {};
TEST_P(LayoutTest, LayoutTest) {
  // Output the name of the current input file so that it is visible in test
  // output.
  std::cout << "(" << GetParam() << ")" << std::endl;

  // Setup a message loop for the current thread since we will be constructing
  // a WebModule, which requires a message loop to exist for the current
  // thread.
  MessageLoop message_loop(MessageLoop::TYPE_DEFAULT);

  // Setup the pixel tester we will use to perform pixel tests on the render
  // trees output by the web module.
  renderer::RenderTreePixelTester::Options pixel_tester_options;
  pixel_tester_options.output_failed_test_details =
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kOutputFailedTestDetails);
  pixel_tester_options.output_all_test_details =
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kOutputAllTestDetails);

  // Resolve the viewport size to a default if the test did not explicitly
  // specify a size.  The size of the test viewport tells us how many pixels our
  // layout tests will have available to them.  We must make a trade-off between
  // room for tests to maneuver within and speed at which pixel tests can be
  // done.
  const math::Size kDefaultViewportSize(640, 360);
  math::Size viewport_size = GetParam().viewport_size
                                 ? *GetParam().viewport_size
                                 : kDefaultViewportSize;

  renderer::RenderTreePixelTester pixel_tester(
      viewport_size, GetTestInputRootDirectory(), GetTestOutputRootDirectory(),
      pixel_tester_options);

  browser::WebModule::LayoutResults layout_results = SnapshotURL(
      GetParam().url, viewport_size, pixel_tester.GetResourceProvider());

  scoped_refptr<render_tree::animations::AnimateNode> animate_node =
      new render_tree::animations::AnimateNode(layout_results.render_tree);

  scoped_refptr<render_tree::animations::AnimateNode> animated_node =
      animate_node->Apply(layout_results.layout_time).animated;

  // We reapply the animation application with the exact same layout time as
  // before.  This is an extra sanity check to ensure that the animated results
  // are deterministic.
  scoped_refptr<render_tree::animations::AnimateNode> twice_animated_node =
      animated_node->Apply(layout_results.layout_time).animated;
  EXPECT_EQ(animated_node, twice_animated_node);

  scoped_refptr<render_tree::Node> static_render_tree =
      twice_animated_node->source();

  bool results =
      pixel_tester.TestTree(static_render_tree, GetParam().base_file_path);

  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kRebaseline) ||
      (!results &&
       CommandLine::ForCurrentProcess()->HasSwitch(
           switches::kRebaselineFailedTests))) {
    pixel_tester.Rebaseline(static_render_tree, GetParam().base_file_path);
  }

  EXPECT_TRUE(results);
}

// Cobalt-specific test cases.
INSTANTIATE_TEST_CASE_P(CobaltSpecificLayoutTests, LayoutTest,
                        ::testing::ValuesIn(EnumerateLayoutTests("cobalt")));
// Custom CSS 2.1 (https://www.w3.org/TR/CSS21/) test cases.
INSTANTIATE_TEST_CASE_P(CSS21LayoutTests, LayoutTest,
                        ::testing::ValuesIn(EnumerateLayoutTests("css-2-1")));
// Custom CSS Background (https://www.w3.org/TR/css3-background/) test cases.
INSTANTIATE_TEST_CASE_P(
    CSSBackground3LayoutTests, LayoutTest,
    ::testing::ValuesIn(EnumerateLayoutTests("css3-background")));
// Custom CSS Color (https://www.w3.org/TR/css3-color/) test cases.
INSTANTIATE_TEST_CASE_P(
    CSSColor3LayoutTests, LayoutTest,
    ::testing::ValuesIn(EnumerateLayoutTests("css3-color")));
// Custom CSS Images (https://www.w3.org/TR/css3-images/) test cases.
INSTANTIATE_TEST_CASE_P(
    CSSImages3LayoutTests, LayoutTest,
    ::testing::ValuesIn(EnumerateLayoutTests("css3-images")));
// Custom CSS Text (https://www.w3.org/TR/css-text-3/) test cases.
INSTANTIATE_TEST_CASE_P(
    CSSText3LayoutTests, LayoutTest,
    ::testing::ValuesIn(EnumerateLayoutTests("css-text-3")));
// Custom CSS Transform (http://https://www.w3.org/TR/css-transforms/)
// test cases.
INSTANTIATE_TEST_CASE_P(
    CSSTransformsLayoutTests, LayoutTest,
    ::testing::ValuesIn(EnumerateLayoutTests("css-transforms")));
// Custom CSS Transition
// (https://www.w3.org/TR/2013/WD-css3-transitions-20131119/)
// test cases.
INSTANTIATE_TEST_CASE_P(
    CSSTransitionLayoutTests, LayoutTest,
    ::testing::ValuesIn(EnumerateLayoutTests("css3-transitions")));
// Custom CSS Animation
// (https://www.w3.org/TR/2013/WD-css3-animations-20130219/#animations)
// test cases.
INSTANTIATE_TEST_CASE_P(
    CSSAnimationLayoutTests, LayoutTest,
    ::testing::ValuesIn(EnumerateLayoutTests("css3-animations")));
// Custom bidi text (http://www.unicode.org/reports/tr9/)
// (https://www.w3.org/TR/CSS21/visuren.html#direction) test cases.
INSTANTIATE_TEST_CASE_P(
    BidiLayoutTests, LayoutTest,
    ::testing::ValuesIn(EnumerateLayoutTests("bidi")));
// Custom text shaping test cases.
INSTANTIATE_TEST_CASE_P(
    TextShapingLayoutTests, LayoutTest,
    ::testing::ValuesIn(EnumerateLayoutTests("text-shaping")));
// Custom CSS Conditional (https://www.w3.org/TR/css3-conditional/) test cases.
INSTANTIATE_TEST_CASE_P(
    CSSConditional3LayoutTests, LayoutTest,
    ::testing::ValuesIn(EnumerateLayoutTests("css3-conditional")));
// Custom CSS Font (https://www.w3.org/TR/css3-fonts/) test cases.
INSTANTIATE_TEST_CASE_P(
    CSS3FontsLayoutTests, LayoutTest,
    ::testing::ValuesIn(EnumerateLayoutTests("css3-fonts")));
// Custom CSS Text Decor (https://www.w3.org/TR/css-text-decor-3/) test cases.
INSTANTIATE_TEST_CASE_P(
    CSS3TextDecorLayoutTests, LayoutTest,
    ::testing::ValuesIn(EnumerateLayoutTests("css3-text-decor")));
// Custom CSS UI (https://www.w3.org/TR/css3-ui/) test cases.
INSTANTIATE_TEST_CASE_P(CSS3UILayoutTests, LayoutTest,
                        ::testing::ValuesIn(EnumerateLayoutTests("css3-ui")));
// Custom CSS Value (https://www.w3.org/TR/css3-values/) test cases.
INSTANTIATE_TEST_CASE_P(
    CSS3ValuesLayoutTests, LayoutTest,
    ::testing::ValuesIn(EnumerateLayoutTests("css3-values")));
// Custom incremental layout test cases.
INSTANTIATE_TEST_CASE_P(
    IncrementalLayoutLayoutTests, LayoutTest,
    ::testing::ValuesIn(EnumerateLayoutTests("incremental-layout")));
// Custom CSSOM view (https://www.w3.org/TR/2013/WD-cssom-view-20131217/)
// test cases.
INSTANTIATE_TEST_CASE_P(
    CSSOMViewLayoutTests, LayoutTest,
    ::testing::ValuesIn(EnumerateLayoutTests("cssom-view")));

// JavaScript HTML5 WebAPIs (https://www.w3.org/TR/html5/webappapis.html) test
// cases.
INSTANTIATE_TEST_CASE_P(
    WebAppAPIsLayoutTests, LayoutTest,
    ::testing::ValuesIn(EnumerateLayoutTests("webappapis")));

// JavaScript HTML5 APIs that describe requestAnimationFrame().
//   https://www.w3.org/TR/animation-timing/
INSTANTIATE_TEST_CASE_P(
    AnimationTimingAPILayoutTests, LayoutTest,
    ::testing::ValuesIn(EnumerateLayoutTests("animation-timing")));

// Problematic test cases found through cluster-fuzz.
INSTANTIATE_TEST_CASE_P(
    ClusterFuzzLayoutTests, LayoutTest,
    ::testing::ValuesIn(EnumerateLayoutTests("cluster-fuzz")));

// Disable on Windows until network stack is implemented.
#if !defined(COBALT_WIN)
// Content Security Policy test cases.
INSTANTIATE_TEST_CASE_P(
    ContentSecurityPolicyTests, LayoutTest,
    ::testing::ValuesIn(EnumerateLayoutTests("csp")));
#endif  // !defined(COBALT_WIN)

}  // namespace layout_tests
}  // namespace cobalt
