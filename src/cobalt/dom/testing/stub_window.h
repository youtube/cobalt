// Copyright 2016 Google Inc. All Rights Reserved.
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

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/message_loop.h"
#include "base/threading/platform_thread.h"
#include "cobalt/css_parser/parser.h"
#include "cobalt/dom/local_storage_database.h"
#include "cobalt/dom/window.h"
#include "cobalt/dom_parser/parser.h"
#include "cobalt/loader/fetcher_factory.h"
#include "cobalt/loader/loader_factory.h"
#include "cobalt/media_session/media_session.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/javascript_engine.h"
#include "googleurl/src/gurl.h"
#include "starboard/window.h"

namespace cobalt {
namespace dom {
namespace testing {
// A helper class for tests that brings up a dom::Window with a number of parts
// stubbed out.
class StubWindow {
 public:
  explicit StubWindow(
      scoped_ptr<script::EnvironmentSettings> environment_settings =
          scoped_ptr<script::EnvironmentSettings>(
              new script::EnvironmentSettings))
      : message_loop_(MessageLoop::TYPE_DEFAULT),
        css_parser_(css_parser::Parser::Create()),
        dom_parser_(new dom_parser::Parser(base::Bind(&StubErrorCallback))),
        fetcher_factory_(new loader::FetcherFactory(NULL)),
        loader_factory_(new loader::LoaderFactory(
            fetcher_factory_.get(), NULL, base::kThreadPriority_Default)),
        local_storage_database_(NULL),
        url_("about:blank"),
        dom_stat_tracker_(new dom::DomStatTracker("StubWindow")),
        environment_settings_(environment_settings.Pass()) {
    engine_ = script::JavaScriptEngine::CreateEngine();
    global_environment_ = engine_->CreateGlobalEnvironment();
    window_ = new dom::Window(
        1920, 1080, 1.f, base::kApplicationStateStarted, css_parser_.get(),
        dom_parser_.get(), fetcher_factory_.get(), loader_factory_.get(), NULL,
        NULL, NULL, NULL, NULL, NULL, &local_storage_database_, NULL, NULL,
        NULL, NULL, global_environment_->script_value_factory(), NULL,
        dom_stat_tracker_.get(), url_, "", "en-US", "en",
        base::Callback<void(const GURL&)>(), base::Bind(&StubErrorCallback),
        NULL, network_bridge::PostSender(), csp::kCSPRequired,
        dom::kCspEnforcementEnable, base::Closure() /* csp_policy_changed */,
        base::Closure() /* ran_animation_frame_callbacks */,
        dom::Window::CloseCallback() /* window_close */,
        base::Closure() /* window_minimize */, NULL, NULL, NULL,
        dom::Window::OnStartDispatchEventCallback(),
        dom::Window::OnStopDispatchEventCallback(),
        dom::ScreenshotManager::ProvideScreenshotFunctionCallback(), NULL);
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
  static void StubErrorCallback(const std::string& /*error*/) {}

  MessageLoop message_loop_;
  scoped_ptr<css_parser::Parser> css_parser_;
  scoped_ptr<dom_parser::Parser> dom_parser_;
  scoped_ptr<loader::FetcherFactory> fetcher_factory_;
  scoped_ptr<loader::LoaderFactory> loader_factory_;
  dom::LocalStorageDatabase local_storage_database_;
  GURL url_;
  scoped_ptr<dom::DomStatTracker> dom_stat_tracker_;
  scoped_ptr<script::EnvironmentSettings> environment_settings_;
  scoped_ptr<script::JavaScriptEngine> engine_;
  scoped_refptr<script::GlobalEnvironment> global_environment_;
  scoped_refptr<dom::Window> window_;
};

}  // namespace testing
}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_TESTING_STUB_WINDOW_H_
