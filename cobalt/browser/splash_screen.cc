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

#include "cobalt/browser/splash_screen.h"

#include "base/bind.h"

namespace cobalt {
namespace browser {

// Static
const char SplashScreen::Options::kDefaultSplashScreenURL[] =
    "h5vcc-embedded://splash_screen.html";

SplashScreen::SplashScreen(
    const WebModule::OnRenderTreeProducedCallback&
        render_tree_produced_callback,
    network::NetworkModule* network_module, const math::Size& window_dimensions,
    render_tree::ResourceProvider* resource_provider, float layout_refresh_rate,
    const SplashScreen::Options& options) {
  WebModule::Options web_module_options;
  web_module_options.name = "SplashScreenWebModule";

  web_module_.reset(new WebModule(
      options.url, render_tree_produced_callback,
      base::Bind(&SplashScreen::OnError, base::Unretained(this)),
      &stub_media_module_, network_module, window_dimensions, resource_provider,
      layout_refresh_rate, web_module_options));
}

}  // namespace browser
}  // namespace cobalt
