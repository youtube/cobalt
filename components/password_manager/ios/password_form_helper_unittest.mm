// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/password_manager/ios/password_form_helper.h"

#include <stddef.h>

#include "base/mac/bundle_locations.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/ios/wait_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#import "components/autofill/ios/form_util/autofill_test_with_web_state.h"
#import "components/autofill/ios/form_util/form_handlers_java_script_feature.h"
#import "components/autofill/ios/form_util/form_util_java_script_feature.h"
#include "components/autofill/ios/form_util/unique_id_data_tab_helper.h"
#include "components/password_manager/ios/account_select_fill_data.h"
#include "components/password_manager/ios/password_manager_java_script_feature.h"
#include "components/password_manager/ios/test_helpers.h"
#include "components/ukm/ios/ukm_url_recorder.h"
#include "components/ukm/test_ukm_recorder.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#include "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NS_ASSUME_NONNULL_BEGIN

using autofill::FieldRendererId;
using autofill::FormData;
using autofill::FormRendererId;
using autofill::PasswordFormFillData;
using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;
using password_manager::FillData;
using test_helpers::SetPasswordFormFillData;
using test_helpers::SetFillData;
using test_helpers::SetFormData;

namespace {
class PasswordFormHelperTest : public AutofillTestWithWebState {
 public:
  PasswordFormHelperTest()
      : AutofillTestWithWebState(std::make_unique<web::FakeWebClient>()) {
    web::FakeWebClient* web_client =
        static_cast<web::FakeWebClient*>(GetWebClient());
    web_client->SetJavaScriptFeatures(
        {autofill::FormHandlersJavaScriptFeature::GetInstance(),
         autofill::FormUtilJavaScriptFeature::GetInstance(),
         password_manager::PasswordManagerJavaScriptFeature::GetInstance()});
  }

  PasswordFormHelperTest(const PasswordFormHelperTest&) = delete;
  PasswordFormHelperTest& operator=(const PasswordFormHelperTest&) = delete;

  ~PasswordFormHelperTest() override = default;

  void SetUp() override {
    WebTestWithWebState::SetUp();
    UniqueIDDataTabHelper::CreateForWebState(web_state());
    helper_ = [[PasswordFormHelper alloc] initWithWebState:web_state()];
    ukm::InitializeSourceUrlRecorderForWebState(web_state());
  }

  void TearDown() override {
    WaitForBackgroundTasks();
    helper_ = nil;
    web::WebTestWithWebState::TearDown();
  }

  web::WebFrame* GetMainFrame() {
    password_manager::PasswordManagerJavaScriptFeature* feature =
        password_manager::PasswordManagerJavaScriptFeature::GetInstance();
    return feature->GetWebFramesManager(web_state())->GetMainWebFrame();
  }

  // Sets up unique form ids and returns true if successful.
  bool SetUpUniqueIDs() {
    __block web::WebFrame* main_frame = nullptr;
    bool success =
        WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
          main_frame = GetMainFrame();
          return main_frame != nullptr;
        });
    if (!success) {
      return false;
    }
    DCHECK(main_frame);
    SetUpForUniqueIds(main_frame);

    if (!success) {
      return false;
    }

    // Run password forms search to set up unique IDs.
    __block bool complete = false;
    password_manager::PasswordManagerJavaScriptFeature::GetInstance()
        ->FindPasswordFormsInFrame(main_frame,
                                   base::BindOnce(^(NSString* forms) {
                                     complete = true;
                                   }));

    return WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
      return complete;
    });
  }

 protected:
  // PasswordFormHelper for testing.
  PasswordFormHelper* helper_;
};

struct FindPasswordFormTestData {
  // HTML String of the form.
  NSString* html_string;
  // True if expected to find the form.
  const bool expected_form_found;
  // Expected number of fields in found form.
  const size_t expected_number_of_fields;
  // Expected form name.
  const char* expected_form_name;
};

