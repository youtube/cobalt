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
#include "starboard/configuration_constants.h"
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

// Returns half of a dimension, though it will never round it down to 0.
int HalveDimension(int d) {
  SB_DCHECK(d > 0);
  return std::max(1, d / 2);
}

SbBlitterSurface CreateCheckerImageWithBlits(SbBlitterDevice device,
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

  int half_width = HalveDimension(width);
  int half_height = HalveDimension(height);

  SbBlitterSetColor(context, color_b);
  SbBlitterFillRect(context, SbBlitterMakeRect(0, 0, half_width, half_height));
  SbBlitterFillRect(
      context, SbBlitterMakeRect(half_width, half_height, width - half_width,
                                 height - half_height));

  return surface;
}

TEST_F(SbBlitterPixelTest, SimpleNonStretchBlitRectToRect) {
  SbBlitterSurface checker_image = CreateCheckerImageWithBlits(
      device_, context_, SbBlitterColorFromRGBA(255, 255, 255, 255),
      SbBlitterColorFromRGBA(0, 0, 0, 255), GetWidth(), GetHeight());

  SbBlitterSetRenderTarget(context_, render_target_);
  SbBlitterBlitRectToRect(context_, checker_image,
                          SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()),
                          SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()));

  SbBlitterDestroySurface(checker_image);
}

#if SB_HAS(BILINEAR_FILTERING_SUPPORT)
TEST_F(SbBlitterPixelTest, MagnifyBlitRectToRectInterpolated) {
#else
TEST_F(SbBlitterPixelTest, MagnifyBlitRectToRectNotInterpolated) {
#endif
  // Create an image with a height and width of 2x2.
  SbBlitterSurface checker_image = CreateCheckerImageWithBlits(
      device_, context_, SbBlitterColorFromRGBA(255, 255, 255, 255),
      SbBlitterColorFromRGBA(0, 0, 0, 255), 2, 2);

  // Now stretch it onto the entire target surface.
  SbBlitterSetRenderTarget(context_, render_target_);
  SbBlitterBlitRectToRect(context_, checker_image,
                          SbBlitterMakeRect(0, 0, 2, 2),
                          SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()));

  SbBlitterDestroySurface(checker_image);
}

TEST_F(SbBlitterPixelTest, MinifyBlitRectToRect) {
  SbBlitterSurface checker_image = CreateCheckerImageWithBlits(
      device_, context_, SbBlitterColorFromRGBA(255, 255, 255, 255),
      SbBlitterColorFromRGBA(0, 0, 0, 255), GetWidth(), GetHeight());

  SbBlitterSetRenderTarget(context_, render_target_);

  SbBlitterBlitRectToRect(
      context_, checker_image, SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()),
      SbBlitterMakeRect(0, 0, GetWidth() / 8, GetHeight() / 8));

  SbBlitterDestroySurface(checker_image);
}

TEST_F(SbBlitterPixelTest, BlitRectToRectPartialSourceRect) {
  SbBlitterSurface checker_image = CreateCheckerImageWithBlits(
      device_, context_, SbBlitterColorFromRGBA(255, 255, 255, 255),
      SbBlitterColorFromRGBA(0, 0, 0, 255), GetWidth(), GetHeight());

  SbBlitterSetRenderTarget(context_, render_target_);

  SbBlitterBlitRectToRect(
      context_, checker_image,
      SbBlitterMakeRect(GetWidth() / 2, 0, GetWidth() / 2, GetHeight()),
      SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()));

  SbBlitterDestroySurface(checker_image);
}

TEST_F(SbBlitterPixelTest, BlitRectToRectTiled) {
  SbBlitterSurface checker_image = CreateCheckerImageWithBlits(
      device_, context_, SbBlitterColorFromRGBA(255, 255, 255, 255),
      SbBlitterColorFromRGBA(0, 0, 0, 255), 8, 8);

  SbBlitterSetRenderTarget(context_, render_target_);

  SbBlitterBlitRectToRectTiled(
      context_, checker_image, SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()),
      SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()));

  SbBlitterDestroySurface(checker_image);
}

TEST_F(SbBlitterPixelTest, BlitRectToRectTiledWithNoTiling) {
  SbBlitterSurface checker_image = CreateCheckerImageWithBlits(
      device_, context_, SbBlitterColorFromRGBA(255, 255, 255, 255),
      SbBlitterColorFromRGBA(0, 0, 0, 255), GetWidth(), GetHeight());

  SbBlitterSetRenderTarget(context_, render_target_);

  SbBlitterBlitRectToRectTiled(
      context_, checker_image, SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()),
      SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()));

  SbBlitterDestroySurface(checker_image);
}

