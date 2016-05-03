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

// All pixel tests here are heavily facilitated by logic in the test fixture,
// |SbBlitterPixelTest| located in blitter_pixel_test_fixture.h.  The fixture
// will setup an environment containing:
//
//   SbBlitterDevice device_;
//   SbBlitterContext context_;
//   SbBlitterSurface surface_;
//   SbBlitterRenderTarget render_target_;
//
// All rendering calls should be made to |render_target_|, which is the render
// target associated with |surface_|.  When the test finishes, the
// |SbBlitterPixelTest| fixture object is destructed at which point |context_|
// will be flushed and the image data downloaded from |surface_| and analyzed
// against an existing expected results PNG image file named after the test.
// See blitter_pixel_test_fixture.cc for a list of command line options that
// can be used to rebaseline the expected results of these tests.  Command line
// options can also be used to have the pixel tests output test failure details
// including image diffs between the actual results and expected results.

#include "starboard/blitter.h"
#include "starboard/nplb/blitter_pixel_tests/fixture.h"
#include "testing/gtest/include/gtest/gtest.h"

#if SB_HAS(BLITTER)

namespace starboard {
namespace nplb {
namespace blitter_pixel_tests {
namespace {

TEST_F(SbBlitterPixelTest, SolidFillRed) {
  SbBlitterSetColor(context_, SbBlitterColorFromRGBA(255, 0, 0, 255));
  SbBlitterFillRect(context_, SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()));
}

TEST_F(SbBlitterPixelTest, SolidFillGreen) {
  SbBlitterSetColor(context_, SbBlitterColorFromRGBA(0, 255, 0, 255));
  SbBlitterFillRect(context_, SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()));
}

TEST_F(SbBlitterPixelTest, SolidFillBlue) {
  SbBlitterSetColor(context_, SbBlitterColorFromRGBA(0, 0, 255, 255));
  SbBlitterFillRect(context_, SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()));
}

TEST_F(SbBlitterPixelTest, SolidFillRedBoxOnWhiteBackground) {
  SbBlitterSetColor(context_, SbBlitterColorFromRGBA(255, 255, 255, 255));
  SbBlitterFillRect(context_, SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()));
  SbBlitterSetColor(context_, SbBlitterColorFromRGBA(255, 0, 0, 255));
  SbBlitterFillRect(context_,
                    SbBlitterMakeRect(GetWidth() / 4, GetHeight() / 4,
                                      GetWidth() / 2, GetHeight() / 2));
}

SbBlitterSurface CreateCheckerImage(SbBlitterDevice device,
                                    SbBlitterContext context,
                                    SbBlitterColor color_a,
                                    SbBlitterColor color_b,
                                    int width,
                                    int height) {
  SbBlitterSurface surface = SbBlitterCreateRenderTargetSurface(
      device, width, height, kSbBlitterSurfaceFormatRGBA8);

  SbBlitterRenderTarget render_target =
      SbBlitterGetRenderTargetFromSurface(surface);
  SbBlitterSetRenderTarget(context, render_target);

  SbBlitterSetColor(context, color_a);
  SbBlitterFillRect(context, SbBlitterMakeRect(0, 0, width, height));

  SbBlitterSetColor(context, color_b);
  SbBlitterFillRect(context, SbBlitterMakeRect(0, 0, width / 2, height / 2));
  SbBlitterFillRect(
      context, SbBlitterMakeRect(width / 2, height / 2, width / 2, height / 2));

  return surface;
}

TEST_F(SbBlitterPixelTest, SimpleNonStretchBlitRectToRect) {
  SbBlitterSurface checker_image = CreateCheckerImage(
      device_, context_, SbBlitterColorFromRGBA(255, 255, 255, 255),
      SbBlitterColorFromRGBA(0, 0, 0, 255), GetWidth(), GetHeight());

  SbBlitterSetRenderTarget(context_, render_target_);
  SbBlitterBlitRectToRect(context_, checker_image,
                          SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()),
                          SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()));
}

TEST_F(SbBlitterPixelTest, MagnifyBlitRectToRect) {
  // Create an image with a height and width of 2x2.
  SbBlitterSurface checker_image = CreateCheckerImage(
      device_, context_, SbBlitterColorFromRGBA(255, 255, 255, 255),
      SbBlitterColorFromRGBA(0, 0, 0, 255), 2, 2);

  // Now stretch it onto the entire target surface.
  SbBlitterSetRenderTarget(context_, render_target_);
  SbBlitterBlitRectToRect(context_, checker_image,
                          SbBlitterMakeRect(0, 0, 2, 2),
                          SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()));
}

