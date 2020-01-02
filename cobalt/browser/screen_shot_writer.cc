// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include <memory>

#include "cobalt/browser/screen_shot_writer.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/loader/image/image_encoder.h"
#include "cobalt/render_tree/resource_provider_stub.h"

namespace cobalt {
namespace browser {

ScreenShotWriter::ScreenShotWriter(renderer::Pipeline* pipeline)
    : pipeline_(pipeline), screenshot_thread_("ScreenshotIOThd") {
  DCHECK(pipeline);
  screenshot_thread_.Start();
}

void ScreenShotWriter::RequestScreenshotToFile(
    loader::image::EncodedStaticImage::ImageFormat desired_format,
    const base::FilePath& output_path,
    const scoped_refptr<render_tree::Node>& render_tree_root,
    const base::Optional<math::Rect>& clip_rect,
    const base::Closure& complete) {
  base::Callback<void(const scoped_refptr<loader::image::EncodedStaticImage>&)>
      done_encoding_callback =
          base::Bind(&ScreenShotWriter::WriteEncodedImageToFile,
                     base::Unretained(this), output_path, complete);

  renderer::Pipeline::RasterizationCompleteCallback callback =
      base::Bind(&ScreenShotWriter::EncodeData, base::Unretained(this),
                 desired_format, done_encoding_callback);
  RequestScreenshotToMemoryUnencoded(render_tree_root, clip_rect, callback);
}

void ScreenShotWriter::RequestScreenshotToMemoryUnencoded(
    const scoped_refptr<render_tree::Node>& render_tree_root,
    const base::Optional<math::Rect>& clip_rect,
    const renderer::Pipeline::RasterizationCompleteCallback& callback) {
  DCHECK(!callback.is_null());
  if (clip_rect && clip_rect->IsEmpty()) {
    callback.Run(std::unique_ptr<uint8[]>(), math::Size());
  } else {
    pipeline_->RasterizeToRGBAPixels(
        render_tree_root, clip_rect,
        base::Bind(&ScreenShotWriter::RunOnScreenshotThread,
                   base::Unretained(this), callback));
  }
}

void ScreenShotWriter::RequestScreenshotToMemory(
    loader::image::EncodedStaticImage::ImageFormat desired_format,
    const scoped_refptr<render_tree::Node>& render_tree_root,
    const base::Optional<math::Rect>& clip_rect,
    const ScreenShotWriter::ImageEncodeCompleteCallback& screenshot_ready) {
  renderer::Pipeline::RasterizationCompleteCallback callback =
      base::Bind(&ScreenShotWriter::EncodeData, base::Unretained(this),
                 desired_format, screenshot_ready);
  RequestScreenshotToMemoryUnencoded(render_tree_root, clip_rect, callback);
}

void ScreenShotWriter::EncodeData(
    loader::image::EncodedStaticImage::ImageFormat desired_format,
    const base::Callback<
        void(const scoped_refptr<loader::image::EncodedStaticImage>&)>&
        done_encoding_callback,
    std::unique_ptr<uint8[]> pixel_data, const math::Size& image_dimensions) {
  TRACE_EVENT0("cobalt::browser", "ScreenshotWriter::EncodeData()");
  scoped_refptr<loader::image::EncodedStaticImage> image_data;
  if (!image_dimensions.IsEmpty()) {
    image_data = loader::image::CompressRGBAImage(
        desired_format, pixel_data.get(), image_dimensions);
  }
  done_encoding_callback.Run(image_data);
}

void ScreenShotWriter::RunOnScreenshotThread(
    const renderer::Pipeline::RasterizationCompleteCallback& callback,
    std::unique_ptr<uint8[]> image_data, const math::Size& image_dimensions) {
  DCHECK(image_data);

  if (base::MessageLoop::current() != screenshot_thread_.message_loop()) {
    screenshot_thread_.message_loop()->task_runner()->PostTask(
        FROM_HERE, base::Bind(&ScreenShotWriter::RunOnScreenshotThread,
                              base::Unretained(this), callback,
                              base::Passed(&image_data), image_dimensions));
    return;
  }

  callback.Run(std::move(image_data), image_dimensions);
}

void ScreenShotWriter::WriteEncodedImageToFile(
    const base::FilePath& output_path, const base::Closure& complete_callback,
    const scoped_refptr<loader::image::EncodedStaticImage>& image_data) {
  DCHECK_EQ(base::MessageLoop::current(), screenshot_thread_.message_loop());

  // Blocking write to output_path.
  if (!image_data) {
    DLOG(ERROR)
        << "Unable to take screenshot because image data is unavailable.";
  } else {
    int num_bytes = static_cast<int>(image_data->GetEstimatedSizeInBytes());
    int bytes_written = base::WriteFile(
        output_path, reinterpret_cast<char*>(image_data->GetMemory()),
        num_bytes);
    LOG_IF(ERROR, bytes_written != num_bytes)
        << "Error writing screenshot to file.";
  }

  // Notify the caller that the screenshot is complete.
  if (!complete_callback.is_null()) {
    complete_callback.Run();
  }
}

}  // namespace browser
}  // namespace cobalt