TEST_F(SbBlitterPixelTest, BlitRectToRectTiledNegativeOffset) {
  SbBlitterSurface checker_image = CreateCheckerImageWithBlits(
      device_, context_, SbBlitterColorFromRGBA(255, 255, 255, 255),
      SbBlitterColorFromRGBA(0, 0, 0, 255), 8, 8);

  SbBlitterSetRenderTarget(context_, render_target_);

  SbBlitterBlitRectToRectTiled(
      context_, checker_image,
      SbBlitterMakeRect(-GetWidth(), -GetHeight(), GetWidth(), GetHeight()),
      SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()));

  SbBlitterDestroySurface(checker_image);
}

TEST_F(SbBlitterPixelTest, BlitRectToRectTiledOffCenter) {
  SbBlitterSurface checker_image = CreateCheckerImageWithBlits(
      device_, context_, SbBlitterColorFromRGBA(255, 255, 255, 255),
      SbBlitterColorFromRGBA(0, 0, 0, 255), GetWidth() / 2, GetHeight() / 2);

  SbBlitterSetRenderTarget(context_, render_target_);

  SbBlitterBlitRectToRectTiled(
      context_, checker_image,
      SbBlitterMakeRect(-GetWidth() / 2, -GetHeight() / 2, GetWidth(),
                        GetHeight()),
      SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()));

  SbBlitterDestroySurface(checker_image);
}

void DrawRectsWithBatchBlit(SbBlitterSurface texture,
                            SbBlitterContext context,
                            int width,
                            int height) {
  const int kNumRects = 8;

  SbBlitterRect src_rects[kNumRects];
  SbBlitterRect dst_rects[kNumRects];

  for (int i = 0; i < kNumRects; ++i) {
    src_rects[i].x = i * (width / kNumRects);
    src_rects[i].y = 0;
    src_rects[i].width = (width / kNumRects) / 2;
    src_rects[i].height = (i + 1) * (height / kNumRects);

    dst_rects[i].x = i * (width / kNumRects);
    dst_rects[i].y = 0;
    dst_rects[i].width = (width / kNumRects) / 2;
    dst_rects[i].height = (i + 1) * (height / kNumRects);
  }
  SbBlitterBlitRectsToRects(context, texture, src_rects, dst_rects, kNumRects);
}

TEST_F(SbBlitterPixelTest, BlitRectsToRects) {
  SbBlitterSurface checker_image = CreateCheckerImageWithBlits(
      device_, context_, SbBlitterColorFromRGBA(255, 0, 0, 255),
      SbBlitterColorFromRGBA(0, 0, 255, 255), GetWidth(), GetHeight());

  SbBlitterSetRenderTarget(context_, render_target_);

  DrawRectsWithBatchBlit(checker_image, context_, GetWidth(), GetHeight());

  SbBlitterDestroySurface(checker_image);
}

