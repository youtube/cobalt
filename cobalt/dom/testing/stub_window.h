// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_DOM_TESTING_STUB_WINDOW_H_
#define COBALT_DOM_TESTING_STUB_WINDOW_H_

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/message_loop/message_loop.h"
#include "base/optional.h"
#include "base/threading/platform_thread.h"
#include "cobalt/base/debugger_hooks.h"
#include "cobalt/css_parser/parser.h"
#include "cobalt/cssom/viewport_size.h"
#include "cobalt/dom/dom_settings.h"
#include "cobalt/dom/local_storage_database.h"
#include "cobalt/dom/window.h"
#include "cobalt/dom_parser/parser.h"
#include "cobalt/loader/fetcher_factory.h"
#include "cobalt/loader/loader_factory.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/javascript_engine.h"
#include "starboard/window.h"
#include "url/gurl.h"

namespace cobalt {
namespace dom {
namespace testing {
// A helper class for tests that brings up a dom::Window with a number of parts
// stubbed out.
class StubWindow {
 public:
  explicit StubWindow(
      std::unique_ptr<script::EnvironmentSettings> environment_settings =
          std::unique_ptr<script::EnvironmentSettings>())
      : message_loop_(base::MessageLoop::TYPE_DEFAULT),
        css_parser_(css_parser::Parser::Create()),
        dom_parser_(
            new dom_parser::Parser(base::Bind(&StubLoadCompleteCallback))),
        fetcher_factory_(new loader::FetcherFactory(NULL)),
        loader_factory_(new loader::LoaderFactory(
            "Test", fetcher_factory_.get(), NULL, null_debugger_hooks_, 0,
            base::ThreadPriority::DEFAULT)),
        local_storage_database_(NULL),
        url_("about:blank"),
        dom_stat_tracker_(new dom::DomStatTracker("StubWindow")) {
    engine_ = script::JavaScriptEngine::CreateEngine();
    global_environment_ = engine_->CreateGlobalEnvironment();
    environment_settings_ =
        environment_settings.get()
            ? std::move(environment_settings)
            : std::unique_ptr<script::EnvironmentSettings>(new DOMSettings(
                  0, NULL, NULL, NULL, NULL, NULL, NULL, engine_.get(),
                  global_environment(), null_debugger_hooks_, NULL));
    window_ = new dom::Window(
        environment_settings_.get(), cssom::ViewportSize(1920, 1080),
        base::kApplicationStateStarted, css_parser_.get(), dom_parser_.get(),
        fetcher_factory_.get(), loader_factory_.get(), NULL, NULL, NULL, NULL,
        NULL, NULL, &local_storage_database_, NULL, NULL, NULL, NULL,
        global_environment_->script_value_factory(), NULL,
        dom_stat_tracker_.get(), url_, "", NULL, "en-US", "en",
        base::Callback<void(const GURL&)>(),
        base::Bind(&StubLoadCompleteCallback), NULL,
        network_bridge::PostSender(), csp::kCSPRequired,
        dom::kCspEnforcementEnable, base::Closure() /* csp_policy_changed */,
        base::Closure() /* ran_animation_frame_callbacks */,
        dom::Window::CloseCallback() /* window_close */,
        base::Closure() /* window_minimize */, NULL, NULL,
        dom::Window::OnStartDispatchEventCallback(),
        dom::Window::OnStopDispatchEventCallback(),
        dom::ScreenshotManager::ProvideScreenshotFunctionCallback(), NULL);
    base::polymorphic_downcast<dom::DOMSettings*>(environment_settings_.get())
        ->set_window(window_);
    global_environment_->CreateGlobalObject(window_,
                                            environment_settings_.get());
  }

  scoped_refptr<dom::Window> window() { return window_; }
  scoped_refptr<script::GlobalEnvironment> global_environment() {
    return global_environment_;
  }
  css_parser::Parser* css_parser() { return css_parser_.get(); }
  script::EnvironmentSettings* environment_settings() {
    return environment_settings_.get();
  }

  ~StubWindow() { window_->DestroyTimers(); }

 private:
  static void StubLoadCompleteCallback(
      const base::Optional<std::string>& error) {}

  base::MessageLoop message_loop_;
  base::NullDebuggerHooks null_debugger_hooks_;
  std::unique_ptr<css_parser::Parser> css_parser_;
  std::unique_ptr<dom_parser::Parser> dom_parser_;
  std::unique_ptr<loader::FetcherFactory> fetcher_factory_;
  std::unique_ptr<loader::LoaderFactory> loader_factory_;
  dom::LocalStorageDatabase local_storage_database_;
  GURL url_;
  std::unique_ptr<dom::DomStatTracker> dom_stat_tracker_;
  std::unique_ptr<script::EnvironmentSettings> environment_settings_;
  std::unique_ptr<script::JavaScriptEngine> engine_;
  scoped_refptr<script::GlobalEnvironment> global_environment_;
  scoped_refptr<dom::Window> window_;
};

}  // namespace testing
}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_TESTING_STUB_WINDOW_H_
