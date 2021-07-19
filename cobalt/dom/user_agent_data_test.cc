// Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/optional.h"
#include "cobalt/bindings/testing/utils.h"
#include "cobalt/browser/user_agent_platform_info.h"
#include "cobalt/css_parser/parser.h"
#include "cobalt/cssom/viewport_size.h"
#include "cobalt/dom/global_stats.h"
#include "cobalt/dom/local_storage_database.h"
#include "cobalt/dom/navigator.h"
#include "cobalt/dom/navigator_ua_data.h"
#include "cobalt/dom/testing/gtest_workarounds.h"
#include "cobalt/dom/testing/stub_environment_settings.h"
#include "cobalt/dom/window.h"
#include "cobalt/dom_parser/parser.h"
#include "cobalt/h5vcc/h5vcc.h"
#include "cobalt/loader/fetcher_factory.h"
#include "cobalt/loader/loader_factory.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/javascript_engine.h"
#include "cobalt/script/source_code.h"
#include "testing/gtest/include/gtest/gtest.h"

using cobalt::cssom::ViewportSize;

namespace cobalt {
namespace dom {

namespace {

class UserAgentDataTest : public ::testing::Test {
 public:
  UserAgentDataTest()
      : environment_settings_(new testing::StubEnvironmentSettings),
        css_parser_(css_parser::Parser::Create()),
        dom_parser_(
            new dom_parser::Parser(base::Bind(&StubLoadCompleteCallback))),
        fetcher_factory_(new loader::FetcherFactory(NULL)),
        loader_factory_(new loader::LoaderFactory(
            "Test", fetcher_factory_.get(), NULL, null_debugger_hooks_, 0,
            base::ThreadPriority::DEFAULT)),
        local_storage_database_(NULL),
        url_("about:blank"),
        engine_(script::JavaScriptEngine::CreateEngine()),
        global_environment_(engine_->CreateGlobalEnvironment()) {
    InitializeEmptyPlatformInfo();
    window_ = new Window(
        environment_settings_.get(), ViewportSize(1920, 1080),
        base::kApplicationStateStarted, css_parser_.get(), dom_parser_.get(),
        fetcher_factory_.get(), loader_factory_.get(), NULL, NULL, NULL, NULL,
        NULL, NULL, &local_storage_database_, NULL, NULL, NULL, NULL,
        global_environment_->script_value_factory() /* script_value_factory */,
        NULL, NULL, url_, "", platform_info_.get(), "en-US", "en",
        base::Callback<void(const GURL&)>(),
        base::Bind(&StubLoadCompleteCallback), NULL,
        network_bridge::PostSender(), csp::kCSPRequired, kCspEnforcementEnable,
        base::Closure() /* csp_policy_changed */,
        base::Closure() /* ran_animation_frame_callbacks */,
        dom::Window::CloseCallback() /* window_close */,
        base::Closure() /* window_minimize */, NULL, NULL,
        dom::Window::OnStartDispatchEventCallback(),
        dom::Window::OnStopDispatchEventCallback(),
        dom::ScreenshotManager::ProvideScreenshotFunctionCallback(), NULL);

    // Make Navigator accessible via Window
    global_environment_->CreateGlobalObject(window_,
                                            environment_settings_.get());

    // Inject H5vcc interface to make it also accessible via Window
    h5vcc::H5vcc::Settings h5vcc_settings;
    h5vcc_settings.media_module = NULL;
    h5vcc_settings.network_module = NULL;
#if SB_IS(EVERGREEN)
    h5vcc_settings.updater_module = NULL;
#endif
    h5vcc_settings.account_manager = NULL;
    h5vcc_settings.event_dispatcher = &event_dispatcher_;
    h5vcc_settings.user_agent_data = window_->navigator()->user_agent_data();
    h5vcc_settings.global_environment = global_environment_;
    global_environment_->Bind("h5vcc", scoped_refptr<script::Wrappable>(
                                           new h5vcc::H5vcc(h5vcc_settings)));
  }

  ~UserAgentDataTest() {
    global_environment_->SetReportEvalCallback(base::Closure());
    global_environment_->SetReportErrorCallback(
        script::GlobalEnvironment::ReportErrorCallback());
    window_->DispatchEvent(new dom::Event(base::Tokens::unload()));

    window_ = nullptr;
    global_environment_ = nullptr;
    EXPECT_TRUE(GlobalStats::GetInstance()->CheckNoLeaks());
  }