SbBlitterSurface CreateCheckerImageWithPixelData(SbBlitterDevice device,
                                                 SbBlitterColor color_a,
                                                 SbBlitterColor color_b,
                                                 int width,
                                                 int height) {
  SbBlitterPixelDataFormat pixel_data_format;

  if (kSbPreferredRgbaByteOrder == SB_PREFERRED_RGBA_BYTE_ORDER_RGBA) {
    pixel_data_format = kSbBlitterPixelDataFormatRGBA8;
  } else if (kSbPreferredRgbaByteOrder == SB_PREFERRED_RGBA_BYTE_ORDER_BGRA) {
    pixel_data_format = kSbBlitterPixelDataFormatBGRA8;
  } else if (kSbPreferredRgbaByteOrder == SB_PREFERRED_RGBA_BYTE_ORDER_ARGB) {
    pixel_data_format = kSbBlitterPixelDataFormatARGB8;
  } else {
    SB_CHECK(false)
        << "Platform's preferred RGBA byte order is not yet supported.";
  }

  // RGBA byte-offsets into each pixel.
  int r, g, b, a;
  switch (pixel_data_format) {
    case kSbBlitterPixelDataFormatRGBA8: {
      r = 0;
      g = 1;
      b = 2;
      a = 3;
    } break;
    case kSbBlitterPixelDataFormatBGRA8: {
      b = 0;
      g = 1;
      r = 2;
      a = 3;
    } break;
    case kSbBlitterPixelDataFormatARGB8: {
      a = 0;
      r = 1;
      g = 2;
      b = 3;
    } break;
    default: { SB_NOTREACHED(); }
  }

  SbBlitterPixelData pixel_data =
      SbBlitterCreatePixelData(device, width, height, pixel_data_format);
  SB_CHECK(SbBlitterIsPixelDataValid(pixel_data));

  int half_width = HalveDimension(width);
  int half_height = HalveDimension(height);

  int pitch = SbBlitterGetPixelDataPitchInBytes(pixel_data);
  uint8_t* current_byte =
      static_cast<uint8_t*>(SbBlitterGetPixelDataPointer(pixel_data));
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      bool use_color_a = (x < half_width) ^ (y < half_height);
      SbBlitterColor color = use_color_a ? color_a : color_b;
      float a_float = SbBlitterAFromColor(color) / 255.0f;
      current_byte[g] = SbBlitterGFromColor(color) * a_float;
      current_byte[r] = SbBlitterRFromColor(color) * a_float;
      current_byte[b] = SbBlitterBFromColor(color) * a_float;
      current_byte[a] = SbBlitterAFromColor(color);

      current_byte += 4;
    }
    current_byte += pitch - width * 4;
  }

  SbBlitterSurface surface =
      SbBlitterCreateSurfaceFromPixelData(device, pixel_data);
  SB_CHECK(SbBlitterIsSurfaceValid(surface));

  return surface;
}

SbBlitterSurface CreateAlphaCheckerImageWithPixelData(SbBlitterDevice device,
                                                      uint8_t color_a,
                                                      uint8_t color_b,
                                                      int width,
                                                      int height) {
  SbBlitterPixelData pixel_data = SbBlitterCreatePixelData(
      device, width, height, kSbBlitterPixelDataFormatA8);
  SB_CHECK(SbBlitterIsPixelDataValid(pixel_data));

  int half_width = HalveDimension(width);
  int half_height = HalveDimension(height);

  int pitch = SbBlitterGetPixelDataPitchInBytes(pixel_data);
  uint8_t* current_byte =
      static_cast<uint8_t*>(SbBlitterGetPixelDataPointer(pixel_data));
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      bool use_color_a = (x < half_width) ^ (y < half_height);
      *current_byte = use_color_a ? color_a : color_b;
      ++current_byte;
    }
    current_byte += pitch - width;
  }

  SbBlitterSurface surface =
      SbBlitterCreateSurfaceFromPixelData(device, pixel_data);
  SB_CHECK(SbBlitterIsSurfaceValid(surface));

  return surface;
}

TEST_F(SbBlitterPixelTest, BlitRectToRectFromPixelData) {
  SbBlitterSurface checker_image = CreateCheckerImageWithPixelData(
      device_, SbBlitterColorFromRGBA(255, 255, 255, 255),
      SbBlitterColorFromRGBA(0, 0, 0, 255), GetWidth(), GetHeight());

  SbBlitterBlitRectToRect(context_, checker_image,
                          SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()),
                          SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()));

  SbBlitterDestroySurface(checker_image);
}

TEST_F(SbBlitterPixelTest, BlitRedGreenRectToRectFromPixelData) {
  SbBlitterSurface checker_image = CreateCheckerImageWithPixelData(
      device_, SbBlitterColorFromRGBA(255, 0, 0, 255),
      SbBlitterColorFromRGBA(0, 255, 0, 255), GetWidth(), GetHeight());

  SbBlitterBlitRectToRect(context_, checker_image,
                          SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()),
                          SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()));

  SbBlitterDestroySurface(checker_image);
}

TEST_F(SbBlitterPixelTest, BlitHalfTransparentRectToRectFromPixelData) {
  SbBlitterSurface checker_image = CreateCheckerImageWithPixelData(
      device_, SbBlitterColorFromRGBA(255, 255, 255, 255),
      SbBlitterColorFromRGBA(255, 255, 255, 128), GetWidth(), GetHeight());

  SbBlitterBlitRectToRect(context_, checker_image,
                          SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()),
                          SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()));

  SbBlitterDestroySurface(checker_image);
}