// Check that HTML forms are converted correctly into PasswordForms.
TEST_F(PasswordFormHelperTest, FindPasswordFormsInView) {
  // clang-format off
  const FindPasswordFormTestData test_data[] = {
    // Normal form: a username and a password element.
    {
      @"<form name='form1'>"
      "<input type='text' name='user0'>"
      "<input type='password' name='pass0'>"
      "</form>",
      true, 2, "form1"
    },
    // User name is captured as an email address (HTML5).
    {
      @"<form name='form1'>"
      "<input type='email' name='email1'>"
      "<input type='password' name='pass1'>"
      "</form>",
      true, 2, "form1"
    },
    // No form found.
    {
      @"<div>",
      false, 0, nullptr
    },
    // Disabled username element.
    {
      @"<form name='form1'>"
      "<input type='text' name='user2' disabled='disabled'>"
      "<input type='password' name='pass2'>"
      "</form>",
      true, 2, "form1"
    },
    // No password element.
    {
      @"<form name='form1'>"
      "<input type='text' name='user3'>"
      "</form>",
      false, 0, nullptr
    },
    // No <form> tag.
    {
      @"<input type='email' name='email1'>"
      "<input type='password' name='pass1'>",
      true, 2, ""
    },
  };
  // clang-format on

  for (const FindPasswordFormTestData& data : test_data) {
    SCOPED_TRACE(testing::Message()
                 << "for html_string="
                 << base::SysNSStringToUTF8(data.html_string));
    LoadHtml(data.html_string);
    __block std::vector<FormData> forms;
    __block BOOL block_was_called = NO;
    [helper_ findPasswordFormsInFrame:GetMainFrame()
                    completionHandler:^(const std::vector<FormData>& result,
                                        uint32_t maxID) {
                      block_was_called = YES;
                      forms = result;
                    }];
    EXPECT_TRUE(
        WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool() {
          return block_was_called;
        }));
    if (data.expected_form_found) {
      ASSERT_EQ(1U, forms.size());
      EXPECT_EQ(data.expected_number_of_fields, forms[0].fields.size());
      EXPECT_EQ(data.expected_form_name, base::UTF16ToUTF8(forms[0].name));
    } else {
      ASSERT_TRUE(forms.empty());
    }
  }
}

// A script that resets all text fields, including those in iframes.
static NSString* kClearInputFieldsScript =
    @"function clearInputFields(win) {"
     "  var inputs = win.document.getElementsByTagName('input');"
     "  for (var i = 0; i < inputs.length; i++) {"
     "    inputs[i].value = '';"
     "  }"
     "  var frames = win.frames;"
     "  for (var i = 0; i < frames.length; i++) {"
     "    clearInputFields(frames[i]);"
     "  }"
     "}"
     "clearInputFields(window);";

// A script that runs after autofilling forms.  It returns ids and values of all
// non-empty fields, including those in iframes.
static NSString* kInputFieldValueVerificationScript =
    @"function findAllInputsInFrame(win, prefix) {"
     "  var result = '';"
     "  var inputs = win.document.getElementsByTagName('input');"
     "  for (var i = 0; i < inputs.length; i++) {"
     "    var input = inputs[i];"
     "    if (input.value) {"
     "      result += prefix + input.id + '=' + input.value + ';';"
     "    }"
     "  }"
     "  var frames = win.frames;"
     "  for (var i = 0; i < frames.length; i++) {"
     "    result += findAllInputsInFrame("
     "        frames[i], prefix + frames[i].name +'.');"
     "  }"
     "  return result;"
     "};"
     "function findAllInputs(win) {"
     "  return findAllInputsInFrame(win, '');"
     "};"
     "findAllInputs(window);";

// Tests that filling password forms with fill data works correctly.
TEST_F(PasswordFormHelperTest, FillPasswordFormWithFillData) {
  ukm::TestAutoSetUkmRecorder test_recorder;
  base::HistogramTester histogram_tester;
  LoadHtml(
      @"<form><input id='u1' type='text' name='un1'>"
       "<input id='p1' type='password' name='pw1'></form>");

  ASSERT_TRUE(SetUpUniqueIDs());
  const std::string base_url = BaseUrl();
  FieldRendererId username_field_id(2);
  FieldRendererId password_field_id(3);
  FillData fill_data;
  SetFillData(base_url, 1, username_field_id.value(), "john.doe@gmail.com",
              password_field_id.value(), "super!secret", &fill_data);

  __block int call_counter = 0;
  [helper_ fillPasswordFormWithFillData:fill_data
                                inFrame:GetMainFrame()
                       triggeredOnField:username_field_id
                      completionHandler:^(BOOL complete) {
                        ++call_counter;
                        EXPECT_TRUE(complete);
                      }];
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return call_counter == 1;
  }));
  id result = ExecuteJavaScript(kInputFieldValueVerificationScript);
  EXPECT_NSEQ(@"u1=john.doe@gmail.com;p1=super!secret;", result);

  histogram_tester.ExpectUniqueSample("PasswordManager.FillingSuccessIOS", true,
                                      1);
  // Check recorded UKM.
  auto entries = test_recorder.GetEntriesByName(
      ukm::builders::PasswordManager_PasswordFillingIOS::kEntryName);
  // Expect one recorded metric.
  ASSERT_EQ(1u, entries.size());
  test_recorder.ExpectEntryMetric(entries[0], "FillingSuccess", true);
}

