// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "starboard/nplb/blitter_pixel_tests/fixture.h"

#include <string>

#include "starboard/blitter.h"
#include "starboard/directory.h"
#include "starboard/nplb/blitter_pixel_tests/command_line.h"
#include "starboard/nplb/blitter_pixel_tests/image.h"
#include "testing/gtest/include/gtest/gtest.h"

#if SB_HAS(BLITTER)

namespace starboard {
namespace nplb {
namespace blitter_pixel_tests {

namespace {
const int kTargetWidth = 128;
const int kTargetHeight = 128;

// The pixel tests accept a set of command line switches that can be used
// to gain more insight into the test results, as well as produce new
// expected test values.

// Runs the tests and outputs the results into the output folder with
// filenames matching those of the expected results PNG filenames.  Thus, after
// this is called, one can copy the files into their source tree to serve as
// new expected results PNG files.
const char kRebaselineSwitch[] = "--rebaseline";

// Similar to "--rebaseline", but only rebaselines tests that fail.
const char kRebaselineFailedTestsSwitch[] = "--rebaseline-failed-tests";

// For each test that fails, 3 files will be output:
//   1. The actual results produced by the test.
//   2. The expected results for the test.
//   3. A diff image indicating where the tests differ, indicated by red pixels.
const char kOutputFailedTestDetailsSwitch[] = "--output-failed-test-details";

const size_t kPathSize = SB_FILE_MAX_PATH + 1;

// Returns the directory that all output will be placed in.  There will never
// be any output unless command line switches are used.
std::string GetTestOutputDirectory() {
  char test_output_path[kPathSize];
  EXPECT_TRUE(SbSystemGetPath(kSbSystemPathTestOutputDirectory,
                              test_output_path, kPathSize));
  std::string output_dir = std::string(test_output_path);

  output_dir += SB_FILE_SEP_CHAR;
  output_dir += "starboard";
  SB_CHECK(SbDirectoryCreate(output_dir.c_str()));

  output_dir += SB_FILE_SEP_CHAR;
  output_dir += "nplb";
  SB_CHECK(SbDirectoryCreate(output_dir.c_str()));

  output_dir += SB_FILE_SEP_CHAR;
  output_dir += "blitter_pixel_tests";
  SB_CHECK(SbDirectoryCreate(output_dir.c_str()));

  output_dir += SB_FILE_SEP_CHAR;
  output_dir += "data";
  SB_CHECK(SbDirectoryCreate(output_dir.c_str()));

  return output_dir;
}

// The input directory in which all the expected results PNG test files can
// be found.
std::string GetTestInputDirectory() {
  char content_path[kPathSize];
  EXPECT_TRUE(SbSystemGetPath(kSbSystemPathContentDirectory, content_path,
                              kPathSize));
  std::string directory_path =
      std::string(content_path) + SB_FILE_SEP_CHAR + "test" +
      SB_FILE_SEP_CHAR + "starboard" + SB_FILE_SEP_CHAR + "nplb" +
      SB_FILE_SEP_CHAR + "blitter_pixel_tests" + SB_FILE_SEP_CHAR + "data";

  SB_CHECK(SbDirectoryCanOpen(directory_path.c_str()));
  return directory_path;
}

// All file paths are based off of the name of the current test.
std::string GetCurrentTestName() {
  return ::testing::UnitTest::GetInstance()->current_test_info()->name();
}

std::string GetRebaselinePath() {
  return GetTestOutputDirectory() + SB_FILE_SEP_CHAR + GetCurrentTestName() +
         "-expected.png";
}

std::string GetExpectedResultsPath() {
  return GetTestInputDirectory() + SB_FILE_SEP_CHAR + GetCurrentTestName() +
         "-expected.png";
}

std::string GetOutputDetailsActualResultsPath() {
  return GetTestOutputDirectory() + SB_FILE_SEP_CHAR + GetCurrentTestName() +
         "-actual.png";
}

std::string GetOutputDetailsExpectedResultsPath() {
  return GetTestOutputDirectory() + SB_FILE_SEP_CHAR + GetCurrentTestName() +
         "-expected.png";
}

std::string GetOutputDetailsDiffPath() {
  return GetTestOutputDirectory() + SB_FILE_SEP_CHAR + GetCurrentTestName() +
         "-diff.png";
}
}  // namespace

SbBlitterPixelTest::SbBlitterPixelTest() {
  // Setup convenience objects that will likely be used by almost every pixel
  // tests.
  device_ = SbBlitterCreateDefaultDevice();
  EXPECT_TRUE(SbBlitterIsDeviceValid(device_));

  surface_ = SbBlitterCreateRenderTargetSurface(
      device_, GetWidth(), GetHeight(), kSbBlitterSurfaceFormatRGBA8);
  if (!SbBlitterIsSurfaceValid(surface_)) {
    SB_LOG(ERROR) << "A 32-bit RGBA format must be supported by the platform "
                  << "in order to run pixel tests.";
    SB_NOTREACHED();
  }

  render_target_ = SbBlitterGetRenderTargetFromSurface(surface_);
  EXPECT_TRUE(SbBlitterIsRenderTargetValid(render_target_));

  context_ = SbBlitterCreateContext(device_);
  EXPECT_TRUE(SbBlitterIsContextValid(context_));

  // Initialize |context_|'s render target to |render_target_|.
  EXPECT_TRUE(SbBlitterSetRenderTarget(context_, render_target_));

  // Initialize the surface by filling it to be completely transparent.
  EXPECT_TRUE(SbBlitterSetBlending(context_, false));
  EXPECT_TRUE(SbBlitterSetColor(context_, SbBlitterColorFromRGBA(0, 0, 0, 0)));
  EXPECT_TRUE(SbBlitterFillRect(
      context_, SbBlitterMakeRect(0, 0, GetWidth(), GetHeight())));
}

SbBlitterPixelTest::~SbBlitterPixelTest() {
  // Upon destruction, perform the pixel test on surface_.
  CheckSurfacePixels();

  EXPECT_TRUE(SbBlitterDestroyContext(context_));
  EXPECT_TRUE(SbBlitterDestroySurface(surface_));
  EXPECT_TRUE(SbBlitterDestroyDevice(device_));
}

int SbBlitterPixelTest::GetWidth() const {
  return kTargetWidth;
}

int SbBlitterPixelTest::GetHeight() const {
  return kTargetHeight;
}

void SbBlitterPixelTest::CheckSurfacePixels() {
  // Here is where the actual pixel testing logic is.  We start by flushing
  // the context so that all draw commands are guaranteed to be submitted.
  SbBlitterFlushContext(context_);

  // Start by converting the resulting SbBlitterSurface into a CPU-accessible
  // Image object.
  Image actual_image(surface_);

  if (CommandLineContains(kRebaselineSwitch)) {
    // If we're rebaselining, we don't actually need to perform any tests, we
    // just write out the resulting file.
    actual_image.WriteToPNG(GetRebaselinePath());
  }

  // Flag indicating whether the actual image matches with the expected image.
  // This is checked at the end of this function and determines whether the
  // test passes or not.
  bool is_match = false;

  // Load the expected results image.
  if (Image::CanOpenFile(GetExpectedResultsPath())) {
    Image expected_image(GetExpectedResultsPath());

    // Adjusting the following values affect how much of a fudge factor the
    // tests are willing to accept before failing.  The |kBlurSigma| parameter
    // can be modified to affect how much the expected/actual images are blurred
    // before being tested, which is good for reducing the severity of
    // 1-pixel-off differences.  The |kPixelTestValueFuzz| parameter can be
    // adjusted to set how much each pixel color must differ by before being
    // considered a difference.
    const float kBlurSigma = 0.0f;
    const int kPixelTestValueFuzz = 0;

    // Blur the actual and expected images before comparing them so that we
    // can be lenient on one-pixel-off differences.
    Image actual_blurred_image = actual_image.GaussianBlur(kBlurSigma);
    Image expected_blurred_image = expected_image.GaussianBlur(kBlurSigma);

    // Perform the image diff on the blurred images.
    Image diff_image = actual_blurred_image.Diff(
        expected_blurred_image, kPixelTestValueFuzz, &is_match);

    // If the images do not match, depending on what command line options were
    // specified, we may write out resulting image PNG files.
    if (!is_match) {
      if (!CommandLineContains(kRebaselineSwitch) &&
          !CommandLineContains(kRebaselineFailedTestsSwitch) &&
          CommandLineContains(kOutputFailedTestDetailsSwitch)) {
        actual_image.WriteToPNG(GetOutputDetailsActualResultsPath());
        expected_image.WriteToPNG(GetOutputDetailsExpectedResultsPath());
        diff_image.WriteToPNG(GetOutputDetailsDiffPath());
      }
    }
  }

  if (!is_match && !CommandLineContains(kRebaselineSwitch) &&
      CommandLineContains(kRebaselineFailedTestsSwitch)) {
    SB_LOG(INFO) << "Rebasing to " << GetRebaselinePath();
    actual_image.WriteToPNG(GetRebaselinePath());
  }

  EXPECT_TRUE(is_match);
}

}  // namespace blitter_pixel_tests
}  // namespace nplb
}  // namespace starboard

#endif  // SB_HAS(BLITTER)