TEST_F(SbBlitterPixelTest, BlitCanPunchThrough) {
  // This test ensures that when blending is disabled, rendering with alpha
  // can punch through the red and opaque background.
  SbBlitterSurface checker_image = CreateCheckerImageWithPixelData(
      device_, SbBlitterColorFromRGBA(255, 255, 255, 255),
      SbBlitterColorFromRGBA(255, 255, 255, 128), GetWidth(), GetHeight());

  SbBlitterSetBlending(context_, false);
  SbBlitterSetColor(context_, SbBlitterColorFromRGBA(255, 0, 0, 255));
  SbBlitterFillRect(context_, SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()));
  SbBlitterBlitRectToRect(context_, checker_image,
                          SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()),
                          SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()));

  SbBlitterDestroySurface(checker_image);
}

TEST_F(SbBlitterPixelTest, BlitCanBlend) {
  // This test ensures that when blending is enabled, rendering with alpha
  // will blend with a red and opaque background.
  SbBlitterSurface checker_image = CreateCheckerImageWithPixelData(
      device_, SbBlitterColorFromRGBA(255, 255, 255, 255),
      SbBlitterColorFromRGBA(255, 255, 255, 128), GetWidth(), GetHeight());

  SbBlitterSetBlending(context_, true);
  SbBlitterSetColor(context_, SbBlitterColorFromRGBA(255, 0, 0, 255));
  SbBlitterFillRect(context_, SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()));
  SbBlitterBlitRectToRect(context_, checker_image,
                          SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()),
                          SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()));

  SbBlitterDestroySurface(checker_image);
}

TEST_F(SbBlitterPixelTest, SimpleAlphaBlitWithNoColorModulation) {
  // Tests that alpha-only surfaces blit to white with no color modulation
  // and disabled blending.
  SbBlitterSurface checker_image = CreateAlphaCheckerImageWithPixelData(
      device_, 128, 255, GetWidth(), GetHeight());

  SbBlitterSetBlending(context_, false);
  SbBlitterSetModulateBlitsWithColor(context_, false);
  SbBlitterSetColor(context_, SbBlitterColorFromRGBA(255, 0, 0, 255));
  SbBlitterBlitRectToRect(context_, checker_image,
                          SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()),
                          SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()));

  SbBlitterDestroySurface(checker_image);
}

TEST_F(SbBlitterPixelTest, BlendedAlphaBlitWithNoColorModulationOnRed) {
  SbBlitterSurface checker_image = CreateAlphaCheckerImageWithPixelData(
      device_, 128, 255, GetWidth(), GetHeight());

  SbBlitterSetBlending(context_, true);
  SbBlitterSetColor(context_, SbBlitterColorFromRGBA(255, 0, 0, 255));
  SbBlitterFillRect(context_, SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()));
  SbBlitterSetModulateBlitsWithColor(context_, false);
  SbBlitterBlitRectToRect(context_, checker_image,
                          SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()),
                          SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()));

  SbBlitterDestroySurface(checker_image);
}

TEST_F(SbBlitterPixelTest, AlphaBlitWithBlueColorModulation) {
  SbBlitterSurface checker_image = CreateAlphaCheckerImageWithPixelData(
      device_, 128, 255, GetWidth(), GetHeight());

  SbBlitterSetBlending(context_, false);
  SbBlitterSetModulateBlitsWithColor(context_, true);
  SbBlitterSetColor(context_, SbBlitterColorFromRGBA(0, 0, 255, 255));
  SbBlitterBlitRectToRect(context_, checker_image,
                          SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()),
                          SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()));

  SbBlitterDestroySurface(checker_image);
}

TEST_F(SbBlitterPixelTest, BlendedAlphaBlitWithBlueColorModulationOnRed) {
  SbBlitterSurface checker_image = CreateAlphaCheckerImageWithPixelData(
      device_, 128, 255, GetWidth(), GetHeight());

  SbBlitterSetBlending(context_, true);
  SbBlitterSetColor(context_, SbBlitterColorFromRGBA(255, 0, 0, 255));
  SbBlitterFillRect(context_, SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()));
  SbBlitterSetModulateBlitsWithColor(context_, true);
  SbBlitterSetColor(context_, SbBlitterColorFromRGBA(0, 0, 255, 255));
  SbBlitterBlitRectToRect(context_, checker_image,
                          SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()),
                          SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()));

  SbBlitterDestroySurface(checker_image);
}

