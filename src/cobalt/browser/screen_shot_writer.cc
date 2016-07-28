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
#include "base/file_util.h"
#include "cobalt/renderer/test/png_utils/png_encode.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkPixelRef.h"

using cobalt::renderer::test::png_utils::EncodeRGBAToBuffer;

namespace cobalt {
namespace browser {
namespace {
scoped_array<uint8> WriteRGBAPixelsToPNG(scoped_array<uint8> pixel_data,
                                         const math::Size& dimensions,
                                         size_t* out_num_bytes) {
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
  return EncodeRGBAToBuffer(static_cast<uint8_t*>(bitmap.pixelRef()->pixels()),
                            bitmap.width(), bitmap.height(), bitmap.rowBytes(),
                            out_num_bytes);
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
  RequestScreenshotToMemory(base::Bind(&ScreenShotWriter::EncodingComplete,
                                       base::Unretained(this), output_path,
                                       complete));
}

void ScreenShotWriter::RequestScreenshotToMemory(
    const PNGEncodeCompleteCallback& callback) {
  DCHECK(!callback.is_null());
  DCHECK(last_submission_);
  renderer::Submission submission(last_submission_.value());
  submission.time_offset +=
      base::TimeTicks::HighResNow() - last_submission_time_;
  pipeline_->RasterizeToRGBAPixels(
      submission, base::Bind(&ScreenShotWriter::RasterizationComplete,
                             base::Unretained(this), callback));
}

void ScreenShotWriter::SetLastPipelineSubmission(
    const renderer::Submission& submission) {
  DCHECK(submission.render_tree.get());
  last_submission_ = submission;
  last_submission_time_ = base::TimeTicks::HighResNow();
}

void ScreenShotWriter::RasterizationComplete(
    const PNGEncodeCompleteCallback& encode_complete_callback,
    scoped_array<uint8> pixel_data, const math::Size& dimensions) {
  if (MessageLoop::current() != screenshot_thread_.message_loop()) {
    screenshot_thread_.message_loop()->PostTask(
        FROM_HERE, base::Bind(&ScreenShotWriter::RasterizationComplete,
                              base::Unretained(this), encode_complete_callback,
                              base::Passed(&pixel_data), dimensions));
    return;
  }
  size_t num_bytes;
  scoped_array<uint8> png_data =
      WriteRGBAPixelsToPNG(pixel_data.Pass(), dimensions, &num_bytes);

  encode_complete_callback.Run(png_data.Pass(), num_bytes);
}

void ScreenShotWriter::EncodingComplete(const FilePath& output_path,
                                        const base::Closure& complete_callback,
                                        scoped_array<uint8> png_data,
                                        size_t num_bytes) {
  DCHECK_EQ(MessageLoop::current(), screenshot_thread_.message_loop());
  // Blocking write to output_path.
  int bytes_written = file_util::WriteFile(
      output_path, reinterpret_cast<char*>(png_data.get()), num_bytes);
  DLOG_IF(ERROR, bytes_written != num_bytes) << "Error writing PNG to file.";

  // Notify the caller that the screenshot is complete.
  if (!complete_callback.is_null()) {
    complete_callback.Run();
  }
}

}  // namespace browser
}  // namespace cobalt
