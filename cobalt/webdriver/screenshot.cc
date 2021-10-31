// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/webdriver/screenshot.h"
#include "base/base64.h"
#include "base/synchronization/waitable_event.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/webdriver/util/command_result.h"

namespace cobalt {
namespace webdriver {

namespace {
// Helper struct for getting a PNG screenshot synchronously.
struct ScreenshotResultContext {
  ScreenshotResultContext()
      : complete_event(base::WaitableEvent::ResetPolicy::MANUAL,
                       base::WaitableEvent::InitialState::NOT_SIGNALED) {}
  scoped_refptr<loader::image::EncodedStaticImage> compressed_file;
  base::WaitableEvent complete_event;
};

// Callback function to be called when PNG encoding is complete.
void OnPNGEncodeComplete(ScreenshotResultContext* context,
                         const scoped_refptr<loader::image::EncodedStaticImage>&
                             compressed_image_data) {
  TRACE_EVENT0("cobalt::WebDriver", "ScreenshotHelper::onPNGEncodeComplete()");

  DCHECK(context);
  DCHECK(!compressed_image_data ||
         compressed_image_data->GetImageFormat() ==
             loader::image::EncodedStaticImage::ImageFormat::kPNG);
  context->compressed_file = compressed_image_data;
  context->complete_event.Signal();
}

}  // namespace

util::CommandResult<std::string> Screenshot::RequestScreenshot(
    const GetScreenshotFunction& screenshot_function,
    base::Optional<math::Rect> clip_rect) {
  typedef util::CommandResult<std::string> CommandResult;

  // Request the screenshot and wait for the PNG data.
  ScreenshotResultContext context;
  screenshot_function.Run(
      // Webdriver spec requires us to encode to PNG format.
      loader::image::EncodedStaticImage::ImageFormat::kPNG, clip_rect,
      base::Bind(&OnPNGEncodeComplete, base::Unretained(&context)));
  context.complete_event.Wait();

  uint32 file_size_in_bytes =
      context.compressed_file
          ? context.compressed_file->GetEstimatedSizeInBytes()
          : 0;
  if (file_size_in_bytes == 0 || !context.compressed_file->GetMemory()) {
    return CommandResult(protocol::Response::kUnknownError,
                         "Failed to take screenshot.");
  }

  // Encode the PNG data as a base64 encoded string.
  std::string encoded;
  {
    // base64 encode the contents of the file to be returned to the client.
    if (!base::Base64Encode(
            base::StringPiece(
                reinterpret_cast<char*>(context.compressed_file->GetMemory()),
                file_size_in_bytes),
            &encoded)) {
      return CommandResult(protocol::Response::kUnknownError,
                           "Failed to base64 encode screenshot file contents.");
    }
  }
  return CommandResult(encoded);
}

}  // namespace webdriver
}  // namespace cobalt
