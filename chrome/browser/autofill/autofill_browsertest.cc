// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/autofill_uitest_util.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/autofill/core/browser/test_autofill_manager_waiter.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/switches.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/events/keycodes/keyboard_codes.h"

using base::ASCIIToUTF16;
using base::UTF16ToASCII;
using testing::_;

namespace autofill {

// Default JavaScript code used to submit the forms.
const char kDocumentClickHandlerSubmitJS[] =
    "document.onclick = function() {"
    "  document.getElementById('testform').submit();"
    "};";

// TODO(bondd): PdmChangeWaiter in autofill_uitest_util.cc is a replacement for
// this class. Remove this class and use helper functions in that file instead.
class WindowedPersonalDataManagerObserver : public PersonalDataManagerObserver {
 public:
  explicit WindowedPersonalDataManagerObserver(Browser* browser)
      : alerted_(false), has_run_message_loop_(false), browser_(browser) {
    PersonalDataManagerFactory::GetForProfile(browser_->profile())->
        AddObserver(this);
  }

  ~WindowedPersonalDataManagerObserver() override {}

  void Wait() {
    if (!alerted_) {
      has_run_message_loop_ = true;
      content::RunMessageLoop();
    }
    PersonalDataManagerFactory::GetForProfile(browser_->profile())->
        RemoveObserver(this);
  }

  // PersonalDataManagerObserver:
  void OnPersonalDataChanged() override {
    if (has_run_message_loop_) {
      base::RunLoop::QuitCurrentWhenIdleDeprecated();
      has_run_message_loop_ = false;
    }
    alerted_ = true;
  }

  void OnInsufficientFormData() override { OnPersonalDataChanged(); }

 private:
  bool alerted_;
  bool has_run_message_loop_;
  raw_ptr<Browser> browser_;
};

class AutofillTest : public InProcessBrowserTest {
 protected:
  class TestAutofillManager : public BrowserAutofillManager {
   public:
    TestAutofillManager(ContentAutofillDriver* driver, AutofillClient* client)
        : BrowserAutofillManager(driver, client, "en-US") {}

    [[nodiscard]] testing::AssertionResult WaitForFormsSeen(
        int min_num_awaited_calls) {
      return forms_seen_waiter_.Wait(min_num_awaited_calls);
    }

   private:
    TestAutofillManagerWaiter forms_seen_waiter_{
        *this,
        {AutofillManagerEvent::kFormsSeen}};
  };

