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

#include "cobalt/browser/web_module.h"

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "cobalt/storage/storage_manager.h"

namespace cobalt {
namespace browser {

WebModule::WebModule(
    const GURL& initial_url,
    const OnRenderTreeProducedCallback& render_tree_produced_callback,
    const base::Callback<void(const std::string&)>& error_callback,
    media::MediaModule* media_module, network::NetworkModule* network_module,
    const math::Size& window_dimensions,
    render_tree::ResourceProvider* resource_provider, float layout_refresh_rate,
    const Options& options)
    : name_(options.name),
      css_parser_(css_parser::Parser::Create()),
      dom_parser_(new dom_parser::Parser(error_callback)),
      fetcher_factory_(new loader::FetcherFactory(network_module)),
      image_cache_(new loader::ImageCache(
          base::StringPrintf("%s.ImageCache", name_.c_str()), resource_provider,
          fetcher_factory_.get())),
      local_storage_database_(network_module->storage_manager()),
      javascript_engine_(script::JavaScriptEngine::CreateEngine()),
      global_object_proxy_(javascript_engine_->CreateGlobalObjectProxy()),
      execution_state_(
          script::ExecutionState::CreateExecutionState(global_object_proxy_)),
      script_runner_(
          script::ScriptRunner::CreateScriptRunner(global_object_proxy_)),
      window_(new dom::Window(
          window_dimensions.width(), window_dimensions.height(),
          css_parser_.get(), dom_parser_.get(), fetcher_factory_.get(),
          image_cache_.get(), &local_storage_database_, media_module,
          execution_state_.get(), script_runner_.get(), initial_url,
          GetUserAgent(), error_callback)),
      environment_settings_(new dom::DOMSettings(
          fetcher_factory_.get(), window_, javascript_engine_.get(),
          global_object_proxy_.get())),
      layout_manager_(window_.get(), resource_provider,
                      render_tree_produced_callback, css_parser_.get(),
                      options.layout_trigger, layout_refresh_rate) {
  global_object_proxy_->CreateGlobalObject(window_,
                                           environment_settings_.get());
  window_->set_debug_hub(options.debug_hub);
}

WebModule::~WebModule() {}

void WebModule::InjectEvent(const scoped_refptr<dom::Event>& event) {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT1("cobalt::browser", "WebModule::InjectEvent()",
               "type", event->type());
  window_->InjectEvent(event);
}

std::string WebModule::GetUserAgent() const {
#if defined(__LB_LINUX__)
  const char kVendor[] = "NoVendor";
  const char kPlatform[] = "Linux";
#elif defined(__LB_PS3__)
  const char kVendor[] = "Sony";
  const char kPlatform[] = "PS3";
#elif defined(COBALT_WIN)
  const char kVendor[] = "Microsoft";
  const char kPlatform[] = "Windows";
#else
#error Undefined platform
#endif

#if defined(COBALT_BUILD_TYPE_DEBUG)
  const char kBuildType[] = "Debug";
#elif defined(COBALT_BUILD_TYPE_DEVEL)
  const char kBuildType[] = "Devel";
#elif defined(COBALT_BUILD_TYPE_QA)
  const char kBuildType[] = "QA";
#elif defined(COBALT_BUILD_TYPE_GOLD)
  const char kBuildType[] = "Gold";
#else
#error Unknown build type
#endif

  const char kChromiumVersion[] = "25.0.1364.70";
  const char kCobaltVersion[] = "0.01";
  const char kLanguageCode[] = "en";
  const char kCountryCode[] = "US";

  std::string user_agent;
  std::string product =
      base::StringPrintf("Chrome/%s Cobalt/%s %s build", kChromiumVersion,
                         kCobaltVersion, kBuildType);
  user_agent.append(base::StringPrintf(
      "Mozilla/5.0 (%s) (KHTML, like Gecko) %s", kPlatform, product.c_str()));

  std::string vendor = kVendor;
  std::string device = kPlatform;
  std::string firmware_version = "";  // Okay to leave blank
  std::string model = kPlatform;
  std::string sku = "";  // Okay to leave blank
  std::string language_code = kLanguageCode;
  std::string country_code = kCountryCode;
  user_agent.append(base::StringPrintf(
      " %s %s/%s (%s, %s, %s, %s)", vendor.c_str(), device.c_str(),
      firmware_version.c_str(), model.c_str(), sku.c_str(),
      language_code.c_str(), country_code.c_str()));

  return user_agent;
}

}  // namespace browser
}  // namespace cobalt
