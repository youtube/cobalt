/*
 * Copyright 2014 Google Inc. All Rights Reserved.
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

#include "cobalt/browser/browser_module.h"

#include "base/bind.h"
#include "base/logging.h"

namespace cobalt {
namespace browser {
namespace {

// TODO(***REMOVED***): Request viewport size from graphics pipeline and subscribe to
// viewport size changes.
const int kInitialWidth = 1920;
const int kInitialHeight = 1080;

}  // namespace

BrowserModule::BrowserModule(const Options& options)
    : renderer_module_(options.renderer_module_options),
      web_module_(base::Bind(&BrowserModule::OnRenderTreeProduced,
                             base::Unretained(this)),
                  WebModule::ErrorCallback(),
                  math::Size(kInitialWidth, kInitialHeight),
                  renderer_module_.pipeline()->GetResourceProvider(),
                  options.web_module_options),
      input_device_manager_(input::InputDeviceManager::Create(base::Bind(
          &BrowserModule::OnKeyEventProduced, base::Unretained(this)))) {}

BrowserModule::~BrowserModule() {}

void BrowserModule::OnRenderTreeProduced(
    const scoped_refptr<render_tree::Node>& render_tree) {
  renderer_module_.pipeline()->Submit(render_tree);
}

void BrowserModule::OnKeyEventProduced(
    const scoped_refptr<dom::KeyboardEvent>& event) {
  web_module_.InjectEvent(event);
}

}  // namespace browser
}  // namespace cobalt
