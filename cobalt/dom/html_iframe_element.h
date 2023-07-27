// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_DOM_HTML_IFRAME_ELEMENT_H_
#define COBALT_DOM_HTML_IFRAME_ELEMENT_H_

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_checker.h"
#include "cobalt/dom/html_element.h"
#include "cobalt/dom/window.h"
#include "cobalt/dom/window_proxy.h"
#include "cobalt/script/v8c/entry_scope.h"
#include "cobalt/web/agent.h"
#include "cobalt/web/window_or_worker_global_scope.h"
#include "url/gurl.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace cobalt {
namespace dom {

class Document;
// class Window;

// expose create frame registration
class Frame {
 public:
  Frame(web::Agent* web_agent, Window* parent)
      : web_agent_(web_agent), parent_(parent) {
    // new WebModule
    //   creates web agent (web context and has environment_settings)
    // web_module.Run()
    //   creates window

    //       WebModule::Options options(options_.web_module_options);
    //   options.splash_screen_cache = splash_screen_cache_.get();
    //   options.navigation_callback =
    //       base::Bind(&BrowserModule::Navigate, base::Unretained(this));
    //   options.loaded_callbacks.push_back(
    //       base::Bind(&BrowserModule::OnLoad, base::Unretained(this)));
    // #if defined(ENABLE_FAKE_MICROPHONE)
    //   if (base::CommandLine::ForCurrentProcess()->HasSwitch(
    //           switches::kFakeMicrophone)) {
    //     options.dom_settings_options.microphone_options.enable_fake_microphone
    //     =
    //         true;
    //   }
    // #endif  // defined(ENABLE_FAKE_MICROPHONE)
    //   if (on_screen_keyboard_bridge_) {
    //     options.on_screen_keyboard_bridge = on_screen_keyboard_bridge_.get();
    //   }
    //   options.image_cache_capacity_multiplier_when_playing_video =
    //       configuration::Configuration::GetInstance()
    //           ->CobaltImageCacheCapacityMultiplierWhenPlayingVideo();
    //   if (input_device_manager_) {
    //     options.camera_3d = input_device_manager_->camera_3d();
    //   }

    //   options.provide_screenshot_function =
    //       base::Bind(&ScreenShotWriter::RequestScreenshotToMemoryUnencoded,
    //                  base::Unretained(screen_shot_writer_.get()));

    // #if defined(ENABLE_DEBUGGER)
    //   if (base::CommandLine::ForCurrentProcess()->HasSwitch(
    //           switches::kWaitForWebDebugger)) {
    //     int wait_for_generation =
    //         atoi(base::CommandLine::ForCurrentProcess()
    //                  ->GetSwitchValueASCII(switches::kWaitForWebDebugger)
    //                  .c_str());
    //     if (wait_for_generation < 1) wait_for_generation = 1;
    //     options.wait_for_web_debugger =
    //         (wait_for_generation == main_web_module_generation_);
    //   }

    //   options.debugger_state = debugger_state_.get();
    // #endif  // ENABLE_DEBUGGER

    //   // Pass down this callback from to Web module.
    //   options.maybe_freeze_callback =
    //       base::Bind(&BrowserModule::OnMaybeFreeze, base::Unretained(this));

    //   options.web_options.web_settings = &web_settings_;
    //   options.web_options.network_module = network_module_;
    //   options.web_options.service_worker_context =
    //       service_worker_registry_->service_worker_context();
    //   options.web_options.platform_info = platform_info_.get();
    //   web_module_.reset(new WebModule("MainWebModule"));
    // // Wait for service worker to start if one exists.
    // if (can_start_service_worker) {
    //   service_worker_started_event->Wait();
    // }
    // web_module_->Run(
    //     url, application_state_, scroll_engine_.get(),
    //     base::Bind(&BrowserModule::QueueOnRenderTreeProduced,
    //                base::Unretained(this), main_web_module_generation_),
    //     base::Bind(&BrowserModule::OnError, base::Unretained(this)),
    //     base::Bind(&BrowserModule::OnWindowClose, base::Unretained(this)),
    //     base::Bind(&BrowserModule::OnWindowMinimize, base::Unretained(this)),
    //     can_play_type_handler_.get(), media_module_.get(), viewport_size,
    //     GetResourceProvider(), kLayoutMaxRefreshFrequencyInHz, options);

    // loader_factory_.reset(new loader::LoaderFactory(
    //   web_context_->name().c_str(), web_context_->fetcher_factory(),
    //   resource_provider_, debugger_hooks_,
    //   data.options.encoded_image_cache_capacity,
    //   data.options.loader_thread_priority));

    // web_context_->SetupEnvironmentSettings(new dom::DOMSettings(
    //   debugger_hooks_, kDOMMaxElementDepth, media_source_registry_.get(),
    //   data.can_play_type_handler, memory_info,
    //   &mutation_observer_task_manager_, data.options.dom_settings_options));
    // web_context_->environment_settings()->set_creation_url(data.initial_url);
    // return web_context_
  }