TEST_F(SbBlitterPixelTest, BlendedAlphaBlitWithAlphaColorModulationOnRed) {
  SbBlitterSurface checker_image = CreateAlphaCheckerImageWithPixelData(
      device_, 0, 255, GetWidth(), GetHeight());

  SbBlitterSetBlending(context_, true);
  SbBlitterSetColor(context_, SbBlitterColorFromRGBA(255, 0, 0, 255));
  SbBlitterFillRect(context_, SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()));
  SbBlitterSetModulateBlitsWithColor(context_, true);
  SbBlitterSetColor(context_, SbBlitterColorFromRGBA(255, 255, 255, 128));
  SbBlitterBlitRectToRect(context_, checker_image,
                          SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()),
                          SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()));

  SbBlitterDestroySurface(checker_image);
}

TEST_F(SbBlitterPixelTest, BlendedColorBlitWithAlphaColorModulationOnRed) {
  SbBlitterSurface checker_image = CreateCheckerImageWithPixelData(
      device_, SbBlitterColorFromRGBA(0, 0, 0, 255),
      SbBlitterColorFromRGBA(255, 255, 255, 255), GetWidth(), GetHeight());

  SbBlitterSetBlending(context_, true);
  SbBlitterSetColor(context_, SbBlitterColorFromRGBA(255, 0, 0, 255));
  SbBlitterFillRect(context_, SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()));
  SbBlitterSetModulateBlitsWithColor(context_, true);
  SbBlitterSetColor(context_, SbBlitterColorFromRGBA(255, 255, 255, 128));
  SbBlitterBlitRectToRect(context_, checker_image,
                          SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()),
                          SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()));

  SbBlitterDestroySurface(checker_image);
}

TEST_F(SbBlitterPixelTest, BlendedColorBlitWithAlphaColorModulation) {
  SbBlitterSurface checker_image = CreateCheckerImageWithPixelData(
      device_, SbBlitterColorFromRGBA(0, 0, 0, 255),
      SbBlitterColorFromRGBA(255, 255, 255, 255), GetWidth(), GetHeight());

  SbBlitterSetBlending(context_, true);
  SbBlitterSetModulateBlitsWithColor(context_, true);
  SbBlitterSetColor(context_, SbBlitterColorFromRGBA(0, 0, 255, 128));
  SbBlitterBlitRectToRect(context_, checker_image,
                          SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()),
                          SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()));

  SbBlitterDestroySurface(checker_image);
}

TEST_F(SbBlitterPixelTest, FillRectColorIsNotPremultiplied) {
  // This test checks that the color blending works fine assuming that fill rect
  // colors are specified in unpremultiplied alpha.
  SbBlitterSetColor(context_, SbBlitterColorFromRGBA(255, 0, 0, 255));
  SbBlitterFillRect(context_, SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()));
  SbBlitterSetBlending(context_, true);
  SbBlitterSetColor(context_, SbBlitterColorFromRGBA(0, 0, 255, 128));
  SbBlitterFillRect(context_, SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()));
}

TEST_F(SbBlitterPixelTest, ScissoredFillRect) {
  // This test checks whether SbBlitterFillRect() works with scissor rectangles.
  SbBlitterSetScissor(
      context_, SbBlitterMakeRect(32, 32, GetWidth() - 48, GetHeight() - 48));
  SbBlitterSetColor(context_, SbBlitterColorFromRGBA(255, 0, 0, 255));
  SbBlitterFillRect(context_, SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()));
}

TEST_F(SbBlitterPixelTest, ScissoredBlitRectToRect) {
  // This test checks whether SbBlitterBlitRectToRect() works with scissor
  // rectangles.
  SbBlitterSurface checker_image = CreateCheckerImageWithBlits(
      device_, context_, SbBlitterColorFromRGBA(255, 255, 255, 255),
      SbBlitterColorFromRGBA(0, 0, 0, 255), GetWidth(), GetHeight());

  SbBlitterSetRenderTarget(context_, render_target_);

  SbBlitterSetScissor(
      context_, SbBlitterMakeRect(32, 32, GetWidth() - 48, GetHeight() - 48));
  SbBlitterBlitRectToRect(context_, checker_image,
                          SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()),
                          SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()));

  SbBlitterDestroySurface(checker_image);
}

