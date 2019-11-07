// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/dom/screenshot_manager.h"

#include <memory>

#include "base/time/time.h"
#include "cobalt/dom/screenshot.h"
#include "cobalt/render_tree/node.h"
#include "cobalt/render_tree/resource_provider_stub.h"
#include "cobalt/script/array_buffer.h"

namespace cobalt {
namespace dom {

ScreenshotManager::ScreenshotManager(
    script::EnvironmentSettings* settings,
    const ScreenshotManager::ProvideScreenshotFunctionCallback&
        screenshot_function_callback)
    : environment_settings_(settings),
      screenshot_function_callback_(screenshot_function_callback) {}

void ScreenshotManager::Screenshot(
    loader::image::EncodedStaticImage::ImageFormat desired_format,
    const scoped_refptr<render_tree::Node>& render_tree_root,
    std::unique_ptr<ScreenshotManager::InterfacePromiseValue::Reference>
        promise_reference) {
  DLOG(INFO) << "Will take a screenshot asynchronously";
  DCHECK(!screenshot_function_callback_.is_null());

  // We want to ScreenshotManager::FillScreenshot, on this thread.
  base::Callback<void(std::unique_ptr<uint8[]>, const math::Size&)>
      fill_screenshot = base::Bind(&ScreenshotManager::FillScreenshot,
                                   base::Unretained(this), next_ticket_id_,
                                   base::MessageLoop::current()->task_runner(),
                                   desired_format);
  bool was_emplaced =
      ticket_to_screenshot_promise_map_
          .emplace(next_ticket_id_, std::move(promise_reference))
          .second;
  DCHECK(was_emplaced);
  ++next_ticket_id_;

  screenshot_function_callback_.Run(
      render_tree_root, /*clip_rect=*/base::nullopt, fill_screenshot);
}

void ScreenshotManager::FillScreenshot(
    int64_t token,
    scoped_refptr<base::SingleThreadTaskRunner> expected_task_runner,
    loader::image::EncodedStaticImage::ImageFormat desired_format,
    std::unique_ptr<uint8[]> image_data, const math::Size& image_dimensions) {
  if (expected_task_runner && !expected_task_runner->BelongsToCurrentThread()) {
    expected_task_runner->PostTask(
        FROM_HERE,
        base::Bind(&ScreenshotManager::FillScreenshot, base::Unretained(this),
                   token, expected_task_runner, desired_format,
                   base::Passed(&image_data), image_dimensions));
    return;
  }
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(environment_settings_);

  auto iterator = ticket_to_screenshot_promise_map_.find(token);
  DCHECK(iterator != ticket_to_screenshot_promise_map_.end());
  const script::Promise<scoped_refptr<script::Wrappable>>& promise =
      iterator->second->value();
  do {
    if (!image_data) {
      // There was no data for the screenshot.
      LOG(WARNING) << "There was no data for the screenshot.";
      promise.Reject();
      break;
    }

    scoped_refptr<loader::image::EncodedStaticImage> encoded_image_data =
        CompressRGBAImage(desired_format, image_data.get(), image_dimensions);

    size_t encoded_size = encoded_image_data->GetEstimatedSizeInBytes();

    if (encoded_image_data->GetEstimatedSizeInBytes() > kint32max) {
      NOTREACHED();
      promise.Reject();
      break;
    }
    DLOG(INFO) << "Filling data in for the screenshot.";
    DOMSettings* dom_settings =
        base::polymorphic_downcast<dom::DOMSettings*>(environment_settings_);
    script::Handle<script::ArrayBuffer> pixel_data =
        script::ArrayBuffer::New(dom_settings->global_environment(),
                                 encoded_image_data->GetMemory(), encoded_size);
    scoped_refptr<script::Wrappable> promise_result =
        new dom::Screenshot(pixel_data);
    promise.Resolve(promise_result);
  } while (0);

  DCHECK(promise.State() != script::PromiseState::kPending);
  // Drop the reference to the promise since it will not be used.
  ticket_to_screenshot_promise_map_.erase(iterator);
}

}  // namespace dom
}  // namespace cobalt
