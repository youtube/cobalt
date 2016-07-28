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

#ifndef COBALT_BROWSER_SCREEN_SHOT_WRITER_H_
#define COBALT_BROWSER_SCREEN_SHOT_WRITER_H_

#include "base/callback.h"
#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread.h"
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
  typedef base::Callback<void(scoped_array<uint8>, size_t)>
      PNGEncodeCompleteCallback;

  // Constructs a new ScreenShotWriter that will create offscreen render targets
  // through |graphics_context|, and submit the most recent Pipeline::Submission
  // to |pipeline| to be rasterized.
  explicit ScreenShotWriter(renderer::Pipeline* pipeline);

  // Creates a PNG at |output_path| from the most recently submitted
  // Pipeline::Submission. When the PNG has been written to disk, |complete|
  // will be called.
  void RequestScreenshot(const FilePath& output_path,
                         const base::Closure& complete);

  // Creates a screenshot from the most recently submitted Pipeline::Submission
  // and converts it to a PNG. |callback| will be called with the PNG data and
  // the number of bytes in the array.
  void RequestScreenshotToMemory(const PNGEncodeCompleteCallback& callback);

  // This should be called whenever a new render tree is produced. The render
  // tree that is submitted here is the one that will be rasterized when a
  // screenshot is requested.
  void SetLastPipelineSubmission(const renderer::Submission& submission);

 private:
  // Callback function that will be fired from the rasterizer thread when
  // rasterization of |last_submission_| is complete.
  // After converting the |pixel_data| to an in-memory PNG,
  // |encode_complete_callback| will be called.
  void RasterizationComplete(
      const PNGEncodeCompleteCallback& encode_complete_callback,
      scoped_array<uint8> pixel_data, const math::Size& dimensions);

  // Callback function that will be fired from the RasterizationComplete
  // callback when PNG encoding is complete.
  // |write_complete_cb| will be called when |png_data| has been completely
  // written to |output_path|.
  void EncodingComplete(const FilePath& output_path,
                        const base::Closure& write_complete_cb,
                        scoped_array<uint8> png_data, size_t num_bytes);

  renderer::Pipeline* pipeline_;
  base::optional<renderer::Submission> last_submission_;
  base::TimeTicks last_submission_time_;

  base::Thread screenshot_thread_;
};

}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_SCREEN_SHOT_WRITER_H_