// Tests that failure in filling password forms with fill data is recorded.
TEST_F(PasswordFormHelperTest, FillPasswordFormWithFillDataFillingFailure) {
  ukm::TestAutoSetUkmRecorder test_recorder;
  base::HistogramTester histogram_tester;
  LoadHtml(@"<form><input id='u1' type='text' name='un1'>"
            "<input id='p1' type='password' name='pw1'></form>");

  ASSERT_TRUE(SetUpUniqueIDs());
  const std::string base_url = BaseUrl();
  FieldRendererId username_field_id(2);
  // The password renderer id does not exist, that's why the filling will fail
  FieldRendererId password_field_id(404);
  FillData fill_data;
  SetFillData(base_url, 1, username_field_id.value(), "john.doe@gmail.com",
              password_field_id.value(), "super!secret", &fill_data);

  __block int call_counter = 0;
  [helper_ fillPasswordFormWithFillData:fill_data
                                inFrame:GetMainFrame()
                       triggeredOnField:username_field_id
                      completionHandler:^(BOOL complete) {
                        ++call_counter;
                      }];
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return call_counter == 1;
  }));

  histogram_tester.ExpectUniqueSample("PasswordManager.FillingSuccessIOS",
                                      false, 1);
  // Check recorded UKM.
  auto entries = test_recorder.GetEntriesByName(
      ukm::builders::PasswordManager_PasswordFillingIOS::kEntryName);
  // Expect one recorded metric.
  ASSERT_EQ(1u, entries.size());
  test_recorder.ExpectEntryMetric(entries[0], "FillingSuccess", false);
}

// Tests that extractPasswordFormData extracts wanted form on page with mutiple
// forms.
TEST_F(PasswordFormHelperTest, ExtractPasswordFormData) {
  LoadHtml(@"<form><input id='u1' type='text' name='un1'>"
            "<input id='p1' type='password' name='pw1'></form>"
            "<form><input id='u2' type='text' name='un2'>"
            "<input id='p2' type='password' name='pw2'></form>"
            "<form><input id='u3' type='text' name='un3'>"
            "<input id='p3' type='password' name='pw3'></form>");

  ASSERT_TRUE(SetUpUniqueIDs());

  __block int call_counter = 0;
  __block int success_counter = 0;
  __block FormData result = FormData();
  [helper_ extractPasswordFormData:FormRendererId(1)
                           inFrame:GetMainFrame()
                 completionHandler:^(BOOL complete, const FormData& form) {
                   ++call_counter;
                   if (complete) {
                     ++success_counter;
                     result = form;
                   }
                 }];
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return call_counter == 1;
  }));
  EXPECT_EQ(1, success_counter);
  EXPECT_EQ(result.unique_renderer_id, FormRendererId(1));

  call_counter = 0;
  success_counter = 0;
  result = FormData();

  [helper_ extractPasswordFormData:FormRendererId(404)
                           inFrame:GetMainFrame()
                 completionHandler:^(BOOL complete, const FormData& form) {
                   ++call_counter;
                   if (complete) {
                     ++success_counter;
                     result = form;
                   }
                 }];
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return call_counter == 1;
  }));
  EXPECT_EQ(0, success_counter);
}

// Tests that a form with username typed by user is not refilled when
// the user selects filling suggestion on password field.
TEST_F(PasswordFormHelperTest, FillPasswordIntoFormWithUserTypedUsername) {
  LoadHtml(@"<form><input id='u1' type='text' name='un1'>"
            "<input id='p1' type='password' name='pw1'></form>");

  ASSERT_TRUE(SetUpUniqueIDs());

  FieldRendererId username_field_id(2);
  FieldRendererId password_field_id(3);

  ExecuteJavaScript(
      @"document.getElementById('u1').value = 'typed@typed.com';");
  [helper_ updateFieldDataOnUserInput:username_field_id
                           inputValue:@"typed@typed.com"];

  // Try to autofill the form.
  FillData fill_data;
  SetFillData(BaseUrl(), 1, username_field_id.value(), "someacc@store.com",
              password_field_id.value(), "store!pw", &fill_data);

  __block bool called = NO;
  __block bool success = NO;
  [helper_ fillPasswordFormWithFillData:fill_data
                                inFrame:GetMainFrame()
                       triggeredOnField:password_field_id
                      completionHandler:^(BOOL res) {
                        called = YES;
                        success = res;
                      }];
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return called;
  }));
  EXPECT_EQ(success, YES);
  id result = ExecuteJavaScript(kInputFieldValueVerificationScript);
  EXPECT_NSEQ(@"u1=typed@typed.com;p1=store!pw;", result);
}

}  // namespace

NS_ASSUME_NONNULL_END