  AutofillTest() {
    feature_list_.InitAndEnableFeature(
        blink::features::kAutofillDetectRemovedFormControls);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    // Don't want Keychain coming up on Mac.
    test::DisableSystemServices(browser()->profile()->GetPrefs());

    // Wait for Personal Data Manager to be fully loaded to prevent that
    // spurious notifications deceive the tests.
    WaitForPersonalDataManagerToBeLoaded(browser()->profile());

    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TearDownOnMainThread() override {
    // Make sure to close any showing popups prior to tearing down the UI.
    ContentAutofillDriverFactory::FromWebContents(web_contents())
        ->DriverForFrame(web_contents()->GetPrimaryMainFrame())
        ->autofill_manager()
        ->client()
        ->HideAutofillPopup(PopupHidingReason::kTabGone);
    test::ReenableSystemServices();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Slower test bots (chromeos, debug, etc) are flaky
    // due to slower loading interacting with deferred commits.
    command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
  }

  PersonalDataManager* personal_data_manager() {
    return PersonalDataManagerFactory::GetForProfile(browser()->profile());
  }

  typedef std::map<std::string, std::string> FormMap;

  // Helper function to obtain the Javascript required to update a form.
  std::string GetJSToFillForm(const FormMap& data) {
    std::string js;
    for (const auto& entry : data) {
      js += "document.getElementById('" + entry.first + "').value = '" +
            entry.second + "';";
    }
    return js;
  }

  // Navigate to the form, input values into the fields, and submit the form.
  // The function returns after the PersonalDataManager is updated.
  void FillFormAndSubmit(const std::string& filename, const FormMap& data) {
    FillFormAndSubmitWithHandler(filename, data, kDocumentClickHandlerSubmitJS,
                                 true);
  }

  // Helper where the actual submit JS code can be specified, as well as whether
  // the test should |simulate_click| on the document.
  void FillFormAndSubmitWithHandler(const std::string& filename,
                                    const FormMap& data,
                                    const std::string& submit_js,
                                    bool simulate_click) {
    GURL url = embedded_test_server()->GetURL("/autofill/" + filename);
    NavigateParams params(browser(), url, ui::PAGE_TRANSITION_LINK);
    params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    ui_test_utils::NavigateToURL(&params);
    ASSERT_TRUE(
        autofill_manager_injector_[web_contents()]->WaitForFormsSeen(1));
    // Shortcut explicit save prompts and automatically accept.
    personal_data_manager()->set_auto_accept_address_imports_for_testing(true);
    WindowedPersonalDataManagerObserver observer(browser());
    ASSERT_TRUE(content::ExecuteScript(web_contents(),
                                       GetJSToFillForm(data) + submit_js));
    if (simulate_click) {
      // Simulate a mouse click to submit the form because form submissions not
      // triggered by user gestures are ignored.
      content::SimulateMouseClick(
          browser()->tab_strip_model()->GetActiveWebContents(), 0,
          blink::WebMouseEvent::Button::kLeft);
    }
    observer.Wait();
  }

  // Aggregate profiles from forms into Autofill preferences. Returns the number
  // of parsed profiles.
  int AggregateProfilesIntoAutofillPrefs(const std::string& filename) {
    std::string data;
    base::FilePath data_file =
        ui_test_utils::GetTestFilePath(base::FilePath().AppendASCII("autofill"),
                                       base::FilePath().AppendASCII(filename));
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      CHECK(base::ReadFileToString(data_file, &data));
    }
    std::vector<std::string> lines = base::SplitString(
        data, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    int parsed_profiles = 0;
    for (const auto& line : lines) {
      if (base::StartsWith(line, "#", base::CompareCase::SENSITIVE))
        continue;

      std::vector<std::string> fields = base::SplitString(
          line, "|", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
      if (fields.empty())
        continue;  // Blank line.

      ++parsed_profiles;
      CHECK_EQ(12u, fields.size());

      FormMap form;
      form["NAME_FIRST"] = fields[0];
      form["NAME_MIDDLE"] = fields[1];
      form["NAME_LAST"] = fields[2];
      form["EMAIL_ADDRESS"] = fields[3];
      form["COMPANY_NAME"] = fields[4];
      form["ADDRESS_HOME_LINE1"] = fields[5];
      form["ADDRESS_HOME_LINE2"] = fields[6];
      form["ADDRESS_HOME_CITY"] = fields[7];
      form["ADDRESS_HOME_STATE"] = fields[8];
      form["ADDRESS_HOME_ZIP"] = fields[9];
      form["ADDRESS_HOME_COUNTRY"] = fields[10];
      form["PHONE_HOME_WHOLE_NUMBER"] = fields[11];

      FillFormAndSubmit("duplicate_profiles_test.html", form);
    }
    return parsed_profiles;
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  TestAutofillManager* autofill_manager() {
    return autofill_manager(web_contents()->GetPrimaryMainFrame());
  }

  TestAutofillManager* autofill_manager(content::RenderFrameHost* rfh) {
    return autofill_manager_injector_[rfh];
  }

  const FormStructure* WaitForFormWithNFields(size_t n) {
    return WaitForMatchingForm(autofill_manager(),
                               base::BindRepeating(
                                   [](size_t n, const FormStructure& form) {
                                     return form.active_field_count() == n;
                                   },
                                   n));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  test::AutofillBrowserTestEnvironment autofill_test_environment_;
  TestAutofillManagerInjector<TestAutofillManager> autofill_manager_injector_;
};

// Test that Autofill aggregates a minimum valid profile.
// The minimum required address fields must be specified: First Name, Last Name,
// Address Line 1, City, Zip Code, and State.
IN_PROC_BROWSER_TEST_F(AutofillTest, AggregatesMinValidProfile) {
  FormMap data;
  data["NAME_FIRST"] = "Bob";
  data["NAME_LAST"] = "Smith";
  data["ADDRESS_HOME_LINE1"] = "1234 H St.";
  data["ADDRESS_HOME_CITY"] = "Mountain View";
  data["ADDRESS_HOME_STATE"] = "CA";
  data["ADDRESS_HOME_ZIP"] = "94043";
  FillFormAndSubmit("duplicate_profiles_test.html", data);

  ASSERT_EQ(1u, personal_data_manager()->GetProfiles().size());
}

// Different Javascript to submit the form.
IN_PROC_BROWSER_TEST_F(AutofillTest, AggregatesMinValidProfileDifferentJS) {
  FormMap data;
  data["NAME_FIRST"] = "Bob";
  data["NAME_LAST"] = "Smith";
  data["ADDRESS_HOME_LINE1"] = "1234 H St.";
  data["ADDRESS_HOME_CITY"] = "Mountain View";
  data["ADDRESS_HOME_STATE"] = "CA";
  data["ADDRESS_HOME_ZIP"] = "94043";

  std::string submit("document.forms[0].submit();");
  FillFormAndSubmitWithHandler("duplicate_profiles_test.html", data, submit,
                               false);

  ASSERT_EQ(1u, personal_data_manager()->GetProfiles().size());
}

// Form submitted via JavaScript, the user's personal data is updated even
// if the event handler on the submit event prevents submission of the form.
IN_PROC_BROWSER_TEST_F(AutofillTest, ProfilesAggregatedWithSubmitHandler) {
  FormMap data;
  data["NAME_FIRST"] = "Bob";
  data["NAME_LAST"] = "Smith";
  data["ADDRESS_HOME_LINE1"] = "1234 H St.";
  data["ADDRESS_HOME_CITY"] = "Mountain View";
  data["ADDRESS_HOME_STATE"] = "CA";
  data["ADDRESS_HOME_ZIP"] = "94043";

  std::string submit(
      "var preventFunction = function(event) { event.preventDefault(); };"
      "document.forms[0].addEventListener('submit', preventFunction);"
      "document.querySelector('input[type=submit]').click();");
  FillFormAndSubmitWithHandler("duplicate_profiles_test.html", data, submit,
                               false);

  // The BrowserAutofillManager will update the user's profile.
  EXPECT_EQ(1u, personal_data_manager()->GetProfiles().size());

  EXPECT_EQ(u"Bob",
            personal_data_manager()->GetProfiles()[0]->GetRawInfo(NAME_FIRST));
  EXPECT_EQ(u"Smith",
            personal_data_manager()->GetProfiles()[0]->GetRawInfo(NAME_LAST));
}

// Test Autofill does not aggregate profiles with no address info.
// The minimum required address fields must be specified: First Name, Last Name,
// Address Line 1, City, Zip Code, and State.
IN_PROC_BROWSER_TEST_F(AutofillTest, ProfilesNotAggregatedWithNoAddress) {
  FormMap data;
  data["NAME_FIRST"] = "Bob";
  data["NAME_LAST"] = "Smith";
  data["EMAIL_ADDRESS"] = "bsmith@example.com";
  data["COMPANY_NAME"] = "Mountain View";
  data["ADDRESS_HOME_CITY"] = "Mountain View";
  data["PHONE_HOME_WHOLE_NUMBER"] = "650-555-4567";
  FillFormAndSubmit("duplicate_profiles_test.html", data);

  ASSERT_TRUE(personal_data_manager()->GetProfiles().empty());
}

// Test Autofill does not aggregate profiles with an invalid email.
IN_PROC_BROWSER_TEST_F(AutofillTest, ProfilesNotAggregatedWithInvalidEmail) {
  FormMap data;
  data["NAME_FIRST"] = "Bob";
  data["NAME_LAST"] = "Smith";
  data["EMAIL_ADDRESS"] = "garbage";
  data["ADDRESS_HOME_LINE1"] = "1234 H St.";
  data["ADDRESS_HOME_CITY"] = "San Jose";
  data["ADDRESS_HOME_STATE"] = "CA";
  data["ADDRESS_HOME_ZIP"] = "95110";
  data["COMPANY_NAME"] = "Company X";
  data["PHONE_HOME_WHOLE_NUMBER"] = "408-871-4567";
  FillFormAndSubmit("duplicate_profiles_test.html", data);

  ASSERT_TRUE(personal_data_manager()->GetProfiles().empty());
}

// Tests that the profile is saved if the phone number is valid in the selected
// country. The data file contains two profiles with valid phone numbers and two
// profiles with invalid phone numbers from their respective country.
// Profiles with an invalid number are imported, but their number is removed.
IN_PROC_BROWSER_TEST_F(AutofillTest, ProfileSavedWithValidCountryPhone) {
  std::vector<FormMap> profiles = {
      {{"NAME_FIRST", "Bob"},
       {"NAME_LAST", "Smith"},
       {"ADDRESS_HOME_LINE1", "123 Cherry Ave"},
       {"ADDRESS_HOME_CITY", "Mountain View"},
       {"ADDRESS_HOME_STATE", "CA"},
       {"ADDRESS_HOME_ZIP", "94043"},
       {"ADDRESS_HOME_COUNTRY", "United States"},
       {"PHONE_HOME_WHOLE_NUMBER", "408-871-4567"}},

      {{"NAME_FIRST", "John"},
       {"NAME_LAST", "Doe"},
       {"ADDRESS_HOME_LINE1", "987 H St"},
       {"ADDRESS_HOME_CITY", "San Jose"},
       {"ADDRESS_HOME_STATE", "CA"},
       {"ADDRESS_HOME_ZIP", "95510"},
       {"ADDRESS_HOME_COUNTRY", "United States"},
       {"PHONE_HOME_WHOLE_NUMBER", "408-123-456"}},

      {{"NAME_FIRST", "Jane"},
       {"NAME_LAST", "Doe"},
       {"ADDRESS_HOME_LINE1", "1523 Garcia St"},
       {"ADDRESS_HOME_CITY", "Mountain View"},
       {"ADDRESS_HOME_STATE", "CA"},
       {"ADDRESS_HOME_ZIP", "94043"},
       {"ADDRESS_HOME_COUNTRY", "Germany"},
       {"PHONE_HOME_WHOLE_NUMBER", "+49 40-80-81-79-000"}},

      {{"NAME_FIRST", "Bonnie"},
       {"NAME_LAST", "Smith"},
       {"ADDRESS_HOME_LINE1", "6723 Roadway Rd"},
       {"ADDRESS_HOME_CITY", "San Jose"},
       {"ADDRESS_HOME_STATE", "CA"},
       {"ADDRESS_HOME_ZIP", "95510"},
       {"ADDRESS_HOME_COUNTRY", "Germany"},
       {"PHONE_HOME_WHOLE_NUMBER", "+21 08450 777 777"}}};

  for (const auto& profile : profiles)
    FillFormAndSubmit("autofill_test_form.html", profile);

  std::vector<std::u16string> actual_phone_numbers;
  for (const AutofillProfile* profile :
       personal_data_manager()->GetProfiles()) {
    actual_phone_numbers.push_back(
        profile->GetRawInfo(PHONE_HOME_WHOLE_NUMBER));
  }
  // Two valid phone numbers are imported, two invalid ones are removed.
  EXPECT_THAT(actual_phone_numbers,
              testing::UnorderedElementsAreArray(
                  {u"408-871-4567", u"+49 40-80-81-79-000", u"", u""}));
}

// Prepend country codes when formatting phone numbers if:
// - It was provided in the first place.
// - `AutofillInferCountryCallingCode` is enabled.
IN_PROC_BROWSER_TEST_F(AutofillTest, AppendCountryCodeForAggregatedPhones) {
  FormMap data = {{"NAME_FIRST", "Bob"},
                  {"NAME_LAST", "Smith"},
                  {"ADDRESS_HOME_LINE1", "1234 H St."},
                  {"ADDRESS_HOME_CITY", "San Jose"},
                  {"ADDRESS_HOME_STATE", "CA"},
                  {"ADDRESS_HOME_ZIP", "95110"},
                  {"ADDRESS_HOME_COUNTRY", "Germany"},
                  {"PHONE_HOME_WHOLE_NUMBER", "+4908450777777"}};
  FillFormAndSubmit("autofill_test_form.html", data);

  data["ADDRESS_HOME_LINE1"] = "4321 H St.";
  data["PHONE_HOME_WHOLE_NUMBER"] = "08450777777";
  FillFormAndSubmit("autofill_test_form.html", data);

  std::vector<std::u16string> actual_phone_numbers;
  for (const AutofillProfile* profile :
       personal_data_manager()->GetProfiles()) {
    actual_phone_numbers.push_back(
        profile->GetRawInfo(PHONE_HOME_WHOLE_NUMBER));
  }

  // With `AutofillInferCountryCallingCode` enabled, the country code of the
  // second phone number is derived from the profile (Germany).
  std::vector<std::u16string> expected_phone_numbers = {
      u"+49 8450 777777",
      base::FeatureList::IsEnabled(features::kAutofillInferCountryCallingCode)
          ? u"+49 8450 777777"
          : u"08450 777777"};

  EXPECT_THAT(actual_phone_numbers,
              testing::UnorderedElementsAreArray(expected_phone_numbers));
}

// Test that Autofill uses '+' sign for international numbers.
// This applies to the following cases:
//   The phone number has a leading '+'.
//   The phone number does not have a leading '+'.
//   The phone number has a leading international direct dialing (IDD) code.
// This does not apply to US numbers. For US numbers, '+' is removed.
IN_PROC_BROWSER_TEST_F(AutofillTest, UsePlusSignForInternationalNumber) {
  std::vector<FormMap> profiles;

  FormMap data1;
  data1["NAME_FIRST"] = "Bonnie";
  data1["NAME_LAST"] = "Smith";
  data1["ADDRESS_HOME_LINE1"] = "6723 Roadway Rd";
  data1["ADDRESS_HOME_CITY"] = "Reading";
  data1["ADDRESS_HOME_STATE"] = "Berkshire";
  data1["ADDRESS_HOME_ZIP"] = "RG12 3BR";
  data1["ADDRESS_HOME_COUNTRY"] = "United Kingdom";
  data1["PHONE_HOME_WHOLE_NUMBER"] = "+44 7624-123456";
  profiles.push_back(data1);

  FormMap data2;
  data2["NAME_FIRST"] = "John";
  data2["NAME_LAST"] = "Doe";
  data2["ADDRESS_HOME_LINE1"] = "987 H St";
  data2["ADDRESS_HOME_CITY"] = "Reading";
  data2["ADDRESS_HOME_STATE"] = "BerkShire";
  data2["ADDRESS_HOME_ZIP"] = "RG12 3BR";
  data2["ADDRESS_HOME_COUNTRY"] = "United Kingdom";
  data2["PHONE_HOME_WHOLE_NUMBER"] = "44 7624 123456";
  profiles.push_back(data2);

  FormMap data3;
  data3["NAME_FIRST"] = "Jane";
  data3["NAME_LAST"] = "Doe";
  data3["ADDRESS_HOME_LINE1"] = "1523 Garcia St";
  data3["ADDRESS_HOME_CITY"] = "Reading";
  data3["ADDRESS_HOME_STATE"] = "BerkShire";
  data3["ADDRESS_HOME_ZIP"] = "RG12 3BR";
  data3["ADDRESS_HOME_COUNTRY"] = "United Kingdom";
  data3["PHONE_HOME_WHOLE_NUMBER"] = "0044 7624 123456";
  profiles.push_back(data3);

  FormMap data4;
  data4["NAME_FIRST"] = "Bob";
  data4["NAME_LAST"] = "Smith";
  data4["ADDRESS_HOME_LINE1"] = "123 Cherry Ave";
  data4["ADDRESS_HOME_CITY"] = "Mountain View";
  data4["ADDRESS_HOME_STATE"] = "CA";
  data4["ADDRESS_HOME_ZIP"] = "94043";
  data4["ADDRESS_HOME_COUNTRY"] = "United States";
  data4["PHONE_HOME_WHOLE_NUMBER"] = "+1 (408) 871-4567";
  profiles.push_back(data4);

  for (const FormMap& profile : profiles) {
    FillFormAndSubmit("autofill_test_form.html", profile);
  }

  ASSERT_EQ(4u, personal_data_manager()->GetProfiles().size());

  for (size_t i = 0; i < personal_data_manager()->GetProfiles().size(); ++i) {
    AutofillProfile* profile = personal_data_manager()->GetProfiles()[i];
    std::string expectation;
    std::string name = UTF16ToASCII(profile->GetRawInfo(NAME_FIRST));

    if (name == "Bonnie")
      expectation = "+447624123456";
    else if (name == "John")
      expectation = "+447624123456";
    else if (name == "Jane")
      expectation = "+447624123456";
    else if (name == "Bob")
      expectation = "14088714567";

    EXPECT_EQ(ASCIIToUTF16(expectation),
              profile->GetInfo(PHONE_HOME_WHOLE_NUMBER, ""));
  }
}

// Test profile not aggregated if email found in non-email field.
IN_PROC_BROWSER_TEST_F(AutofillTest, ProfileWithEmailInOtherFieldNotSaved) {
  FormMap data;
  data["NAME_FIRST"] = "Bob";
  data["NAME_LAST"] = "Smith";
  data["ADDRESS_HOME_LINE1"] = "bsmith@gmail.com";
  data["ADDRESS_HOME_CITY"] = "San Jose";
  data["ADDRESS_HOME_STATE"] = "CA";
  data["ADDRESS_HOME_ZIP"] = "95110";
  data["COMPANY_NAME"] = "Company X";
  data["PHONE_HOME_WHOLE_NUMBER"] = "408-871-4567";
  FillFormAndSubmit("duplicate_profiles_test.html", data);

  ASSERT_EQ(0u, personal_data_manager()->GetProfiles().size());
}

// Test that profiles merge for aggregated data with same address.
// The criterion for when two profiles are expected to be merged is when their
// 'Address Line 1' and 'City' data match. When two profiles are merged, any
// remaining address fields are expected to be overwritten. Any non-address
// fields should accumulate multi-valued data.
// DISABLED: http://crbug.com/281541
IN_PROC_BROWSER_TEST_F(AutofillTest,
                       DISABLED_MergeAggregatedProfilesWithSameAddress) {
  AggregateProfilesIntoAutofillPrefs("dataset_same_address.txt");

  ASSERT_EQ(3u, personal_data_manager()->GetProfiles().size());
}

// Test profiles are not merged without minimum address values.
// Minimum address values needed during aggregation are: address line 1, city,
// state, and zip code.
// Profiles are merged when data for address line 1 and city match.
IN_PROC_BROWSER_TEST_F(AutofillTest, ProfilesNotMergedWhenNoMinAddressData) {
  AggregateProfilesIntoAutofillPrefs("dataset_no_address.txt");

  ASSERT_EQ(0u, personal_data_manager()->GetProfiles().size());
}

// Test Autofill ability to merge duplicate profiles and throw away junk.
// TODO(isherman): this looks redundant, consider removing.
// DISABLED: http://crbug.com/281541
// This tests opens and submits over 240 forms which does not finish within the
// allocated time of browser_tests. This should be converted into a unittest.
IN_PROC_BROWSER_TEST_F(AutofillTest,
                       DISABLED_MergeAggregatedDuplicatedProfiles) {
  int num_of_profiles =
      AggregateProfilesIntoAutofillPrefs("dataset_duplicated_profiles.txt");

  ASSERT_GT(num_of_profiles,
            static_cast<int>(personal_data_manager()->GetProfiles().size()));
}

IN_PROC_BROWSER_TEST_F(AutofillTest, DynamicForm_DiscoverRemovedFormFields) {
  // Load a form that contains 3 fields.
  GURL url = embedded_test_server()->GetURL(
      "/autofill/dynamic_form_element_removed.html");
  NavigateParams params(browser(), url, ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  ui_test_utils::NavigateToURL(&params);
  ASSERT_TRUE(WaitForFormWithNFields(3))
      << "Waiting for form before field removal";

  // Remove one field via JavaScript and expect that the AutofillManager learns
  // about this.
  ASSERT_TRUE(content::ExecuteScript(web_contents(), "RemoveCity();"));
  EXPECT_TRUE(WaitForFormWithNFields(2))
      << "Waiting for after before field removal";
}

// Accessibility Tests
class AutofillAccessibilityTest : public AutofillTest {
 protected:
  AutofillAccessibilityTest() {
    command_line_.GetProcessCommandLine()->AppendSwitchASCII(
        "vmodule", "accessibility_notification_waiter=1");
  }

  // Returns true if kAutofillAvailable state is present AND  kAutoComplete
  // string attribute is missing; only one should be set at any given time.
  // Returns false otherwise.
  bool AutofillIsAvailable(const ui::AXNodeData& data) {
    return data.HasState(ax::mojom::State::kAutofillAvailable) &&
           !data.HasStringAttribute(ax::mojom::StringAttribute::kAutoComplete);
  }

  // Returns true if kAutocomplete string attribute is present AND
  // kAutofillAvailable state is missing; only one should be set at any given
  // time. Returns false otherwise.
  bool AutocompleteIsAvailable(const ui::AXNodeData& data) {
    return data.HasStringAttribute(ax::mojom::StringAttribute::kAutoComplete) &&
           !data.HasState(ax::mojom::State::kAutofillAvailable);
  }

 private:
  base::test::ScopedCommandLine command_line_;
};

// Test that autofill available state is correctly set on accessibility node.
// Test is flaky: https://crbug.com/1239099
IN_PROC_BROWSER_TEST_F(AutofillAccessibilityTest, DISABLED_TestAutofillState) {
  content::BrowserAccessibilityState::GetInstance()->EnableAccessibility();

  // Navigate to url and wait for accessibility notification.
  GURL url =
      embedded_test_server()->GetURL("/autofill/duplicate_profiles_test.html");
  NavigateParams params(browser(), url, ui::PAGE_TRANSITION_LINK);
  content::AccessibilityNotificationWaiter layout_waiter_one(
      web_contents(), ui::kAXModeComplete, ax::mojom::Event::kLoadComplete);
  ui_test_utils::NavigateToURL(&params);
  ASSERT_TRUE(layout_waiter_one.WaitForNotification());

  // Focus target form field.
  const std::string focus_name_first_js =
      "document.getElementById('NAME_FIRST').focus();";
  ASSERT_TRUE(content::ExecuteScript(web_contents(), focus_name_first_js));

  // Assert that autofill is not yet available for target form field.
  // Loop while criteria is not met.
  ui::AXNodeData node_data;
  std::string node_name;
  const ax::mojom::Role target_role = ax::mojom::Role::kTextField;
  const std::string target_name = "First Name:";
  while (!(node_data.role == target_role && node_name == target_name &&
           !AutofillIsAvailable(node_data))) {
    content::WaitForAccessibilityTreeToChange(web_contents());
    node_data = content::GetFocusedAccessibilityNodeInfo(web_contents());
    node_name = node_data.GetStringAttribute(ax::mojom::StringAttribute::kName);
  }
  // Sanity check.
  ASSERT_FALSE(AutofillIsAvailable(node_data));

  // Fill form and submit.
  FormMap data;
  data["NAME_FIRST"] = "Bob";
  data["NAME_LAST"] = "Smith";
  data["ADDRESS_HOME_LINE1"] = "1234 H St.";
  data["ADDRESS_HOME_CITY"] = "Mountain View";
  data["EMAIL_ADDRESS"] = "bsmith@example.com";
  data["ADDRESS_HOME_STATE"] = "CA";
  data["ADDRESS_HOME_ZIP"] = "94043";
  data["ADDRESS_HOME_COUNTRY"] = "United States";
  data["PHONE_HOME_WHOLE_NUMBER"] = "408-871-4567";
  FillFormAndSubmit("duplicate_profiles_test.html", data);
  ASSERT_EQ(1u, personal_data_manager()->GetProfiles().size());

  // Reload page.
  content::AccessibilityNotificationWaiter layout_waiter_two(
      web_contents(), ui::kAXModeComplete, ax::mojom::Event::kLoadComplete);
  ui_test_utils::NavigateToURL(&params);
  ASSERT_TRUE(layout_waiter_two.WaitForNotification());

  // Focus target form field.
  ASSERT_TRUE(content::ExecuteScript(web_contents(), focus_name_first_js));

  // Assert that autofill is now available for target form field.
  // Loop while criteria is not met.
  while (!(node_data.role == target_role && node_name == target_name &&
           AutofillIsAvailable(node_data))) {
    content::WaitForAccessibilityTreeToChange(web_contents());
    node_data = content::GetFocusedAccessibilityNodeInfo(web_contents());
    node_name = node_data.GetStringAttribute(ax::mojom::StringAttribute::kName);
  }
  // Sanity check.
  ASSERT_TRUE(AutofillIsAvailable(node_data));
}

// Test that autocomplete available string attribute is correctly set on
// accessibility node. Test autocomplete in this file since it uses the same
// infrastructure as autofill.
// Test is flaky: http://crbug.com/1239099
IN_PROC_BROWSER_TEST_F(AutofillAccessibilityTest,
                       DISABLED_TestAutocompleteState) {
  content::BrowserAccessibilityState::GetInstance()->EnableAccessibility();
  // Navigate to url and wait for accessibility notification
  GURL url =
      embedded_test_server()->GetURL("/autofill/duplicate_profiles_test.html");
  NavigateParams params(browser(), url, ui::PAGE_TRANSITION_LINK);
  content::AccessibilityNotificationWaiter layout_waiter_one(
      web_contents(), ui::kAXModeComplete, ax::mojom::Event::kLoadComplete);
  ui_test_utils::NavigateToURL(&params);
  ASSERT_TRUE(layout_waiter_one.WaitForNotification());

  // Focus target form field.
  const std::string focus_name_first_js =
      "document.getElementById('NAME_FIRST').focus();";
  ASSERT_TRUE(content::ExecuteScript(web_contents(), focus_name_first_js));

  // Assert that autocomplete is not yet available for target form field.
  // Loop while criteria is not met.
  ui::AXNodeData node_data;
  std::string node_name;
  const ax::mojom::Role target_role = ax::mojom::Role::kTextField;
  const std::string target_name = "First Name:";
  while (!(node_data.role == target_role && node_name == target_name &&
           !AutocompleteIsAvailable(node_data))) {
    content::WaitForAccessibilityTreeToChange(web_contents());
    node_data = content::GetFocusedAccessibilityNodeInfo(web_contents());
    node_name = node_data.GetStringAttribute(ax::mojom::StringAttribute::kName);
  }
  // Sanity check.
  ASSERT_FALSE(AutocompleteIsAvailable(node_data));

  // Partially fill form. This should not set autofill state, but rather,
  // autocomplete state.
  FormMap data;
  data["NAME_FIRST"] = "Bob";
  data["NAME_LAST"] = "Smith";
  FillFormAndSubmit("duplicate_profiles_test.html", data);
  // Since we didn't fill the entire form, we should not have increased the
  // number of autofill profiles.
  ASSERT_EQ(0u, personal_data_manager()->GetProfiles().size());

  // Reload page.
  content::AccessibilityNotificationWaiter layout_waiter_two(
      web_contents(), ui::kAXModeComplete, ax::mojom::Event::kLoadComplete);
  ui_test_utils::NavigateToURL(&params);
  ASSERT_TRUE(layout_waiter_two.WaitForNotification());

  // Focus target form field.
  ASSERT_TRUE(content::ExecuteScript(web_contents(), focus_name_first_js));

  // Assert that autocomplete is now available for target form field.
  // Loop while criteria is not met.
  while (!(node_data.role == target_role && node_name == target_name &&
           AutocompleteIsAvailable(node_data))) {
    content::WaitForAccessibilityTreeToChange(web_contents());
    node_data = content::GetFocusedAccessibilityNodeInfo(web_contents());
    node_name = node_data.GetStringAttribute(ax::mojom::StringAttribute::kName);
  }
  // Sanity check.
  ASSERT_TRUE(AutocompleteIsAvailable(node_data));
}

// Test fixture for prerendering tests. In general, these tests aim to check
// that we avoid unexpected behavior while the prerendered page is inactive and
// that the page operates as expected, post-activation.
class AutofillTestPrerendering : public InProcessBrowserTest {
 protected:
  class MockAutofillManager : public BrowserAutofillManager {
   public:
    MockAutofillManager(ContentAutofillDriver* driver, AutofillClient* client)
        : BrowserAutofillManager(driver, client, "en-US") {
      // We need to set these expectations immediately to catch any premature
      // calls while prerendering.
      if (driver->render_frame_host()->GetLifecycleState() ==
          content::RenderFrameHost::LifecycleState::kPrerendering) {
        EXPECT_CALL(*this, OnFormsSeen(_, _)).Times(0);
        EXPECT_CALL(*this, OnFocusOnFormFieldImpl(_, _, _)).Times(0);
      }
    }
    MOCK_METHOD(void,
                OnFormsSeen,
                (const std::vector<FormData>&,
                 const std::vector<FormGlobalId>&),
                (override));
    MOCK_METHOD(void,
                OnFocusOnFormFieldImpl,
                (const FormData&,
                 const FormFieldData&,
                 const gfx::RectF& bounding_box),
                (override));
  };

  void SetUp() override {
    prerender_helper_.SetUp(embedded_test_server());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    // Slower test bots (chromeos, debug, etc) are flaky
    // due to slower loading interacting with deferred commits.
    command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
  }

  content::test::PrerenderTestHelper& prerender_helper() {
    return prerender_helper_;
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  MockAutofillManager* autofill_manager(content::RenderFrameHost* rfh) {
    return autofill_manager_injector_[rfh];
  }

 private:
  test::AutofillBrowserTestEnvironment autofill_test_environment_;
  TestAutofillManagerInjector<MockAutofillManager> autofill_manager_injector_;
  content::test::PrerenderTestHelper prerender_helper_{
      base::BindRepeating(&AutofillTestPrerendering::web_contents,
                          base::Unretained(this))};
};

// Ensures that the prerendered renderer does not attempt to communicate with
// the browser in response to RenderFrameObserver messages. Specifically, it
// checks that it does not alert the browser that a form has been seen prior to
// activation and that it does alert the browser after activation. Also ensures
// that programmatic input on the prerendered page does not result in unexpected
// messages prior to activation and that things work correctly post-activation.
IN_PROC_BROWSER_TEST_F(AutofillTestPrerendering, DeferWhilePrerendering) {
  GURL prerender_url =
      embedded_test_server()->GetURL("/autofill/prerendered.html");
  GURL initial_url = embedded_test_server()->GetURL("/empty.html");
  prerender_helper().NavigatePrimaryPage(initial_url);

  int host_id = prerender_helper().AddPrerender(prerender_url);
  auto* rfh = prerender_helper().GetPrerenderedMainFrameHost(host_id);
  ASSERT_TRUE(content::ExecJs(rfh,
                              "document.querySelector('#NAME_FIRST').focus();",
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Since the initial prerender page load has finished at this point and we
  // have issued our programmatic focus, we need to check that the expectations
  // we set up during render frame creation have been met (i.e., that we did not
  // issue a calls to the driver for either the forms being seen nor the focus
  // update).
  MockAutofillManager* mock = autofill_manager(rfh);
  testing::Mock::VerifyAndClearExpectations(mock);
  // Next, we ensure that once we activate, we issue the deferred calls.
  base::RunLoop run_loop;
  EXPECT_CALL(*mock, OnFocusOnFormFieldImpl(_, _, _)).Times(1);
  EXPECT_CALL(*mock, OnFormsSeen(_, _))
      .Times(1)
      .WillRepeatedly(
          testing::InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));

  prerender_helper().NavigatePrimaryPage(prerender_url);
  EXPECT_EQ(prerender_helper().GetRequestCount(prerender_url), 1);
  run_loop.Run();
}

// Test fixture for testing that that appropriate form submission events are
// fired in BrowserAutofillManager.
// The parameters indicate whether or not to enable
// AutofillAllowDuplicateFormSubmissions and
// AutofillProbableFormSubmissionInBrowser, respectively.
class AutofillTestFormSubmission
    : public InProcessBrowserTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 protected:
  class MockAutofillManager : public BrowserAutofillManager {
   public:
    MockAutofillManager(ContentAutofillDriver* driver, AutofillClient* client)
        : BrowserAutofillManager(driver, client, "en-US") {}
    MOCK_METHOD(void,
                OnFormSubmittedImpl,
                (const FormData&, bool, mojom::SubmissionSource),
                (override));
  };

  AutofillTestFormSubmission() {
    std::vector<base::test::FeatureRef> enabled;
    std::vector<base::test::FeatureRef> disabled;
    if (std::get<0>(GetParam())) {
      enabled.push_back(features::kAutofillAllowDuplicateFormSubmissions);
    } else {
      disabled.push_back(features::kAutofillAllowDuplicateFormSubmissions);
    }
    if (std::get<1>(GetParam())) {
      enabled.push_back(features::kAutofillProbableFormSubmissionInBrowser);
    } else {
      disabled.push_back(features::kAutofillProbableFormSubmissionInBrowser);
    }
    feature_list_.InitWithFeatures(enabled, disabled);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    SetUpServer();
    NavigateToPage("/form.html");
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Slower test bots (chromeos, debug, etc) are flaky
    // due to slower loading interacting with deferred commits.
    command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
  }

  void ExecuteScript(const std::string& js) {
    // Simulate a mouse click to submit the form because form submissions not
    // triggered by user gestures are ignored.
    std::string onclick_js = "document.onclick = function() { " + js + "; };";
    ASSERT_TRUE(content::ExecuteScript(web_contents(), onclick_js));
    content::SimulateMouseClick(web_contents(), 0,
                                blink::WebMouseEvent::Button::kLeft);
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  MockAutofillManager* autofill_manager() {
    return autofill_manager(web_contents()->GetPrimaryMainFrame());
  }

  MockAutofillManager* autofill_manager(content::RenderFrameHost* rfh) {
    return autofill_manager_injector_[rfh];
  }

 private:
  void SetUpServer() {
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &AutofillTestFormSubmission::HandleRequest, base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    GURL absolute_url = embedded_test_server()->GetURL(request.relative_url);
    std::string content;
    if (absolute_url.path() == "/form.html") {
      content = get_form_html();
    } else if (absolute_url.path() == "/success.html") {
      content = "<html><body>Happy times!";
    } else {
      return nullptr;
    }

    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_OK);
    http_response->set_content_type("text/html");
    http_response->set_content(content);
    return http_response;
  }

  virtual std::string get_form_html() const {
    return "<html><body>"
           "<form id='form' method='POST' action='/success.html'>"
           "Name: <input type='text' id='name'><br>"
           "Address: <input type='text' id='address'><br>"
           "City: <input type='text' id='city'><br>"
           "ZIP: <input type='text' id='zip'><br>"
           "State: <select id='state'>"
           "  <option value='CA'>CA</option>"
           "  <option value='WA'>WA</option>"
           "</select><br>"
           "</form>";
  }

  void NavigateToPage(const std::string& filename) {
    GURL url = embedded_test_server()->GetURL(filename);
    NavigateParams params(browser(), url, ui::PAGE_TRANSITION_LINK);
    ui_test_utils::NavigateToURL(&params);
  }

  test::AutofillBrowserTestEnvironment autofill_test_environment_;
  base::test::ScopedFeatureList feature_list_;
  TestAutofillManagerInjector<MockAutofillManager> autofill_manager_injector_;
};

// Tests that user-triggered submission triggers a submission event in
// BrowserAutofillManager.
IN_PROC_BROWSER_TEST_P(AutofillTestFormSubmission, Submission) {
  base::RunLoop run_loop;
  EXPECT_CALL(
      *autofill_manager(),
      OnFormSubmittedImpl(_, _, mojom::SubmissionSource::FORM_SUBMISSION))
      .Times(1)
      .WillRepeatedly(
          testing::InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  ExecuteScript(
      "document.getElementById('name').value = 'Sarah';"
      "document.getElementById('name').select();"
      "document.getElementById('form').submit();");
  run_loop.Run();
}

// Tests that non-link-click, renderer-initiated navigation triggers a
// submission event in BrowserAutofillManager.
IN_PROC_BROWSER_TEST_P(AutofillTestFormSubmission, ProbableSubmission) {
  base::RunLoop run_loop;
  EXPECT_CALL(*autofill_manager(),
              OnFormSubmittedImpl(
                  _, _, mojom::SubmissionSource::PROBABLY_FORM_SUBMITTED))
      .Times(1)
      .WillRepeatedly(
          testing::InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  // Add a delay before navigating away to avoid race conditions. This is
  // appropriate since we're faking user interaction here.
  ExecuteScript(
      "document.getElementById('name').focus();"
      "document.getElementById('name').value = 'Sarah';"
      "document.getElementById('name').select();"
      "document.getElementById('state').selectedIndex = 1;"
      "document.getElementById('zip').focus();"
      "document.getElementById('name').select();"
      "setTimeout(function() { window.location.assign('/success.html'); }, "
      "50);");
  run_loop.Run();
}

INSTANTIATE_TEST_SUITE_P(AutofillTest,
                         AutofillTestFormSubmission,
                         testing::Combine(testing::Bool(), testing::Bool()));

}  // namespace autofill
