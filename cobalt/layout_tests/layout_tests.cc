/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#include <ostream>
#include <vector>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/string_util.h"
#include "cobalt/base/cobalt_paths.h"
#include "cobalt/browser/web_module.h"
#include "cobalt/math/size.h"
#include "cobalt/media/media_module_stub.h"
#include "cobalt/network/network_module.h"
#include "cobalt/renderer/render_tree_pixel_tester.h"
#include "cobalt/storage/storage_manager.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace layout_tests {

namespace switches {
// If enabled, running the tests will result in the generation of expected
// test output from the actual output to the output directory.
const char kRebaseline[] = "rebaseline";

// If enabled, will output details (in the form of files placed in the output
// directory) for all tests that fail.
const char kOutputFailedTestDetails[] = "output-failed-test-details";

// Like kOutputFailedTestDetails, but outputs details for tests that
// succeed as well.
const char kOutputAllTestDetails[] = "output-all-test-details";
}  // namespace switches

namespace {
// The size of the test viewport tells us how many pixels our layout tests
// will have available to them.  We must make a trade-off between room for
// tests to maneuver within and speed at which pixel tests can be done.
const math::Size kTestViewportSize(640, 360);

// Returns the relative path to Cobalt layout tests.  This can be appended
// to either base::DIR_SOURCE_ROOT to get the input directory or
// base::DIR_COBALT_TEST_OUT to get the output directory.
FilePath GetDirSourceRootRelativePath() {
  return FilePath(FILE_PATH_LITERAL("cobalt"))
      .Append(FILE_PATH_LITERAL("layout_tests"));
}
// Returns the root directory that all output will be placed within.  Output
// is generated when rebaselining test expected output, or when test details
// have been chosen to be output.
FilePath GetTestOutputRootDirectory() {
  FilePath dir_cobalt_test_out;
  PathService::Get(cobalt::paths::DIR_COBALT_TEST_OUT, &dir_cobalt_test_out);
  return dir_cobalt_test_out.Append(GetDirSourceRootRelativePath());
}

// Returns the root directory that all test input can be found in (e.g.
// the HTML files that define the tests, and the PNG/TXT files that define
// the expected output).
FilePath GetTestInputRootDirectory() {
  FilePath dir_source_root;
  PathService::Get(base::DIR_SOURCE_ROOT, &dir_source_root);
  return dir_source_root.Append(GetDirSourceRootRelativePath());
}

// Handles an incoming render tree by testing it against a pre-rendered
// grount truth/expected image via the RenderTreePixelTester.  Places the
// boolean result of the test in the result output parameter.  May generate
// output files if pixel_tester is constructed with options specifying that it
// do so upon tests.
void AcceptRenderTreeForTest(
    const FilePath& test_html_path,
    renderer::RenderTreePixelTester* pixel_tester, base::RunLoop* run_loop,
    bool* result, const scoped_refptr<render_tree::Node>& tree,
    const scoped_refptr<render_tree::animations::NodeAnimationsMap>&
        node_animations_map,
    base::TimeDelta render_tree_time) {
  *result = pixel_tester->TestTree(tree, test_html_path);
  MessageLoop::current()->PostTask(FROM_HERE, run_loop->QuitClosure());
}

// Handles an incoming render tree by rendering it to an expected output image
// file in the output directory.
void AcceptRenderTreeForRebaseline(
    const FilePath& test_html_path,
    renderer::RenderTreePixelTester* pixel_tester, base::RunLoop* run_loop,
    const scoped_refptr<render_tree::Node>& tree,
    const scoped_refptr<render_tree::animations::NodeAnimationsMap>&
        node_animations_map,
    base::TimeDelta render_tree_time) {
  pixel_tester->Rebaseline(tree, test_html_path);
  MessageLoop::current()->PostTask(FROM_HERE, run_loop->QuitClosure());
}

void AcceptDocumentError(base::RunLoop* run_loop, const std::string& error) {
  // If an error occurs while loading a document (i.e. this function is called)
  // then the test has failed, and we will NOT set the result variable to true.
  std::cout << "Error loading document: " << error.c_str() << std::endl;
  MessageLoop::current()->PostTask(FROM_HERE, run_loop->QuitClosure());
}

GURL GetURLFromBaseFilePath(const FilePath& base_file_path) {
  return GURL("file:///" + GetDirSourceRootRelativePath().value() + "/" +
              base_file_path.AddExtension("html").value());
}
}  // namespace

struct TestInfo {
  TestInfo(const FilePath& base_file_path, const GURL& url)
      : base_file_path(base_file_path), url(url) {}

  // The base_file_path gives a path (relative to the root layout_tests
  // directory) to the test base filename from which all related files (such
  // as the source HTML file and the expected output PNG file) can be
  // derived.  This essentially acts as the test's identifier.
  FilePath base_file_path;

