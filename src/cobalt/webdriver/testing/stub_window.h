/*
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef COBALT_WEBDRIVER_TESTING_STUB_WINDOW_H_
#define COBALT_WEBDRIVER_TESTING_STUB_WINDOW_H_

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/message_loop.h"
#include "cobalt/css_parser/parser.h"
#include "cobalt/dom/local_storage_database.h"
#include "cobalt/dom/window.h"
#include "cobalt/dom_parser/parser.h"
#include "cobalt/loader/fetcher_factory.h"
#include "cobalt/media/media_module_stub.h"
#include "cobalt/network/network_module.h"
#include "googleurl/src/gurl.h"

namespace cobalt {
namespace webdriver {
namespace testing {

// A helper class for WebDriver tests that brings up a dom::Window with a number
// of parts stubbed out.
class StubWindow {
 public:
  StubWindow()
      : message_loop_(MessageLoop::TYPE_DEFAULT),
        css_parser_(css_parser::Parser::Create()),
        dom_parser_(new dom_parser::Parser(base::Bind(&StubErrorCallback))),
        fetcher_factory_(new loader::FetcherFactory(&network_module_)),
        local_storage_database_(NULL),
        stub_media_module_(new media::MediaModuleStub()),
        url_("about:blank"),
        dom_stat_tracker_(new dom::DomStatTracker("StubWindow")) {
    engine_ = script::JavaScriptEngine::CreateEngine();
    global_environment_ = engine_->CreateGlobalEnvironment();
    window_ = new dom::Window(
        1920, 1080, css_parser_.get(), dom_parser_.get(),
        fetcher_factory_.get(), NULL, NULL, NULL, NULL,
        &local_storage_database_, stub_media_module_.get(),
        stub_media_module_.get(), NULL, NULL, NULL, dom_stat_tracker_.get(),
        url_, "", "en-US", base::Callback<void(const GURL&)>(),
        base::Bind(&StubErrorCallback), NULL, network_bridge::PostSender(),
        std::string() /* default security policy */, dom::kCspEnforcementEnable,
        base::Closure() /* csp_policy_changed */,
        base::Closure() /* window_close */);
    global_environment_->CreateGlobalObject(window_, &environment_settings_);
  }

  scoped_refptr<dom::Window> window() { return window_; }
  scoped_refptr<script::GlobalEnvironment> global_environment() {
    return global_environment_;
  }

 private:
  static void StubErrorCallback(const std::string& /*error*/) {}

  MessageLoop message_loop_;
  scoped_ptr<css_parser::Parser> css_parser_;
  scoped_ptr<dom_parser::Parser> dom_parser_;
  network::NetworkModule network_module_;
  scoped_ptr<loader::FetcherFactory> fetcher_factory_;
  dom::LocalStorageDatabase local_storage_database_;
  scoped_ptr<media::MediaModule> stub_media_module_;
  GURL url_;
  scoped_ptr<dom::DomStatTracker> dom_stat_tracker_;
  script::EnvironmentSettings environment_settings_;
  scoped_ptr<script::JavaScriptEngine> engine_;
  scoped_refptr<script::GlobalEnvironment> global_environment_;
  scoped_refptr<dom::Window> window_;
};

}  // namespace testing
}  // namespace webdriver
}  // namespace cobalt

#endif  // COBALT_WEBDRIVER_TESTING_STUB_WINDOW_H_