  bool EvaluateScript(const std::string& js_code, std::string* result);

  void SetUp() override;

 private:
  static void StubLoadCompleteCallback(
      const base::Optional<std::string>& error) {}
  const std::unique_ptr<testing::StubEnvironmentSettings> environment_settings_;
  base::NullDebuggerHooks null_debugger_hooks_;
  base::MessageLoop message_loop_;
  std::unique_ptr<css_parser::Parser> css_parser_;
  std::unique_ptr<dom_parser::Parser> dom_parser_;
  std::unique_ptr<loader::FetcherFactory> fetcher_factory_;
  std::unique_ptr<loader::LoaderFactory> loader_factory_;
  dom::LocalStorageDatabase local_storage_database_;
  GURL url_;
  std::unique_ptr<script::JavaScriptEngine> engine_;
  base::EventDispatcher event_dispatcher_;
  scoped_refptr<script::GlobalEnvironment> global_environment_;
  std::unique_ptr<browser::UserAgentPlatformInfo> platform_info_;
  scoped_refptr<Window> window_;

  void InitializeEmptyPlatformInfo();
};

bool UserAgentDataTest::EvaluateScript(const std::string& js_code,
                                       std::string* result) {
  DCHECK(global_environment_);
  scoped_refptr<script::SourceCode> source_code =
      script::SourceCode::CreateSourceCode(
          js_code, base::SourceLocation(__FILE__, __LINE__, 1));

  global_environment_->EnableEval();
  global_environment_->SetReportEvalCallback(base::Closure());
  bool succeeded = global_environment_->EvaluateScript(source_code, result);
  return succeeded;
}

void UserAgentDataTest::SetUp() {
  // Let's first check that the Web API is by default disabled, and that the
  // switch to turn it on works
  std::string result;
  EXPECT_TRUE(EvaluateScript("navigator.userAgentData", &result));
  EXPECT_FALSE(bindings::testing::IsAcceptablePrototypeString("NavigatorUAData",
                                                              result));

  EXPECT_TRUE(
      EvaluateScript("h5vcc.settings.set('NavigatorUAData', 1)", &result));
  EXPECT_EQ("true", result);

  EXPECT_TRUE(EvaluateScript("navigator.userAgentData", &result));
  EXPECT_TRUE(bindings::testing::IsAcceptablePrototypeString("NavigatorUAData",
                                                             result));
}

void UserAgentDataTest::InitializeEmptyPlatformInfo() {
  platform_info_.reset(new browser::UserAgentPlatformInfo());

  platform_info_->set_starboard_version("");
  platform_info_->set_os_name_and_version("");
  platform_info_->set_original_design_manufacturer("");
  platform_info_->set_device_type(kSbSystemDeviceTypeUnknown);
  platform_info_->set_chipset_model_number("");
  platform_info_->set_model_year("");
  platform_info_->set_firmware_version("");
  platform_info_->set_brand("");
  platform_info_->set_model("");
  platform_info_->set_aux_field("");
  platform_info_->set_connection_type(base::nullopt);
  platform_info_->set_javascript_engine_version("");
  platform_info_->set_rasterizer_type("");
  platform_info_->set_evergreen_version("");
  platform_info_->set_cobalt_version("");
  platform_info_->set_cobalt_build_version_number("");
  platform_info_->set_build_configuration("");
}

}  // namespace

TEST_F(UserAgentDataTest, Brands) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("navigator.userAgentData.brands", &result));
  EXPECT_TRUE(bindings::testing::IsAcceptablePrototypeString("Object", result));
}

TEST_F(UserAgentDataTest, Mobile) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("navigator.userAgentData.mobile", &result));
  EXPECT_EQ("false", result);
}

TEST_F(UserAgentDataTest, GetHighEntropyValues) {
  std::string result;
  EXPECT_TRUE(
      EvaluateScript("navigator.userAgentData.getHighEntropyValues", &result));
  EXPECT_PRED_FORMAT2(::testing::IsSubstring, "function getHighEntropyValues()",
                      result.c_str());
  EXPECT_TRUE(EvaluateScript("navigator.userAgentData.getHighEntropyValues([])",
                             &result));
  EXPECT_TRUE(
      bindings::testing::IsAcceptablePrototypeString("Promise", result));
}

}  // namespace dom
}  // namespace cobalt
