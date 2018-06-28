// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_NPLB_BLITTER_PIXEL_TESTS_FIXTURE_H_
#define STARBOARD_NPLB_BLITTER_PIXEL_TESTS_FIXTURE_H_

#include "starboard/blitter.h"

#include "testing/gtest/include/gtest/gtest.h"

#if SB_HAS(BLITTER)

namespace starboard {
namespace nplb {
namespace blitter_pixel_tests {

// Starboard Blitter API pixel test fixture.
// As a convenience, this test fixture class will automatically construct a set
// of useful objects including a SbBlitterDevice named |device_|,
// SbBlitterContext named |context_|, and the test render target surface and its
// corresponding render target, |surface_| and |render_target_|on the results of
// respectively.  |render_target_| will automatically be set to the current
// render target on |context_|.  The test fixture is setup to perform the actual
// test on the SbBlitterSurface object in its destructor, after automatically
// flushing |context_|.  It does the test against an expected results PNG file
// named after the test and located in starboard/nplb/data/blitter_pixel_tests.
// Thus, in order to write a test, one needs only to issue draw commands.
class SbBlitterPixelTest : public testing::Test {
 public:
  SbBlitterPixelTest();
  ~SbBlitterPixelTest();

  // Returns the width of the render target surface.
  int GetWidth() const;
  // Returns the height of the render target surface.
  int GetHeight() const;

 protected:
  // Convenience objects setup by the fixture and accessible by the tests.
  SbBlitterDevice device_;
  // The surface that will ultimately be pixel tested against.
  SbBlitterSurface surface_;
  // The render target corresponding to |surface_|.  Tests should render their
  // results to this |render_target_|.
  SbBlitterRenderTarget render_target_;
  // A |context_| conveniently setup by the fixture.  It will automatically be
  // flushed in the test's destructor before the test results are evaluated.
  SbBlitterContext context_;

 private:
  // The function responsible for performing the pixel tests, called from
  // the destructor.
  void CheckSurfacePixels();
};

}  // namespace blitter_pixel_tests
}  // namespace nplb
}  // namespace starboard

#endif  // SB_HAS(BLITTER)

#endif  // STARBOARD_NPLB_BLITTER_PIXEL_TESTS_FIXTURE_H_
