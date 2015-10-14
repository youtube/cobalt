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
#include "base/optional.h"
#include "base/stringprintf.h"
#include "cobalt/dom/storage.h"
#include "cobalt/h5vcc/h5vcc.h"
#include "cobalt/storage/storage_manager.h"

namespace cobalt {
namespace browser {

namespace {

// TODO(***REMOVED***): These numbers should be adjusted by the size of client memory.
const uint32 kImageCacheCapacity = 32U * 1024 * 1024;
const uint32 kRemoteFontCacheCapacity = 5U * 1024 * 1024;

// Prefix applied to local storage keys.
const char kLocalStoragePrefix[] = "cobalt.webModule";
}  // namespace

WebModule::WebModule(
    const GURL& initial_url,
    const OnRenderTreeProducedCallback& render_tree_produced_callback,
    const base::Callback<void(const std::string&)>& error_callback,
    media::MediaModule* media_module, network::NetworkModule* network_module,
    const math::Size& window_dimensions,
    render_tree::ResourceProvider* resource_provider, float layout_refresh_rate,
    const scoped_refptr<h5vcc::H5vcc>& h5vcc, const Options& options)
    : name_(options.name),
      // TODO(***REMOVED***) This assumes the web module runs in the message loop
      // current when it was created. If that changes, we must change this.
      self_message_loop_(MessageLoop::current()),
      css_parser_(css_parser::Parser::Create()),
      dom_parser_(new dom_parser::Parser(error_callback)),
      fetcher_factory_(new loader::FetcherFactory(network_module)),
      image_cache_(loader::image::CreateImageCache(
          base::StringPrintf("%s.ImageCache", name_.c_str()),
          kImageCacheCapacity, resource_provider, fetcher_factory_.get())),
      remote_font_cache_(loader::font::CreateRemoteFontCache(
          base::StringPrintf("%s.RemoteFontCache", name_.c_str()),
          kRemoteFontCacheCapacity, resource_provider, fetcher_factory_.get())),
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
          image_cache_.get(), remote_font_cache_.get(),
          &local_storage_database_, media_module, execution_state_.get(),
          script_runner_.get(), initial_url, network_module->user_agent(),
          network_module->preferred_language(), error_callback)),
      environment_settings_(new dom::DOMSettings(
          fetcher_factory_.get(), window_, javascript_engine_.get(),
          global_object_proxy_.get())),
      layout_manager_(window_.get(), resource_provider,
                      render_tree_produced_callback, css_parser_.get(),
                      options.layout_trigger, layout_refresh_rate) {
  global_object_proxy_->CreateGlobalObject(window_,
                                           environment_settings_.get());
  window_->set_debug_hub(options.debug_hub);
  window_->set_h5vcc(h5vcc);
}

WebModule::~WebModule() {}

void WebModule::InjectEvent(const scoped_refptr<dom::Event>& event) {
  // Repost to this web module's message loop if it was called on another.
  if (MessageLoop::current() != self_message_loop_) {
    self_message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&WebModule::InjectEvent, base::Unretained(this), event));
    return;
  }

  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT1("cobalt::browser", "WebModule::InjectEvent()",
               "type", event->type());
  window_->InjectEvent(event);
}

std::string WebModule::ExecuteJavascript(
    const std::string& script_utf8,
    const base::SourceLocation& script_location) {
  // If this method was called on a message loop different to this WebModule's,
  // post the task to this WebModule's message loop and wait for the result.
  if (MessageLoop::current() != self_message_loop_) {
    base::WaitableEvent got_result(true, false);
    std::string result;
    self_message_loop_->PostTask(
        FROM_HERE, base::Bind(&WebModule::ExecuteJavascriptInternal,
                              base::Unretained(this), script_utf8,
                              script_location, &got_result, &result));
    got_result.Wait();
    return result;
  }

  return script_runner_->Execute(script_utf8, script_location);
}

void WebModule::ExecuteJavascriptInternal(
    const std::string& script_utf8, const base::SourceLocation& script_location,
    base::WaitableEvent* got_result, std::string* result) {
  DCHECK(thread_checker_.CalledOnValidThread());
  *result = script_runner_->Execute(script_utf8, script_location);
  got_result->Signal();
}

std::string WebModule::GetItemInLocalStorage(const std::string& key) {
  std::string long_key = std::string(kLocalStoragePrefix) + "." + key;
  base::optional<std::string> result =
      window_->local_storage()->GetItem(long_key);
  if (result) {
    return result.value();
  }
  // Key wasn't found.
  DLOG(WARNING) << "Key \"" << long_key << "\" not found in local storage.";
  return "";
}

void WebModule::SetItemInLocalStorage(const std::string& key,
                                      const std::string& value) {
  std::string long_key = std::string(kLocalStoragePrefix) + "." + key;
  window_->local_storage()->SetItem(long_key, value);
}

}  // namespace browser
}  // namespace cobalt