  // The URL is what the layout tests will load and render to produce the
  // actual output.  It is commonly computed from base_file_path (via
  // GetURLFromBaseFilePath()), but it can also be stated explicitly in the case
  // that it is external from the layout_tests.
  GURL url;
};

// Define operator<< so that this test parameter can be printed by gtest if
// a test fails.
std::ostream& operator<<(std::ostream& out, const TestInfo& test_info) {
  return out << test_info.base_file_path.value();
}

class LayoutTest : public ::testing::TestWithParam<TestInfo> {};
TEST_P(LayoutTest, LayoutTest) {
  // Output the name of the current input file so that it is visible in test
  // output.
  std::cout << "(" << GetParam() << ")" << std::endl;

  // Setup a message loop for the current thread since we will be constructing
  // a WebModule, which requires a message loop to exist for the current
  // thread.
  MessageLoop message_loop(MessageLoop::TYPE_DEFAULT);
  base::RunLoop run_loop;

  // Setup the pixel tester we will use to perform pixel tests on the render
  // trees output by the web module.
  renderer::RenderTreePixelTester::Options pixel_tester_options;
  pixel_tester_options.output_failed_test_details =
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kOutputFailedTestDetails);
  pixel_tester_options.output_all_test_details =
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kOutputAllTestDetails);
  renderer::RenderTreePixelTester pixel_tester(
      kTestViewportSize, GetTestInputRootDirectory(),
      GetTestOutputRootDirectory(), pixel_tester_options);

  // Setup the WebModule options.  In particular, we specify here the URL of
  // the test that we wish to run.
  browser::WebModule::Options web_module_options;
  web_module_options.layout_trigger = layout::LayoutManager::kTestRunnerMode;

  // Setup the function that should be called whenever the WebModule produces
  // a new render tree.  Essentially, we decide here if we are rebaselining or
  // actually testing, and setup the appropriate callback.
  bool result = false;
  browser::WebModule::OnRenderTreeProducedCallback callback_function(
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kRebaseline)
          ? base::Bind(&AcceptRenderTreeForRebaseline,
                       GetParam().base_file_path, &pixel_tester, &run_loop)
          : base::Bind(&AcceptRenderTreeForTest, GetParam().base_file_path,
                       &pixel_tester, &run_loop, &result));

  // Setup external modules needed by the WebModule.
  storage::StorageManager::Options storage_manager_options;
  storage::StorageManager storage_manager(storage_manager_options);
  network::NetworkModule network_module(&storage_manager);
  scoped_ptr<media::MediaModule> stub_media_module(
      new media::MediaModuleStub());

  // Create the web module.
  browser::WebModule web_module(
      GetParam().url, callback_function,
      base::Bind(&AcceptDocumentError, &run_loop), stub_media_module.get(),
      &network_module, kTestViewportSize, pixel_tester.GetResourceProvider(),
      60.0f,  // Layout refresh rate. Doesn't matter much for layout tests.
      web_module_options);

  // Start the WebModule wheels churning.  This will initiate the required loads
  // as well as eventually any JavaScript execution that needs to take place.
  // Ultimately, it should result in the passed in callback_function being
  // run after the page has been layed out and a render tree produced.
  run_loop.Run();

  // If we are not rebaselining, we are testing, so in this case, verify that
  // the result of the test is successful or not.
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kRebaseline)) {
    EXPECT_TRUE(result);
  }
}