TEST_F(SbBlitterPixelTest, MinifyBlitRectToRect) {
  SbBlitterSurface checker_image = CreateCheckerImage(
      device_, context_, SbBlitterColorFromRGBA(255, 255, 255, 255),
      SbBlitterColorFromRGBA(0, 0, 0, 255), GetWidth(), GetHeight());

  SbBlitterSetRenderTarget(context_, render_target_);

  SbBlitterBlitRectToRect(
      context_, checker_image, SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()),
      SbBlitterMakeRect(0, 0, GetWidth() / 8, GetHeight() / 8));
}

TEST_F(SbBlitterPixelTest, BlitRectToRectPartialSourceRect) {
  SbBlitterSurface checker_image = CreateCheckerImage(
      device_, context_, SbBlitterColorFromRGBA(255, 255, 255, 255),
      SbBlitterColorFromRGBA(0, 0, 0, 255), GetWidth(), GetHeight());

  SbBlitterSetRenderTarget(context_, render_target_);

  SbBlitterBlitRectToRect(
      context_, checker_image,
      SbBlitterMakeRect(GetWidth() / 2, 0, GetWidth() / 2, GetHeight()),
      SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()));
}

TEST_F(SbBlitterPixelTest, BlitRectToRectTiled) {
  SbBlitterSurface checker_image = CreateCheckerImage(
      device_, context_, SbBlitterColorFromRGBA(255, 255, 255, 255),
      SbBlitterColorFromRGBA(0, 0, 0, 255), 8, 8);

  SbBlitterSetRenderTarget(context_, render_target_);

  SbBlitterBlitRectToRectTiled(
      context_, checker_image, SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()),
      SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()));
}

TEST_F(SbBlitterPixelTest, BlitRectToRectTiledWithNoTiling) {
  SbBlitterSurface checker_image = CreateCheckerImage(
      device_, context_, SbBlitterColorFromRGBA(255, 255, 255, 255),
      SbBlitterColorFromRGBA(0, 0, 0, 255), GetWidth(), GetHeight());

  SbBlitterSetRenderTarget(context_, render_target_);

  SbBlitterBlitRectToRectTiled(
      context_, checker_image, SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()),
      SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()));
}

TEST_F(SbBlitterPixelTest, BlitRectToRectTiledNegativeOffset) {
  SbBlitterSurface checker_image = CreateCheckerImage(
      device_, context_, SbBlitterColorFromRGBA(255, 255, 255, 255),
      SbBlitterColorFromRGBA(0, 0, 0, 255), 8, 8);

  SbBlitterSetRenderTarget(context_, render_target_);

  SbBlitterBlitRectToRectTiled(
      context_, checker_image,
      SbBlitterMakeRect(-GetWidth(), -GetHeight(), GetWidth(), GetHeight()),
      SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()));
}

TEST_F(SbBlitterPixelTest, BlitRectToRectTiledOffCenter) {
  SbBlitterSurface checker_image = CreateCheckerImage(
      device_, context_, SbBlitterColorFromRGBA(255, 255, 255, 255),
      SbBlitterColorFromRGBA(0, 0, 0, 255), GetWidth() / 2, GetHeight() / 2);

  SbBlitterSetRenderTarget(context_, render_target_);

  SbBlitterBlitRectToRectTiled(
      context_, checker_image,
      SbBlitterMakeRect(-GetWidth() / 2, -GetHeight() / 2, GetWidth(),
                        GetHeight()),
      SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()));
}

TEST_F(SbBlitterPixelTest, BlitRectsToRects) {
  SbBlitterSurface checker_image = CreateCheckerImage(
      device_, context_, SbBlitterColorFromRGBA(255, 0, 0, 255),
      SbBlitterColorFromRGBA(0, 0, 255, 255), GetWidth(), GetHeight());

  SbBlitterSetRenderTarget(context_, render_target_);

  const int kNumRects = 8;

  SbBlitterRect src_rects[kNumRects];
  SbBlitterRect dst_rects[kNumRects];

  for (int i = 0; i < kNumRects; ++i) {
    src_rects[i].x = i * (GetWidth() / kNumRects);
    src_rects[i].y = 0;
    src_rects[i].width = (GetWidth() / kNumRects) / 2;
    src_rects[i].height = (i + 1) * (GetHeight() / kNumRects);

    dst_rects[i].x = i * (GetWidth() / kNumRects);
    dst_rects[i].y = 0;
    dst_rects[i].width = (GetWidth() / kNumRects) / 2;
    dst_rects[i].height = (i + 1) * (GetHeight() / kNumRects);
  }
  SbBlitterBlitRectsToRects(context_, checker_image, src_rects, dst_rects,
                            kNumRects);
}

}  // namespace
}  // namespace blitter_pixel_tests
}  // namespace nplb
}  // namespace starboard

#endif  // SB_HAS(BLITTER)