  Window* window() const {
    // if (web_agent_) {
    //   LOG(WARNING) << "has web_agent_";
    //   if (web_agent_->context()) {
    //     LOG(WARNING) << "has web::context";
    //     if (web_agent_->context()->GetWindowOrWorkerGlobalScope()) {
    //       LOG(WARNING) << "has GetWindowOrWorkerGlobalScope";
    //       if
    //       (web_agent_->context()->GetWindowOrWorkerGlobalScope()->AsWindow())
    //       {
    //         LOG(WARNING) << "is window";
    //       }
    //     }
    //   }
    // }
    script::v8c::EntryScope entry_scope(parent_->environment_settings()
                                            ->context()
                                            ->global_environment()
                                            ->isolate());
    return web_agent_->context()->GetWindowOrWorkerGlobalScope()->AsWindow();
  }

  Window* parent() const { return parent_; }

  WindowProxy* window_proxy() {
    script::v8c::EntryScope entry_scope(parent_->environment_settings()
                                            ->context()
                                            ->global_environment()
                                            ->isolate());
    if (!window_proxy_) {
      auto* settings = web_agent_->context()->environment_settings();
      auto* window =
          web_agent_->context()->GetWindowOrWorkerGlobalScope()->AsWindow();
      window_proxy_ = new WindowProxy(settings, window, parent_);
    }
    return window_proxy_;
  }

 private:
  web::Agent* web_agent_;
  scoped_refptr<WindowProxy> window_proxy_;
  Window* parent_;
};

class FrameFactory {
 public:
  typedef base::RepeatingCallback<web::Agent*(const std::string&, const GURL&,
                                              base::Closure)>
      CreateAgentCallback;
  typedef base::RepeatingCallback<dom::Window*()> GetParentWindowCallback;
  static FrameFactory* GetInstance();

  std::unique_ptr<Frame> CreateFrame(const GURL& url,
                                     base::Closure on_loaded_callback) {
    if (!create_agent_callback_) {
      return nullptr;
    }

    std::string name = base::StringPrintf("IFrame[%d]", ++count);
    LOG(WARNING) << "Creating " << name;
    Window* parent = get_parent_window_callback_.Run();
    LOG(WARNING) << "Got parent for iFrame creation";
    return std::make_unique<Frame>(
        create_agent_callback_.Run(name, url, std::move(on_loaded_callback)),
        parent);
  }

  void set_create_agent_callback(CreateAgentCallback create_agent_callback) {
    create_agent_callback_ = std::move(create_agent_callback);
  }

  void set_get_parent_window_callback(
      GetParentWindowCallback get_parent_window_callback) {
    get_parent_window_callback_ = std::move(get_parent_window_callback);
  }

 private:
  friend struct base::DefaultSingletonTraits<FrameFactory>;
  FrameFactory() = default;

  CreateAgentCallback create_agent_callback_;
  GetParentWindowCallback get_parent_window_callback_;
  static int count;
};

class HTMLIframeElement : public HTMLElement {
 public:
  static const char kTagName[];

  explicit HTMLIframeElement(Document* document);

  std::string src() const;
  void set_src(const std::string& value) { SetAttribute("src", value); }

  std::string name() const { return GetAttribute("name").value_or(""); }
  void set_name(const std::string& value) { SetAttribute("name", value); }

  std::string loading() const { return GetAttribute("loading").value_or(""); }
  void set_loading(const std::string& value) { SetAttribute("loading", value); }

  Document* content_document() const {
    return content_frame_->window()->document();
  }

  WindowProxy* content_window() const {
    script::v8c::EntryScope entry_scope(content_frame_->parent()
                                            ->environment_settings()
                                            ->context()
                                            ->global_environment()
                                            ->isolate());
    // if (!content_frame_) {
    //   return nullptr;
    // }
    LOG(WARNING) << "window ptr: " << content_frame_->window();

    return content_frame_->window_proxy();
  }

  // Custom, not in any spec.
  //
  // From Node.
  void OnInsertedIntoDocument() override;
  void OnRemovedFromDocument() override;

  // From Element.
  void OnParserStartTag(
      const base::SourceLocation& opening_tag_location) override;
  void OnParserEndTag() override;

  // From HTMLElement.
  scoped_refptr<HTMLIframeElement> AsHTMLIframeElement() override;

  // Create Performance Resource Timing entry for script element.
  void GetLoadTimingInfoAndCreateResourceTiming();

  DEFINE_WRAPPABLE_TYPE(HTMLIframeElement);

 private:
  ~HTMLIframeElement() override;

  void NavigateFrame(const GURL& url);
  void ProcessAttributes();

  THREAD_CHECKER(thread_checker_);
  std::unique_ptr<Frame> content_frame_;
  bool initial_insertion_;
  // scoped_refptr<Document> document_;
  // scoped_refptr<Window> window_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_HTML_IFRAME_ELEMENT_H_
