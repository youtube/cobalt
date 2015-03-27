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

#include <vector>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "cobalt/math/size.h"
#include "cobalt/base/cobalt_paths.h"
#include "cobalt/browser/web_module.h"
#include "cobalt/renderer/render_tree_pixel_tester.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace layout_tests {

namespace switches {
// If enabled, running the tests will result in the generation of expected
// test output from the actual output to the output directory.
const char* kRebaseline = "rebaseline";

// If enabled, will output details (in the form of files placed in the output
// directory) for all tests that fail.
const char* kOutputFailedTestDetails = "output-failed-test-details";

// Like kOutputFailedTestDetails, but outputs details for tests that
// succeed as well.
const char* kOutputAllTestDetails = "output-all-test-details";
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
void AcceptRenderTreeForTest(const FilePath& test_html_path,
                             renderer::RenderTreePixelTester* pixel_tester,
                             base::RunLoop* run_loop, bool* result,
                             const scoped_refptr<render_tree::Node>& tree) {
  *result = pixel_tester->TestTree(tree, test_html_path.RemoveExtension());
  MessageLoop::current()->PostTask(FROM_HERE, run_loop->QuitClosure());
}

// Handles an incoming render tree by rendering it to an expected output image
// file in the output directory.
void AcceptRenderTreeForRebaseline(
    const FilePath& test_html_path,
    renderer::RenderTreePixelTester* pixel_tester, base::RunLoop* run_loop,
    const scoped_refptr<render_tree::Node>& tree) {
  pixel_tester->Rebaseline(tree, test_html_path.RemoveExtension());
  MessageLoop::current()->PostTask(FROM_HERE, run_loop->QuitClosure());
}
}  // namespace

class LayoutTest : public ::testing::TestWithParam<FilePath> {};
TEST_P(LayoutTest, LayoutTest) {
  // Output the name of the current input file so that it is visible in test
  // output.
  std::cout << "(" << GetParam().value() << ")" << std::endl;

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

  // Setup a message loop for the current thread since we will be constructing
  // a WebModule, which requires a message loop to exist for the current
  // thread.
  MessageLoop message_loop(MessageLoop::TYPE_DEFAULT);
  base::RunLoop run_loop;

  // Setup the WebModule options.  In particular, we specify here the URL of
  // the test that we wish to run.
  browser::WebModule::Options web_module_options;
  web_module_options.url =
      GURL("file:///" + GetDirSourceRootRelativePath().value() + "/" +
           GetParam().value());
  web_module_options.layout_trigger = layout::LayoutManager::kOnDocumentLoad;

  // Setup the function that should be called whenever the WebModule produces
  // a new render tree.  Essentially, we decide here if we are rebaselining or
  // actually testing, and setup the appropriate callback.
  bool result = false;
  layout::LayoutManager::OnRenderTreeProducedCallback callback_function(
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kRebaseline)
          ? base::Bind(&AcceptRenderTreeForRebaseline, GetParam(),
                       &pixel_tester, &run_loop)
          : base::Bind(&AcceptRenderTreeForTest, GetParam(), &pixel_tester,
                       &run_loop, &result));

  // Create the web module.
  browser::WebModule web_module(
      callback_function, kTestViewportSize,
      pixel_tester.GetResourceProvider(), web_module_options);

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
// Enumerate all files in the top level directory, counting each ".html"
// file found as a separate test, and returning it in the vector of layout_tests
// to execute.  The top_level parameter allows one to make different test case
// instantiations for different types of layout tests.  For example there may
// be an instantiation for Cobalt-specific layout tests, and another for
// CSS test suite tests.
std::vector<FilePath> EnumerateLayoutTests(const std::string& top_level) {
  FilePath test_dir(GetTestInputRootDirectory().Append(top_level));

  std::vector<FilePath> tests;
  file_util::FileEnumerator files(test_dir, true,
                                  file_util::FileEnumerator::FILES);
  for (FilePath next_file = files.Next(); !next_file.empty();
       next_file = files.Next()) {
    if (next_file.Extension() == ".html") {
      FilePath path_relative_to_input_directory;
      CHECK(GetTestInputRootDirectory().AppendRelativePath(
          next_file, &path_relative_to_input_directory));
      tests.push_back(path_relative_to_input_directory);
    }
  }

  return tests;
}
}  // namespace

// Instantiate the set of Cobalt-specific test cases.
INSTANTIATE_TEST_CASE_P(CobaltSpecificLayoutTests, LayoutTest,
                        ::testing::ValuesIn(EnumerateLayoutTests("cobalt")));

}  // namespace layout_tests
}  // namespace cobalt
