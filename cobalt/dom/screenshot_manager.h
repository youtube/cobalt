// Copyright 2018 Google Inc. All Rights Reserved.
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

#ifndef COBALT_DOM_SCREENSHOT_MANAGER_H_
#define COBALT_DOM_SCREENSHOT_MANAGER_H_

#include <unordered_map>

#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "cobalt/loader/image/image.h"
#include "cobalt/loader/image/image_encoder.h"
#include "cobalt/render_tree/node.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/promise.h"
#include "cobalt/script/script_value.h"
#include "cobalt/script/script_value_factory.h"

namespace cobalt {
namespace dom {

class ScreenshotManager {
 public:
  using InterfacePromise = script::Promise<scoped_refptr<script::Wrappable>>;
  using InterfacePromiseValue = script::ScriptValue<InterfacePromise>;

  using OnEncodedStaticImageCallback = base::Callback<void(
      const scoped_refptr<loader::image::EncodedStaticImage>& image_data)>;
  using OnUnencodedImageCallback =
      base::Callback<void(scoped_array<uint8>, const math::Size&)>;

  using ProvideScreenshotFunctionCallback =
      base::Callback<void(const scoped_refptr<render_tree::Node>&,
                          const OnUnencodedImageCallback&)>;

  explicit ScreenshotManager(
      const ProvideScreenshotFunctionCallback& screenshot_function_callback_);

  void Screenshot(
      loader::image::EncodedStaticImage::ImageFormat desired_format,
      const scoped_refptr<render_tree::Node>& render_tree_root,
      std::unique_ptr<ScreenshotManager::InterfacePromiseValue::Reference>
          promise_reference);
  void SetEnvironmentSettings(script::EnvironmentSettings* settings);

 private:
  void FillScreenshot(
      int64_t token,
      scoped_refptr<base::MessageLoopProxy> expected_message_loop,
      loader::image::EncodedStaticImage::ImageFormat desired_format,
      scoped_array<uint8> image_data, const math::Size& dimensions);

  int64_t next_ticket_id_ = 0;

  using TicketToPromiseMap =
      std::unordered_map<int64_t,
                         std::unique_ptr<InterfacePromiseValue::Reference>>;

  base::ThreadChecker thread_checker_;
  script::EnvironmentSettings* environment_settings_ = nullptr;
  TicketToPromiseMap ticket_to_screenshot_promise_map_;
  ProvideScreenshotFunctionCallback screenshot_function_callback_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_SCREENSHOT_MANAGER_H_
