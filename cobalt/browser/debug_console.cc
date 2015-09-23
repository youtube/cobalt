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

#include "cobalt/browser/debug_console.h"

#include "cobalt/dom/event_names.h"
#include "cobalt/input/keyboard_code.h"

namespace cobalt {
namespace browser {

DebugConsole::DebugConsole(
    const GURL& url, const WebModule::OnRenderTreeProducedCallback&
                         render_tree_produced_callback,
    const base::Callback<void(const std::string&)>& error_callback,
    media::MediaModule* media_module, network::NetworkModule* network_module,
    const math::Size& window_dimensions,
    render_tree::ResourceProvider* resource_provider, float layout_refresh_rate,
    const WebModule::Options& options)
    : web_module_(url, render_tree_produced_callback, error_callback,
                  media_module, network_module, window_dimensions,
                  resource_provider, layout_refresh_rate, options) {}

DebugConsole::~DebugConsole() {}

bool DebugConsole::FilterKeyEvent(
    const scoped_refptr<dom::KeyboardEvent>& event) {
  // Assume here the full debug console is visible - pass all events to its
  // web module, and return false to indicate the event has been consumed.
  web_module_.InjectEvent(event);
  return false;
}

}  // namespace browser
}  // namespace cobalt
