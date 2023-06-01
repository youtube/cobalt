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

#include "cobalt/layout_tests/layout_snapshot.h"

#include <memory>
#include <string>

#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cobalt/browser/client_hint_headers.h"
#include "cobalt/browser/user_agent_string.h"
#include "cobalt/browser/web_module.h"
#include "cobalt/cssom/viewport_size.h"
#include "cobalt/dom/screenshot_manager.h"
#include "cobalt/dom/window.h"
#include "cobalt/network/network_module.h"
#include "cobalt/render_tree/resource_provider.h"
#include "cobalt/web/web_settings.h"
#include "starboard/window.h"

using cobalt::cssom::ViewportSize;

namespace cobalt {
namespace layout_tests {

namespace {
void Quit(base::RunLoop* run_loop) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                run_loop->QuitClosure());
}

// Called when layout completes and results have been produced.  We use this
// signal to stop the WebModule's message loop since our work is done after a
// layout has been performed.
void WebModuleOnRenderTreeProducedCallback(
    base::Optional<browser::WebModule::LayoutResults>* out_results,
    base::RunLoop* run_loop, base::MessageLoop* message_loop,
    const browser::WebModule::LayoutResults& results) {
  out_results->emplace(results.render_tree, results.layout_time);
  message_loop->task_runner()->PostTask(FROM_HERE, base::Bind(Quit, run_loop));
}

// This callback, when called, quits a message loop, outputs the error message
// and sets the success flag to false.
void WebModuleErrorCallback(base::RunLoop* run_loop,
                            base::MessageLoop* message_loop, const GURL& url,
                            const std::string& error) {
  LOG(FATAL) << "Error loading document: " << error << ". URL: " << url;
  message_loop->task_runner()->PostTask(FROM_HERE, base::Bind(Quit, run_loop));
}
}  // namespace

browser::WebModule::LayoutResults SnapshotURL(
    const GURL& url, const ViewportSize& viewport_size,
    render_tree::ResourceProvider* resource_provider,
    const dom::ScreenshotManager::ProvideScreenshotFunctionCallback&
        screenshot_provider) {
  base::RunLoop run_loop;

  // Setup WebModule's auxiliary components.
  network::NetworkModule::Options net_options;
  // Some layout tests test Content Security Policy; allow HTTP so we
  // don't interfere.
  net_options.https_requirement = network::kHTTPSOptional;
  web::WebSettingsImpl web_settings;
  browser::UserAgentPlatformInfo platform_info;
  network::NetworkModule network_module(
      browser::CreateUserAgentString(platform_info),
      browser::GetClientHintHeaders(platform_info), nullptr, net_options);

  // Use 128M of image cache to minimize the effect of image loading.
  const size_t kImageCacheCapacity = 128 * 1024 * 1024;

  // Use test runner mode to allow the content itself to dictate when it is
  // ready for layout should be performed.  See cobalt/dom/test_runner.h.
  browser::WebModule::Options web_module_options;
  web_module_options.layout_trigger = layout::LayoutManager::kTestRunnerMode;
  web_module_options.image_cache_capacity = kImageCacheCapacity;
  web_module_options.provide_screenshot_function = screenshot_provider;
  // We assume that we won't suspend/resume while running the tests, and so
  // we take advantage of the convenience of inline script tags.
  web_module_options.enable_inline_script_warnings = false;

  web_module_options.web_options.web_settings = &web_settings;
  web_module_options.web_options.network_module = &network_module;

  // Prepare a slot for our results to be placed when ready.
  base::Optional<browser::WebModule::LayoutResults> results;

  // Create the WebModule and wait for a layout to occur.
  browser::WebModule web_module("SnapshotURL");
  web_module.Run(
      url, base::kApplicationStateStarted, nullptr /* scroll_engine */,
      base::Bind(&WebModuleOnRenderTreeProducedCallback, &results, &run_loop,
                 base::MessageLoop::current()),
      base::Bind(&WebModuleErrorCallback, &run_loop,
                 base::MessageLoop::current()),
      browser::WebModule::CloseCallback() /* window_close_callback */,
      base::Closure() /* window_minimize_callback */,
      NULL /* can_play_type_handler */, NULL /* web_media_player_factory */,
      viewport_size, resource_provider, 60.0f, web_module_options);

  run_loop.Run();

  // Return the results.
  return *results;
}

}  // namespace layout_tests
}  // namespace cobalt