namespace {
base::optional<TestInfo> ParseTestCaseLine(const FilePath& top_level,
                                           const std::string& line_string) {
  // Split the line up by commas, of which there may be none.
  std::vector<std::string> test_case_tokens;
  Tokenize(line_string, ",", &test_case_tokens);
  if (test_case_tokens.empty() || test_case_tokens.size() > 2) {
    return base::nullopt;
  }

  // Extract the test case file path as the first element before a comma, if
  // there is one.
  std::string base_file_path_string;
  TrimWhitespaceASCII(test_case_tokens[0], TRIM_ALL, &base_file_path_string);
  if (base_file_path_string.empty()) {
    return base::nullopt;
  }
  FilePath base_file_path(top_level.Append(base_file_path_string));

  if (test_case_tokens.size() == 1) {
    // If there is no comma, determine the URL from the file path.
    return TestInfo(base_file_path, GetURLFromBaseFilePath(base_file_path));
  } else {
    // If there is a comma, the string that comes after it contains the
    // explicitly specified URL.  Extract that and use it as the test URL.
    DCHECK_EQ(2, test_case_tokens.size());
    std::string url_string;
    TrimWhitespaceASCII(test_case_tokens[1], TRIM_ALL, &url_string);
    if (url_string.empty()) {
      return base::nullopt;
    }
    return TestInfo(base_file_path, GURL(url_string));
  }
}

// Load the file "layout_tests.txt" within the top-level directory and parse it.
// The file should contain a list of file paths (one per line) relative to the
// top level directory which the file lives.  These paths are returned as a
// vector of file paths relative to the input directory.
// The top_level parameter allows one to make different test case
// instantiations for different types of layout tests.  For example there may
// be an instantiation for Cobalt-specific layout tests, and another for
// CSS test suite tests.
std::vector<TestInfo> EnumerateLayoutTests(const std::string& top_level) {
  FilePath test_dir(GetTestInputRootDirectory().Append(top_level));
  FilePath layout_tests_list_file(
      test_dir.Append(FILE_PATH_LITERAL("layout_tests.txt")));

  std::string layout_test_list_string;
  if (!file_util::ReadFileToString(layout_tests_list_file,
                                   &layout_test_list_string)) {
    DLOG(ERROR) << "Could not open '" << layout_tests_list_file.value() << "'.";
    return std::vector<TestInfo>();
  } else {
    // Tokenize the file contents into lines, and then read each line one by
    // one as the name of the test file.
    std::vector<std::string> line_tokens;
    Tokenize(layout_test_list_string, "\n\r", &line_tokens);

    std::vector<TestInfo> test_info_list;
    for (std::vector<std::string>::const_iterator iter = line_tokens.begin();
         iter != line_tokens.end(); ++iter) {
      base::optional<TestInfo> parsed_test_info =
          ParseTestCaseLine(FilePath(top_level), *iter);
      if (!parsed_test_info) {
        DLOG(WARNING) << "Ignoring invalid test case line: " << iter->c_str();
        continue;
      } else {
        test_info_list.push_back(*parsed_test_info);
      }
    }
    return test_info_list;
  }
}
}  // namespace

// Cobalt-specific test cases.
INSTANTIATE_TEST_CASE_P(CobaltSpecificLayoutTests, LayoutTest,
                        ::testing::ValuesIn(EnumerateLayoutTests("cobalt")));
// Custom CSS 2.1 (http://www.w3.org/TR/CSS21/) test cases.
INSTANTIATE_TEST_CASE_P(CSS21LayoutTests, LayoutTest,
                        ::testing::ValuesIn(EnumerateLayoutTests("css-2-1")));
// Custom CSS Text 3 (http://www.w3.org/TR/css3-background/) test cases.
INSTANTIATE_TEST_CASE_P(
    CSSBackground3LayoutTests, LayoutTest,
    ::testing::ValuesIn(EnumerateLayoutTests("css3-background")));
// Custom CSS Text 3 (http://www.w3.org/TR/css3-color/) test cases.
INSTANTIATE_TEST_CASE_P(
    CSSColor3LayoutTests, LayoutTest,
    ::testing::ValuesIn(EnumerateLayoutTests("css3-color")));
// Custom CSS Text 3 (http://www.w3.org/TR/css-text-3/) test cases.
INSTANTIATE_TEST_CASE_P(
    CSSText3LayoutTests, LayoutTest,
    ::testing::ValuesIn(EnumerateLayoutTests("css-text-3")));
// Custom CSS Transform (http://http://www.w3.org/TR/css-transforms/)
// test cases.
INSTANTIATE_TEST_CASE_P(
    CSSTransformsLayoutTests, LayoutTest,
    ::testing::ValuesIn(EnumerateLayoutTests("css-transforms")));
// Custom bidi text (http://www.unicode.org/reports/tr9/)
// (http://www.w3.org/TR/CSS21/visuren.html#direction) test cases.
INSTANTIATE_TEST_CASE_P(
    BidiLayoutTests, LayoutTest,
    ::testing::ValuesIn(EnumerateLayoutTests("bidi")));
// Custom CSS Conditional 3 (http://www.w3.org/TR/css3-conditional/) test cases.
INSTANTIATE_TEST_CASE_P(
    CSSConditional3LayoutTests, LayoutTest,
    ::testing::ValuesIn(EnumerateLayoutTests("css3-conditional")));

// JavaScript HTML5 WebAPIs (http://www.w3.org/TR/html5/webappapis.html) test
// cases.
INSTANTIATE_TEST_CASE_P(
    WebAppAPIsLayoutTests, LayoutTest,
    ::testing::ValuesIn(EnumerateLayoutTests("webappapis")));

// JavaScript HTML5 APIs that describe requestAnimationFrame().
//   http://www.w3.org/TR/animation-timing/
INSTANTIATE_TEST_CASE_P(
    AnimationTimingAPILayoutTests, LayoutTest,
    ::testing::ValuesIn(EnumerateLayoutTests("animation-timing")));

}  // namespace layout_tests
}  // namespace cobalt
