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
#include "cobalt/bindings/testing/utils.h"
#include "cobalt/browser/user_agent_platform_info.h"
#include "cobalt/dom/navigator.h"
#include "cobalt/dom/testing/stub_window.h"
#include "cobalt/dom/testing/test_with_javascript.h"
#include "cobalt/h5vcc/h5vcc.h"
#include "cobalt/web/navigator_ua_data.h"
#include "cobalt/web/testing/gtest_workarounds.h"
#include "cobalt/web/testing/stub_web_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace dom {

namespace {

class UserAgentDataTest : public testing::TestWithJavaScript {
 public:
  UserAgentDataTest() {
    InitializeEmptyPlatformInfo();

    stub_web_context()->set_platform_info(platform_info_.get());

    // Inject H5vcc interface to make it also accessible via Window
    h5vcc::H5vcc::Settings h5vcc_settings;
    h5vcc_settings.network_module = NULL;
#if SB_IS(EVERGREEN)
    h5vcc_settings.updater_module = NULL;
#endif
    h5vcc_settings.account_manager = NULL;
    h5vcc_settings.event_dispatcher = event_dispatcher();
    h5vcc_settings.user_agent_data = window()->navigator()->user_agent_data();
    h5vcc_settings.global_environment = global_environment();
    global_environment()->Bind("h5vcc", scoped_refptr<script::Wrappable>(
                                            new h5vcc::H5vcc(h5vcc_settings)));
  }

  ~UserAgentDataTest() {}

  void SetUp() override;

 private:
  static void StubLoadCompleteCallback(
      const base::Optional<std::string>& error) {}
  std::unique_ptr<browser::UserAgentPlatformInfo> platform_info_;

  void InitializeEmptyPlatformInfo();
};

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
