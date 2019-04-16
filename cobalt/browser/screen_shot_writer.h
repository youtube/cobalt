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

#ifndef COBALT_BROWSER_SCREEN_SHOT_WRITER_H_
#define COBALT_BROWSER_SCREEN_SHOT_WRITER_H_

#include <memory>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "base/threading/thread.h"
#include "cobalt/dom/screenshot_manager.h"
#include "cobalt/loader/image/image.h"
#include "cobalt/math/rect.h"
#include "cobalt/render_tree/image.h"
#include "cobalt/renderer/pipeline.h"
#include "cobalt/renderer/submission.h"

namespace cobalt {
namespace browser {

// Helper class for creating screenshots. A ScreenShotWriter instance will hold
// a reference to a Pipeline::Submission instance and, when requested, create
// a screenshot of the render tree.
// File I/O is performed on a dedicated thread.
class ScreenShotWriter {
 public:
  // Callback providing the encoded screenshot. If the screenshot could not be
  // made (e.g. because an empty |clip_rect| was specified) the |image_data|
  // will reference NULL.
  using ImageEncodeCompleteCallback = base::Callback<void(
      const scoped_refptr<loader::image::EncodedStaticImage>& image_data)>;

  // Constructs a new ScreenShotWriter that will create offscreen render targets
  // through |graphics_context|, and submit the most recent Pipeline::Submission
  // to |pipeline| to be rasterized.
  explicit ScreenShotWriter(renderer::Pipeline* pipeline);

  // Creates a screenshot at |output_path| from the most recently submitted
  // Pipeline::Submission. When the file has been written to disk, |complete|
  // will be called.
  void RequestScreenshotToFile(
      loader::image::EncodedStaticImage::ImageFormat desired_format,
      const base::FilePath& output_path,
      const scoped_refptr<render_tree::Node>& render_tree_root,
      const base::Optional<math::Rect>& clip_rect,
      const base::Closure& complete);

  // Renders the |render_tree_root| and converts it to the image format that is
  // requested. |callback| will be called with the image data.
  void RequestScreenshotToMemory(
      loader::image::EncodedStaticImage::ImageFormat desired_format,
      const scoped_refptr<render_tree::Node>& render_tree_root,
      const base::Optional<math::Rect>& clip_rect,
      const ImageEncodeCompleteCallback& callback);

  // Runs callback on screenshot thread.
  void RequestScreenshotToMemoryUnencoded(
      const scoped_refptr<render_tree::Node>& render_tree_root,
      const base::Optional<math::Rect>& clip_rect,
      const renderer::Pipeline::RasterizationCompleteCallback& callback);

 private:
  // Callback function that will be fired from the rasterizer thread when
  // rasterization of |last_submission_| is complete.
  // After converting the |pixel_data| to an in-memory PNG,
  // |encode_complete_callback| will be called.
  void RunOnScreenshotThread(
      const renderer::Pipeline::RasterizationCompleteCallback& cb,
      std::unique_ptr<uint8[]> image_data, const math::Size& image_dimensions);

  void EncodeData(loader::image::EncodedStaticImage::ImageFormat desired_format,
                  const base::Callback<void(
                      const scoped_refptr<loader::image::EncodedStaticImage>&)>&
                      done_encoding_callback,
                  std::unique_ptr<uint8[]> pixel_data,
                  const math::Size& image_dimensions);

  // Callback function that will be fired from the RasterizationComplete
  // callback when image encoding is complete.
  // |write_complete_cb| will be called when data from |image_data| has been
  // completely written to |output_path|.
  void WriteEncodedImageToFile(
      const base::FilePath& output_path, const base::Closure& complete_callback,
      const scoped_refptr<loader::image::EncodedStaticImage>& image_data);

  renderer::Pipeline* pipeline_;

  base::Thread screenshot_thread_;
};

}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_SCREEN_SHOT_WRITER_H_
