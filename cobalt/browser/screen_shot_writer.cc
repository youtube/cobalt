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

#include "cobalt/browser/screen_shot_writer.h"

#include "base/bind.h"
#include "cobalt/renderer/backend/surface_info.h"
#include "cobalt/renderer/test/png_utils/png_encode.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkPixelRef.h"

using cobalt::renderer::test::png_utils::EncodeRGBAToPNG;

namespace cobalt {
namespace browser {
namespace {
void WriteRGBAPixelsToPNG(scoped_array<uint8> pixel_data,
                          const math::Size& dimensions,
                          const FilePath& output_file) {
  // Given a chunk of memory formatted as RGBA8 with pitch = width * 4, this
  // function will wrap that memory in a SkBitmap that does *not* own the
  // pixels and return that.
  const int kRGBABytesPerPixel = 4;
  SkBitmap bitmap;
  bitmap.installPixels(
      SkImageInfo::Make(dimensions.width(), dimensions.height(),
                        kRGBA_8888_SkColorType, kUnpremul_SkAlphaType),
      const_cast<uint8_t*>(pixel_data.get()),
      dimensions.width() * kRGBABytesPerPixel);

  // No conversion needed here, simply write out the pixels as is.
  EncodeRGBAToPNG(output_file,
                  static_cast<uint8_t*>(bitmap.pixelRef()->pixels()),
                  bitmap.width(), bitmap.height(), bitmap.rowBytes());
}
}  // namespace

ScreenShotWriter::ScreenShotWriter(renderer::Pipeline* pipeline)
    : pipeline_(pipeline),
      screenshot_thread_("Screenshot IO thread") {
  base::Thread::Options options;
  options.message_loop_type = MessageLoop::TYPE_IO;
  screenshot_thread_.StartWithOptions(options);
}

void ScreenShotWriter::RequestScreenshot(const FilePath& output_path,
                                         const base::Closure& complete) {
  DCHECK(last_submission_);
  renderer::Pipeline::Submission submission(last_submission_.value());
  submission.time_offset +=
      base::TimeTicks::HighResNow() - last_submission_time_;
  pipeline_->RasterizeToRGBAPixels(
      submission,
      base::Bind(&ScreenShotWriter::RasterizationComplete,
                 base::Unretained(this), output_path, complete));
}

void ScreenShotWriter::SetLastPipelineSubmission(
    const renderer::Pipeline::Submission& submission) {
  DCHECK(submission.render_tree.get());
  last_submission_ = submission;
  last_submission_time_ = base::TimeTicks::HighResNow();
}

void ScreenShotWriter::RasterizationComplete(
    const FilePath& output_path, const base::Closure& complete_cb,
    scoped_array<uint8> pixel_data, const math::Size& dimensions) {
  if (MessageLoop::current() != screenshot_thread_.message_loop()) {
    screenshot_thread_.message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&ScreenShotWriter::RasterizationComplete,
                   base::Unretained(this), output_path, complete_cb,
                   base::Passed(&pixel_data), dimensions));
    return;
  }
  // Blocking write to output_path.
  WriteRGBAPixelsToPNG(pixel_data.Pass(), dimensions, output_path);

  // Notify the caller that the screenshot is complete.
  if (!complete_cb.is_null()) {
    complete_cb.Run();
  }
}

}  // namespace browser
}  // namespace cobalt
