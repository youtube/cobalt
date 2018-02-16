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

#include "cobalt/renderer/rasterizer/pixel_test_fixture.h"

#include <string>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "cobalt/base/cobalt_paths.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {

namespace switches {
// Command line switches.

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

namespace {
// Specifies the size of all test render targets, in pixels.
const int kTestSurfaceWidth = 200;
const int kTestSurfaceHeight = 200;

// Returns the directory that all test output files should be placed in (e.g.
// when kRebaseline, kOutputFailedTestDetails, or
// kOutputAllTestDetails switches are set).
FilePath GetTestOutputDirectory() {
  FilePath out_file_dir;
  PathService::Get(cobalt::paths::DIR_COBALT_TEST_OUT, &out_file_dir);
  out_file_dir = out_file_dir.Append(FILE_PATH_LITERAL("cobalt"))
                     .Append(FILE_PATH_LITERAL("renderer"))
                     .Append(FILE_PATH_LITERAL("rasterizer"))
                     .Append(FILE_PATH_LITERAL("testdata"));

  return out_file_dir;
}

// Returns the directory that all input expected results test data files should
// be found in. Files should be placed in this location by the build process.
FilePath GetTestInputDirectory() {
  FilePath in_file_dir;
  PathService::Get(base::DIR_TEST_DATA, &in_file_dir);
  return in_file_dir.Append(FILE_PATH_LITERAL("cobalt"))
      .Append(FILE_PATH_LITERAL("renderer"))
      .Append(FILE_PATH_LITERAL("rasterizer"))
      .Append(FILE_PATH_LITERAL("testdata"));
}

}  // namespace

PixelTest::PixelTest() {
  // Create a render tree pixel tester object based on options specified
  // by command-line parameters and compile time constants defined in this
  // file.
  RenderTreePixelTester::Options pixel_tester_options;
  pixel_tester_options.output_failed_test_details =
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kOutputFailedTestDetails);
  pixel_tester_options.output_all_test_details =
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kOutputAllTestDetails);

  pixel_tester_.emplace(math::Size(kTestSurfaceWidth, kTestSurfaceHeight),
                        GetTestInputDirectory(), GetTestOutputDirectory(),
                        pixel_tester_options);

  output_surface_size_ = math::Size(kTestSurfaceWidth, kTestSurfaceHeight);
}

PixelTest::~PixelTest() {}

void PixelTest::TestTree(
    const scoped_refptr<cobalt::render_tree::Node>& test_tree) {
  // First extract the current test's name into a std::string so that we
  // can use it to load and save image files associated with this test.
  std::string current_test_name(
      ::testing::UnitTest::GetInstance()->current_test_info()->name());

  bool results =
      pixel_tester_->TestTree(test_tree, FilePath(current_test_name));
  EXPECT_TRUE(results);

  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kRebaseline) ||
      (!results &&
       CommandLine::ForCurrentProcess()->HasSwitch(
           switches::kRebaselineFailedTests))) {
    // If the 'rebase' flag is set, we should not run any actual tests but
    // instead output the results of our tests so that they can be used as
    // expected output for subsequent tests.
    pixel_tester_->Rebaseline(test_tree, FilePath(current_test_name));
  }
}

}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