TEST_F(SbBlitterPixelTest, ScissoredBlitRectToRectWithSourceWidthOf1) {
  // This is a common scenario for rendering vertical linear gradients.
  SbBlitterSurface checker_image = CreateCheckerImageWithBlits(
      device_, context_, SbBlitterColorFromRGBA(255, 255, 255, 255),
      SbBlitterColorFromRGBA(0, 0, 0, 255), 1, GetHeight());

  SbBlitterSetRenderTarget(context_, render_target_);

  SbBlitterSetScissor(
      context_, SbBlitterMakeRect(32, 32, GetWidth() - 48, GetHeight() - 48));
  SbBlitterBlitRectToRect(context_, checker_image,
                          SbBlitterMakeRect(0, 0, 1, GetHeight()),
                          SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()));

  SbBlitterDestroySurface(checker_image);
}

TEST_F(SbBlitterPixelTest, ScissoredBlitRectToRectWithSourceHeightOf1) {
  // This is a common scenario for rendering horizontal linear gradients.
  SbBlitterSurface checker_image = CreateCheckerImageWithBlits(
      device_, context_, SbBlitterColorFromRGBA(255, 255, 255, 255),
      SbBlitterColorFromRGBA(0, 0, 0, 255), GetWidth(), 1);

  SbBlitterSetRenderTarget(context_, render_target_);

  SbBlitterSetScissor(
      context_, SbBlitterMakeRect(32, 32, GetWidth() - 48, GetHeight() - 48));
  SbBlitterBlitRectToRect(context_, checker_image,
                          SbBlitterMakeRect(0, 0, GetWidth(), 1),
                          SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()));

  SbBlitterDestroySurface(checker_image);
}

TEST_F(SbBlitterPixelTest, ScissoredBlitRectToRectPixelDataWithSourceWidthOf1) {
  // This is a common scenario for rendering vertical linear gradients.
  SbBlitterSurface checker_image = CreateCheckerImageWithPixelData(
      device_, SbBlitterColorFromRGBA(255, 255, 255, 255),
      SbBlitterColorFromRGBA(0, 0, 0, 255), 1, GetHeight());

  SbBlitterSetRenderTarget(context_, render_target_);

  SbBlitterSetScissor(
      context_, SbBlitterMakeRect(32, 32, GetWidth() - 48, GetHeight() - 48));
  SbBlitterBlitRectToRect(context_, checker_image,
                          SbBlitterMakeRect(0, 0, 1, GetHeight()),
                          SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()));

  SbBlitterDestroySurface(checker_image);
}

TEST_F(SbBlitterPixelTest,
       ScissoredBlitRectToRectPixelDataWithBlitSourceWidthOf1) {
  // This is a common scenario for rendering vertical linear gradients.
  SbBlitterSurface checker_image = CreateCheckerImageWithPixelData(
      device_, SbBlitterColorFromRGBA(255, 255, 255, 255),
      SbBlitterColorFromRGBA(0, 0, 0, 255), 8, GetHeight());

  SbBlitterSetRenderTarget(context_, render_target_);

  SbBlitterSetScissor(
      context_, SbBlitterMakeRect(32, 32, GetWidth() - 48, GetHeight() - 48));
  SbBlitterBlitRectToRect(context_, checker_image,
                          SbBlitterMakeRect(4, 0, 1, GetHeight()),
                          SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()));

  SbBlitterDestroySurface(checker_image);
}

TEST_F(SbBlitterPixelTest,
       ScissoredBlitRectToRectPixelDataWithSourceHeightOf1) {
  // This is a common scenario for rendering horizontal linear gradients.
  SbBlitterSurface checker_image = CreateCheckerImageWithPixelData(
      device_, SbBlitterColorFromRGBA(255, 255, 255, 255),
      SbBlitterColorFromRGBA(0, 0, 0, 255), GetWidth(), 1);

  SbBlitterSetRenderTarget(context_, render_target_);

  SbBlitterSetScissor(
      context_, SbBlitterMakeRect(32, 32, GetWidth() - 48, GetHeight() - 48));
  SbBlitterBlitRectToRect(context_, checker_image,
                          SbBlitterMakeRect(0, 0, GetWidth(), 1),
                          SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()));

  SbBlitterDestroySurface(checker_image);
}

