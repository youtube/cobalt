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

#include "cobalt/renderer/render_tree_pixel_tester.h"

#include <algorithm>
#include <string>

#include "base/file_path.h"
#include "base/file_util.h"
#include "cobalt/renderer/backend/default_graphics_system.h"
#include "cobalt/renderer/backend/graphics_context.h"
#include "cobalt/renderer/backend/graphics_system.h"
#include "cobalt/renderer/backend/render_target.h"
#include "cobalt/renderer/rasterizer/rasterizer.h"
#include "cobalt/renderer/renderer_module.h"
#include "cobalt/renderer/test/png_utils/png_decode.h"
#include "cobalt/renderer/test/png_utils/png_encode.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "third_party/skia/include/effects/SkBlurImageFilter.h"

// Avoid overriding of Windows' CreateDirectory macro.
#undef CreateDirectory

using cobalt::render_tree::ResourceProvider;
using cobalt::renderer::backend::GraphicsContext;
using cobalt::renderer::backend::GraphicsSystem;
using cobalt::renderer::backend::RenderTarget;
using cobalt::renderer::test::png_utils::DecodePNGToRGBA;
using cobalt::renderer::test::png_utils::EncodeRGBAToPNG;

namespace cobalt {
namespace renderer {

RenderTreePixelTester::Options::Options()
    : gaussian_blur_sigma(6.2f),
      acceptable_channel_range(0.08f),
      output_failed_test_details(false),
      output_all_test_details(false) {}

RenderTreePixelTester::RenderTreePixelTester(
    const math::Size& test_surface_dimensions,
    const FilePath& expected_results_directory,
    const FilePath& output_directory, const Options& options)
    : expected_results_directory_(expected_results_directory),
      output_directory_(output_directory),
      options_(options) {
  graphics_system_ = cobalt::renderer::backend::CreateDefaultGraphicsSystem();
  graphics_context_ = graphics_system_->CreateGraphicsContext();

  // Create our offscreen surface that will be the target of our test
  // rasterizations.
  test_surface_ =
      graphics_context_->CreateDownloadableOffscreenRenderTarget(
          test_surface_dimensions);

  // Create the rasterizer using the platform default RenderModule options.
  RendererModule::Options render_module_options;

  // Don't purge the Skia font caches on destruction. Doing so will result in
  // too much font thrashing during the tests.
  render_module_options.purge_skia_font_caches_on_destruction = false;

  rasterizer_ = render_module_options.create_rasterizer_function.Run(
      graphics_context_.get(), render_module_options);
}

RenderTreePixelTester::~RenderTreePixelTester() {}

ResourceProvider* RenderTreePixelTester::GetResourceProvider() const {
  return rasterizer_->GetResourceProvider();
}

namespace {

// Convenience function that will modify a base file path (without an
// extension) to include a postfix and an extension.
// e.g. ModifyBaseFileName("output/tests/my_test", "-expected", "png")
//      will return "output/tests/my_test-expected.png".
FilePath ModifyBaseFileName(const FilePath& base_file_name,
                            const std::string& postfix,
                            const std::string& extension) {
  return base_file_name.InsertBeforeExtension(postfix).AddExtension(extension);
}

// This utility function will take a SkBitmap and perform a Gaussian blur on
// it, returning a new, blurred SkBitmap.  The sigma parameter defines how
// strong the blur should be.
SkBitmap BlurBitmap(const SkBitmap& bitmap, float sigma) {
  SkBitmap premul_alpha_bitmap;
  if (bitmap.info().colorType() == kN32_SkColorType &&
      bitmap.info().alphaType() == kPremul_SkAlphaType) {
    premul_alpha_bitmap = bitmap;
  } else {
    // We need to convert our image to premultiplied alpha and N32 color
    // before proceeding to blur them, as Skia is designed to primarily deal
    // only with images in this format.
    SkImageInfo premul_alpha_image_info = SkImageInfo::Make(
        bitmap.width(), bitmap.height(), kN32_SkColorType, kPremul_SkAlphaType);
    premul_alpha_bitmap.allocPixels(premul_alpha_image_info);
    bitmap.readPixels(premul_alpha_image_info, premul_alpha_bitmap.getPixels(),
                      premul_alpha_bitmap.rowBytes(), 0, 0);
  }
  SkBitmap blurred_bitmap;
  blurred_bitmap.allocPixels(SkImageInfo::Make(
      bitmap.width(), bitmap.height(), kN32_SkColorType, kPremul_SkAlphaType));

  SkPaint paint;
  sk_sp<SkImageFilter> blur_filter(
      SkBlurImageFilter::Make(sigma, sigma, nullptr));
  paint.setImageFilter(blur_filter);

  SkCanvas canvas(blurred_bitmap);
  canvas.clear(SkColorSetARGB(0, 0, 0, 0));
  canvas.drawBitmap(premul_alpha_bitmap, 0, 0, &paint);

  return blurred_bitmap;
}

bool BitmapsAreEqual(const SkBitmap& bitmap_a, const SkBitmap& bitmap_b) {
  if (bitmap_a.height() != bitmap_b.height() ||
      bitmap_a.rowBytes() != bitmap_b.rowBytes()) {
    return false;
  }

  // Do not need to lock pixels here.  See:
  // https://bugs.chromium.org/p/skia/issues/detail?id=6481&desc=2
  void* pixels_a = reinterpret_cast<void*>(bitmap_a.getPixels());
  void* pixels_b = reinterpret_cast<void*>(bitmap_b.getPixels());
  size_t byte_count = bitmap_a.rowBytes() * bitmap_a.height();
  return memcmp(pixels_a, pixels_b, byte_count) == 0;
}

// Compares bitmap_a with bitmap_b, where the comparison is done by checking
// each pixel to see if any one of its color channel values differ between
// bitmap_a to bitmap_b by more than acceptable_channel_range.  If so,
// number_of_diff_pixels is incremented and the corresponding pixel in the
// returned diff SkBitmap is set to white with full alpha.  If the pixel color
// channel values are not found to differ significantly, the corresponding
// pixel in the returned diff SkBitmap is set to black with no alpha.
SkBitmap DiffBitmaps(const SkBitmap& bitmap_a, const SkBitmap& bitmap_b,
                     int acceptable_channel_range, int* number_of_diff_pixels) {
  // Construct a diff bitmap where we can place the results of our diff,
  // allowing us to mark which pixels differ between bitmap_a and bitmap_b.
  SkBitmap bitmap_diff;
  bitmap_diff.allocPixels(SkImageInfo::Make(bitmap_a.width(), bitmap_a.height(),
                                            kRGBA_8888_SkColorType,
                                            kUnpremul_SkAlphaType));

  // Initialize the number of pixels that we have found to differ significantly
  // from bitmap_a to bitmap_b to 0.
  *number_of_diff_pixels = 0;
  // Start checking for pixel differences row by row.
  for (int r = 0; r < bitmap_a.height(); ++r) {
    const uint8_t* pixels_a =
        static_cast<uint8_t*>(bitmap_a.pixelRef()->pixels()) +
        bitmap_a.rowBytes() * r;
    const uint8_t* pixels_b =
        static_cast<uint8_t*>(bitmap_b.pixelRef()->pixels()) +
        bitmap_b.rowBytes() * r;
    // Since the diff image will be set to either all black or all white, we
    // reference its pixels with a uint32_t since it simplifies writing to it.
    uint32_t* pixels_diff =
        static_cast<uint32_t*>(bitmap_diff.pixelRef()->pixels()) +
        bitmap_diff.rowBytes() * r / sizeof(uint32_t);
    for (int c = 0; c < bitmap_a.width(); ++c) {
      // Check each pixel in the current row for differences.  We do this by
      // looking at each color channel seperately and taking the max of the
      // differences we see in each channel.
      int max_diff = 0;
      for (int i = 0; i < 4; ++i) {
        max_diff = std::max(max_diff, std::abs(pixels_a[i] - pixels_b[i]));
      }

      // If the maximum color channel difference is larger than the acceptable
      // error range, we count one diff pixel and adjust the diff SkBitmap
      // accordingly.
      if (max_diff > acceptable_channel_range) {
        ++*number_of_diff_pixels;
        // Set the diff image pixel to all white with full alpha.
        *pixels_diff = 0xFF0000FF;
      } else {
        // Set the diff image pixel to all black with no alpha.
        *pixels_diff = 0x00000000;
      }

      // Get ready for the next pixel in the row.
      pixels_a += 4;
      pixels_b += 4;
      pixels_diff += 1;
    }
  }

  return bitmap_diff;
}

// Helper function to simplify the process of encoding a Skia RGBA8 SkBitmap
// object to a PNG file.
void EncodeSkBitmapToPNG(const FilePath& output_file, const SkBitmap& bitmap) {
  if (bitmap.info().alphaType() == kUnpremul_SkAlphaType &&
      bitmap.info().colorType() == kRGBA_8888_SkColorType) {
    // No conversion needed here, simply write out the pixels as is.
    EncodeRGBAToPNG(output_file,
                    static_cast<uint8_t*>(bitmap.pixelRef()->pixels()),
                    bitmap.width(), bitmap.height(), bitmap.rowBytes());
  } else {
    // First convert the pixels to the proper format and then output them.
    SkImageInfo output_image_info =
        SkImageInfo::Make(bitmap.width(), bitmap.height(),
                          kRGBA_8888_SkColorType, kUnpremul_SkAlphaType);
    scoped_array<uint8_t> pixels(
        new uint8_t[output_image_info.minRowBytes() * bitmap.height()]);

    // Reformat and copy the pixel data into our fresh pixel buffer.
    bitmap.readPixels(output_image_info, pixels.get(),
                      output_image_info.minRowBytes(), 0, 0);

    // Write the pixels out to disk.
    EncodeRGBAToPNG(output_file, pixels.get(), bitmap.width(), bitmap.height(),
                    output_image_info.minRowBytes());
  }
}

// Given a chunk of memory formatted as RGBA8 with pitch = width * 4, this
// function will wrap that memory in a SkBitmap that does *not* own the
// pixels and return that.
const SkBitmap CreateBitmapFromRGBAPixels(const math::SizeF& dimensions,
                                          const uint8_t* pixels) {
  const int kRGBABytesPerPixel = 4;
  SkBitmap bitmap;
  bitmap.installPixels(
      SkImageInfo::Make(dimensions.width(), dimensions.height(),
                        kRGBA_8888_SkColorType, kUnpremul_SkAlphaType),
      const_cast<uint8_t*>(pixels), dimensions.width() * kRGBABytesPerPixel);
  return bitmap;
}

bool TestActualAgainstExpectedBitmap(float gaussian_blur_sigma,
                                     float acceptable_channel_range,
                                     const SkBitmap& expected_bitmap,
                                     const SkBitmap& actual_bitmap,
                                     const FilePath& output_base_filename,
                                     bool output_failed_test_details,
                                     bool output_all_test_details) {
  DCHECK_EQ(kRGBA_8888_SkColorType, expected_bitmap.colorType());
  DCHECK_EQ(kRGBA_8888_SkColorType, actual_bitmap.colorType());

  // We can try an exact comparison if we don't need to dump out the
  // diff and blur images.
  bool quick_test_ok = !output_failed_test_details && !output_all_test_details;

  if (quick_test_ok && BitmapsAreEqual(expected_bitmap, actual_bitmap)) {
    return true;
  }

  // We first blur both the actual and expected bitmaps before testing them.
  // This is done to permit small 1 or 2 pixel translation differences in the
  // images.  If these small differences are not acceptable, the blur amount
  // specified by gaussian_blur_sigma should be lowered to make the tests
  // stricter.
  SkBitmap blurred_actual_bitmap =
      BlurBitmap(actual_bitmap, gaussian_blur_sigma);
  SkBitmap blurred_expected_bitmap =
      BlurBitmap(expected_bitmap, gaussian_blur_sigma);

  // Diff the blurred actual image with the blurred expected image and
  // count how many pixels are out of range.
  int number_of_diff_pixels = 0;
  SkBitmap diff_image =
      DiffBitmaps(blurred_expected_bitmap, blurred_actual_bitmap,
                  acceptable_channel_range * 255, &number_of_diff_pixels);

  // If the user has requested it via command-line flags, we can also output
  // the images that were used by this test to help debug any problems.
  if (output_all_test_details ||
      (number_of_diff_pixels > 0 && output_failed_test_details)) {
    file_util::CreateDirectory(output_base_filename.DirName());

    EncodeSkBitmapToPNG(
        ModifyBaseFileName(output_base_filename, "-actual", "png"),
        actual_bitmap);
    EncodeSkBitmapToPNG(
        ModifyBaseFileName(output_base_filename, "-expected", "png"),
        expected_bitmap);
    EncodeSkBitmapToPNG(
        ModifyBaseFileName(output_base_filename, "-actual-blurred", "png"),
        blurred_actual_bitmap);
    EncodeSkBitmapToPNG(
        ModifyBaseFileName(output_base_filename, "-expected-blurred", "png"),
        blurred_expected_bitmap);
    EncodeSkBitmapToPNG(
        ModifyBaseFileName(output_base_filename, "-diff", "png"), diff_image);
  }

  // Execute the main check for this rasterizer test:  Are all pixels
  // in the generated image in the given range of the expected image
  // pixels.
  return number_of_diff_pixels == 0;
}
}  // namespace

scoped_array<uint8_t> RenderTreePixelTester::RasterizeRenderTree(
    const scoped_refptr<cobalt::render_tree::Node>& tree) const {
  // Rasterize the test render tree to the rasterizer's offscreen render target.
  rasterizer::Rasterizer::Options rasterizer_options;
  rasterizer_options.flags = rasterizer::Rasterizer::kSubmitFlags_Clear;
  rasterizer_->Submit(tree, test_surface_, rasterizer_options);

  // Load the texture's pixel data into a CPU memory buffer and return it.
  return graphics_context_->DownloadPixelDataAsRGBA(test_surface_);
}

void RenderTreePixelTester::Rebaseline(
    const scoped_refptr<cobalt::render_tree::Node>& test_tree,
    const FilePath& expected_base_filename) const {
  scoped_array<uint8_t> test_image_pixels = RasterizeRenderTree(test_tree);

  // Wrap the generated image's raw RGBA8 pixel data in a SkBitmap so that
  // we can manipulate it using Skia.
  const SkBitmap actual_bitmap = CreateBitmapFromRGBAPixels(
      test_surface_->GetSize(), test_image_pixels.get());

  FilePath output_base_filename(
      output_directory_.Append(expected_base_filename));

  // Create the output directory if it doesn't already exist.
  file_util::CreateDirectory(output_base_filename.DirName());

  // If the 'rebase' flag is set, we should not run any actual tests but
  // instead output the results of our tests so that they can be used as
  // expected output for subsequent tests.
  EncodeSkBitmapToPNG(
      ModifyBaseFileName(output_base_filename, "-expected", "png"),
      actual_bitmap);
}

bool RenderTreePixelTester::TestTree(
    const scoped_refptr<cobalt::render_tree::Node>& test_tree,
    const FilePath& expected_base_filename) const {
  scoped_array<uint8_t> test_image_pixels = RasterizeRenderTree(test_tree);

  // Wrap the generated image's raw RGBA8 pixel data in a SkBitmap so that
  // we can manipulate it using Skia.
  const SkBitmap actual_bitmap = CreateBitmapFromRGBAPixels(
      test_surface_->GetSize(), test_image_pixels.get());

  // Here we proceed with the the pixel tests.  We must first load the
  // expected output image from disk and use that to compare against
  // the synthesized image.
  int expected_width;
  int expected_height;
  FilePath expected_output_file = expected_results_directory_.Append(
      ModifyBaseFileName(expected_base_filename, "-expected", "png"));
  if (!file_util::PathExists(expected_output_file)) {
    DLOG(WARNING) << "Expected pixel test output file \""
                  << expected_output_file.value() << "\" cannot be found.";
    // If the expected output file does not exist, we cannot continue, so
    // return in failure.
    return false;
  }
  scoped_array<uint8_t> expected_image_pixels =
      DecodePNGToRGBA(expected_output_file, &expected_width, &expected_height);

  DCHECK_EQ(test_surface_->GetSize().width(), expected_width);
  DCHECK_EQ(test_surface_->GetSize().height(), expected_height);

  // We then wrap the expected image in a SkBitmap so that we can manipulate
  // it with Skia.
  const SkBitmap expected_bitmap = CreateBitmapFromRGBAPixels(
      test_surface_->GetSize(), expected_image_pixels.get());

  // Finally we perform the actual pixel tests on the bitmap given the
  // actual and expected bitmaps.  If it is requested that test details
  // be output, then this function call may have the side effect of generating
  // these details as output files.
  return TestActualAgainstExpectedBitmap(
      options_.gaussian_blur_sigma, options_.acceptable_channel_range,
      expected_bitmap, actual_bitmap,
      output_directory_.Append(expected_base_filename),
      options_.output_failed_test_details, options_.output_all_test_details);
}

}  // namespace renderer
}  // namespace cobalt