TEST_F(SbBlitterPixelTest, ScissoredBlitRectToRectWithSourceWidthOf1AtTopLeft) {
  // This is a common scenario for rendering vertical linear gradients.  This
  // test exercises a bug that was found when rendering outside the top-left
  // corner of the render surface and scissor rectangle.
  SbBlitterSurface checker_image = CreateCheckerImageWithBlits(
      device_, context_, SbBlitterColorFromRGBA(255, 255, 255, 255),
      SbBlitterColorFromRGBA(0, 0, 0, 255), 1, GetHeight());

  SbBlitterSetRenderTarget(context_, render_target_);

  SbBlitterSetScissor(
      context_, SbBlitterMakeRect(-10, -10, GetWidth(), GetHeight()));
  SbBlitterBlitRectToRect(context_, checker_image,
                          SbBlitterMakeRect(0, 0, 1, GetHeight()),
                          SbBlitterMakeRect(-12, -12, GetWidth(), GetHeight()));

  SbBlitterDestroySurface(checker_image);
}

TEST_F(SbBlitterPixelTest,
       ScissoredBlitRectToRectWithSourceWidthOf1AtBottomRight) {
  // This is a common scenario for rendering vertical linear gradients.  This
  // test exercises a bug that was found when rendering outside the bottom-right
  // corner of the render surface and scissor rectangle.
  SbBlitterSurface checker_image = CreateCheckerImageWithBlits(
      device_, context_, SbBlitterColorFromRGBA(255, 255, 255, 255),
      SbBlitterColorFromRGBA(0, 0, 0, 255), 1, GetHeight());

  SbBlitterSetRenderTarget(context_, render_target_);

  SbBlitterSetScissor(
      context_, SbBlitterMakeRect(10, 10, GetWidth() + 10, GetHeight() + 10));
  SbBlitterBlitRectToRect(context_, checker_image,
                          SbBlitterMakeRect(0, 0, 1, GetHeight()),
                          SbBlitterMakeRect(20, 20, GetWidth(), GetHeight()));

  SbBlitterDestroySurface(checker_image);
}

TEST_F(SbBlitterPixelTest, ScissoredBlitRectToRectTiled) {
  // This test checks whether SbBlitterBlitRectToRectTiled() works with scissor
  // rectangles.
  SbBlitterSurface checker_image = CreateCheckerImageWithBlits(
      device_, context_, SbBlitterColorFromRGBA(255, 255, 255, 255),
      SbBlitterColorFromRGBA(0, 0, 0, 255), GetWidth(), GetHeight());

  SbBlitterSetRenderTarget(context_, render_target_);

  SbBlitterSetScissor(
      context_, SbBlitterMakeRect(32, 32, GetWidth() - 48, GetHeight() - 48));
  SbBlitterBlitRectToRectTiled(
      context_, checker_image,
      SbBlitterMakeRect(0, 0, GetWidth() * 2, GetHeight() * 2),
      SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()));

  SbBlitterDestroySurface(checker_image);
}

TEST_F(SbBlitterPixelTest, ScissoredBlitRectToRects) {
  // This test checks whether DrawRectsWithBatchBlit() works with scissor
  // rectangles.
  SbBlitterSurface checker_image = CreateCheckerImageWithBlits(
      device_, context_, SbBlitterColorFromRGBA(255, 255, 255, 255),
      SbBlitterColorFromRGBA(0, 0, 0, 255), GetWidth(), GetHeight());

  SbBlitterSetRenderTarget(context_, render_target_);

  SbBlitterSetScissor(
      context_, SbBlitterMakeRect(32, 32, GetWidth() - 48, GetHeight() - 48));
  DrawRectsWithBatchBlit(checker_image, context_, GetWidth(), GetHeight());

  SbBlitterDestroySurface(checker_image);
}

TEST_F(SbBlitterPixelTest, ScissorResetsWhenSetRenderTargetIsCalled) {
  // This test checks that the scissor rectangle is reset whenever
  // SbBlitterSetRenderTarget() is called.
  SbBlitterSetScissor(
      context_, SbBlitterMakeRect(32, 32, GetWidth() - 48, GetHeight() - 48));

  // This call should reset the scissor rectangle to the extents of the render
  // target.
  SbBlitterSetRenderTarget(context_, render_target_);

  SbBlitterSetColor(context_, SbBlitterColorFromRGBA(255, 0, 0, 255));
  SbBlitterFillRect(context_, SbBlitterMakeRect(0, 0, GetWidth(), GetHeight()));
}

}  // namespace
}  // namespace blitter_pixel_tests
}  // namespace nplb
}  // namespace starboard

#endif  // SB_HAS(BLITTER)
