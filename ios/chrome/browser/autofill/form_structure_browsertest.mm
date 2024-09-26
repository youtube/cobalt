// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <WebKit/WebKit.h>
#import <vector>

#import "base/files/file_enumerator.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/mac/foundation_util.h"
#import "base/memory/ptr_util.h"
#import "base/path_service.h"
#import "base/strings/string_util.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/task/thread_pool/thread_pool_instance.h"
#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "components/autofill/core/browser/browser_autofill_manager.h"
#import "components/autofill/core/browser/form_structure.h"
#import "components/autofill/core/browser/test_autofill_manager_waiter.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/core/common/autofill_payments_features.h"
#import "components/autofill/core/common/unique_ids.h"
#import "components/autofill/ios/browser/autofill_agent.h"
#import "components/autofill/ios/browser/autofill_driver_ios.h"
#import "components/autofill/ios/browser/autofill_driver_ios_factory.h"
#import "components/autofill/ios/browser/test_autofill_manager_injector.h"
#import "components/autofill/ios/form_util/form_util_java_script_feature.h"
#import "components/autofill/ios/form_util/unique_id_data_tab_helper.h"
#import "components/password_manager/core/browser/mock_password_store_interface.h"
#import "components/password_manager/core/browser/password_manager_test_utils.h"
#import "components/sync_user_events/fake_user_event_service.h"
#import "ios/chrome/browser/autofill/address_normalizer_factory.h"
#import "ios/chrome/browser/autofill/form_suggestion_controller.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#import "ios/chrome/browser/passwords/password_controller.h"
#import "ios/chrome/browser/paths/paths.h"
#import "ios/chrome/browser/sync/ios_user_event_service_factory.h"
#import "ios/chrome/browser/ui/autofill/chrome_autofill_client_ios.h"
#import "ios/chrome/browser/web/chrome_web_client.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/task_observer_util.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/test/web_view_interaction_test_util.h"
#import "ios/web/public/web_state.h"
#import "testing/data_driven_testing/data_driven_test.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace autofill {

namespace {

const base::FilePath::CharType kFeatureName[] = FILE_PATH_LITERAL("autofill");
const base::FilePath::CharType kTestName[] = FILE_PATH_LITERAL("heuristics");

base::FilePath GetTestDataDir() {
  base::FilePath dir;
  base::PathService::Get(ios::DIR_TEST_DATA, &dir);
  return dir;
}

base::FilePath GetIOSInputDirectory() {
  base::FilePath dir;
  CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &dir));

  return dir.AppendASCII("components")
      .AppendASCII("test")
      .AppendASCII("data")
      .Append(kFeatureName)
      .Append(kTestName)
      .AppendASCII("input");
}

base::FilePath GetIOSOutputDirectory() {
  base::FilePath dir;
  CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &dir));

  return dir.AppendASCII("components")
      .AppendASCII("test")
      .AppendASCII("data")
      .Append(kFeatureName)
      .Append(kTestName)
      .AppendASCII("output");
}

const std::vector<base::FilePath> GetTestFiles() {
  base::FilePath dir(GetIOSInputDirectory());
  std::string input_list_string;
  if (!base::ReadFileToString(dir.AppendASCII("autofill_test_files"),
                              &input_list_string)) {
    return {};
  }
  std::vector<base::FilePath> result;
  for (const base::StringPiece& piece :
       base::SplitStringPiece(input_list_string, "\n", base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    result.push_back(dir.AppendASCII(piece));
  }
  return result;
}

}  // namespace

// Test fixture for verifying Autofill heuristics. Each input is an HTML
// file that contains one or more forms. The corresponding output file lists the
// heuristically detected type for each field.
// This is based on FormStructureBrowserTest from the Chromium Project.
// TODO(crbug.com/245246): Unify the tests.
class FormStructureBrowserTest
    : public PlatformTest,
      public testing::DataDrivenTest,
      public testing::WithParamInterface<base::FilePath> {
 public:
  FormStructureBrowserTest(const FormStructureBrowserTest&) = delete;
  FormStructureBrowserTest& operator=(const FormStructureBrowserTest&) = delete;

 protected:
  class TestAutofillClient : public ChromeAutofillClientIOS {
   public:
    using ChromeAutofillClientIOS::ChromeAutofillClientIOS;
    AutofillDownloadManager* GetDownloadManager() override { return nullptr; }
  };

  class TestAutofillManager : public BrowserAutofillManager {
   public:
    TestAutofillManager(AutofillDriverIOS* driver, AutofillClient* client)
        : BrowserAutofillManager(driver, client, "en-US") {}

    TestAutofillManagerWaiter& waiter() { return waiter_; }

   private:
    TestAutofillManagerWaiter waiter_{*this,
                                      {AutofillManagerEvent::kFormsSeen}};
  };

  FormStructureBrowserTest();
  ~FormStructureBrowserTest() override {}

  void SetUp() override;
  void TearDown() override;

  bool LoadHtmlWithoutSubresourcesAndInitRendererIds(const std::string& html);

  // DataDrivenTest:
  void GenerateResults(const std::string& input, std::string* output) override;

  // Serializes the given `forms` into a string.
  std::string FormStructuresToString(
      const std::map<FormGlobalId, std::unique_ptr<FormStructure>>& forms);

  web::WebState* web_state() const { return web_state_.get(); }

  web::ScopedTestingWebClient web_client_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<web::WebState> web_state_;
  std::unique_ptr<TestAutofillClient> autofill_client_;
  AutofillAgent* autofill_agent_;
  std::unique_ptr<TestAutofillManagerInjector<TestAutofillManager>>
      autofill_manager_injector_;
  FormSuggestionController* suggestion_controller_;

 private:
  base::test::ScopedFeatureList feature_list_;
  PasswordController* password_controller_;
};

FormStructureBrowserTest::FormStructureBrowserTest()
    : DataDrivenTest(GetTestDataDir(), kFeatureName, kTestName),
      web_client_(std::make_unique<ChromeWebClient>()) {
  TestChromeBrowserState::Builder builder;
  builder.AddTestingFactory(
      IOSChromePasswordStoreFactory::GetInstance(),
      base::BindRepeating(&password_manager::BuildPasswordStoreInterface<
                          web::BrowserState,
                          password_manager::MockPasswordStoreInterface>));
  builder.AddTestingFactory(
      IOSUserEventServiceFactory::GetInstance(),
      base::BindRepeating(
          [](web::BrowserState*) -> std::unique_ptr<KeyedService> {
            return std::make_unique<syncer::FakeUserEventService>();
          }));
  browser_state_ = builder.Build();

  web::WebState::CreateParams params(browser_state_.get());
  web_state_ = web::WebState::Create(params);
  feature_list_.InitWithFeatures(
      // Enabled
      {// TODO(crbug.com/1076175) Remove once launched.
       autofill::features::kAutofillUseNewSectioningMethod,
       // TODO(crbug.com/1157405) Remove once launched.
       features::kAutofillEnableDependentLocalityParsing,
       // TODO(crbug.com/1165780): Remove once shared labels are launched.
       features::kAutofillEnableSupportForParsingWithSharedLabels,
       // TODO(crbug.com/1150895) Remove once launched.
       features::kAutofillParsingPatternProvider,
       features::kAutofillPageLanguageDetection,
       // TODO(crbug.com/1335549): Remove once launched.
       features::kAutofillParseIBANFields,
       // TODO(crbug.com/1311937): Remove once launched.
       features::kAutofillEnableSupportForPhoneNumberTrunkTypes,
       features::kAutofillInferCountryCallingCode,
       // TODO(crbug.com/1355264): Remove once launched.
       features::kAutofillLabelAffixRemoval},
      // Disabled
      {// TODO(crbug.com/1311937): Remove once launched.
       // This feature is part of the AutofillRefinedPhoneNumberTypes rollout.
       // As it is not supported on iOS yet, it is disabled.
       features::kAutofillConsiderPhoneNumberSeparatorsValidLabels,
       // TODO(crbug.com/1317961): Remove once launched. This feature is
       // disabled since it is not supported on iOS.
       features::kAutofillAlwaysParsePlaceholders});
}

void FormStructureBrowserTest::SetUp() {
  PlatformTest::SetUp();

  // Create a PasswordController instance that will handle set up for renderer
  // ids.
  UniqueIDDataTabHelper::CreateForWebState(web_state());
  password_controller_ =
      [[PasswordController alloc] initWithWebState:web_state()];

  // AddressNormalizerFactory must be initialized in a blocking allowed scoped.
  // Initialize it now as it may DCHECK if it is initialized during the test.
  AddressNormalizerFactory::GetInstance();

  autofill_agent_ =
      [[AutofillAgent alloc] initWithPrefService:browser_state_->GetPrefs()
                                        webState:web_state()];
  suggestion_controller_ =
      [[FormSuggestionController alloc] initWithWebState:web_state()
                                               providers:@[ autofill_agent_ ]];

  InfoBarManagerImpl::CreateForWebState(web_state());
  infobars::InfoBarManager* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state());
  autofill_client_ = std::make_unique<TestAutofillClient>(
      browser_state_.get(), web_state(), infobar_manager, autofill_agent_,
      /*password_generation_manager=*/nullptr);

  std::string locale("en");
  autofill::AutofillDriverIOSFactory::CreateForWebState(
      web_state(), autofill_client_.get(), /*autofill_agent=*/nil, locale);

  autofill_manager_injector_ =
      std::make_unique<TestAutofillManagerInjector<TestAutofillManager>>(
          web_state());
}

void FormStructureBrowserTest::TearDown() {
  web::test::WaitForBackgroundTasks();
  web_state_.reset();
}

bool FormStructureBrowserTest::LoadHtmlWithoutSubresourcesAndInitRendererIds(
    const std::string& html) {
  bool success = web::test::LoadHtmlWithoutSubresources(
      base::SysUTF8ToNSString(html), web_state());
  if (!success) {
    return false;
  }

  autofill::FormUtilJavaScriptFeature* feature =
      autofill::FormUtilJavaScriptFeature::GetInstance();

  __block web::WebFrame* main_frame = nullptr;
  success = WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    web::WebFramesManager* frames_manager =
        feature->GetWebFramesManager(web_state());
    main_frame = frames_manager->GetMainWebFrame();
    return main_frame != nullptr;
  });
  if (!success) {
    return false;
  }
  DCHECK(main_frame);

  uint32_t next_available_id = 1;
  feature->SetUpForUniqueIDsWithInitialState(main_frame, next_available_id);

  // Wait for `SetUpForUniqueIDsWithInitialState` to complete.
  return WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    return [web::test::ExecuteJavaScriptForFeature(
               web_state(), @"document[__gCrWeb.fill.ID_SYMBOL]", feature)
               intValue] == static_cast<int>(next_available_id);
  });
}

void FormStructureBrowserTest::GenerateResults(const std::string& input,
                                               std::string* output) {
  ASSERT_TRUE(LoadHtmlWithoutSubresourcesAndInitRendererIds(input));
  base::ThreadPoolInstance::Get()->FlushForTesting();
  TestAutofillManager* autofill_manager =
      autofill_manager_injector_->GetForMainFrame();
  ASSERT_NE(nullptr, autofill_manager);
  ASSERT_TRUE(autofill_manager->waiter().Wait(1));
  *output = FormStructuresToString(autofill_manager->form_structures());
}

std::string FormStructureBrowserTest::FormStructuresToString(
    const std::map<FormGlobalId, std::unique_ptr<FormStructure>>& forms) {
  std::string forms_string;
  // The forms are sorted by their global ID, which should make the order
  // deterministic.
  for (const auto& form_kv : forms) {
    const auto* form = form_kv.second.get();
    std::map<std::string, int> section_to_index;
    for (const auto& field : *form) {
      std::string name = base::UTF16ToUTF8(field->name);
      if (base::StartsWith(name, "gChrome~field~",
                           base::CompareCase::SENSITIVE)) {
        // The name has been generated by iOS JavaScript. Output an empty name
        // to have a behavior similar to other platforms.
        name = "";
      }

      std::string section = field->section.ToString();
      if (base::StartsWith(section, "gChrome~field~",
                           base::CompareCase::SENSITIVE)) {
        // The name has been generated by iOS JavaScript. Output an empty name
        // to have a behavior similar to other platforms.
        size_t first_underscore = section.find_first_of('_');
        section = section.substr(first_underscore);
      }

      if (field->section.is_from_fieldidentifier()) {
        // Normalize the section by replacing the unique but platform-dependent
        // integers in `field->section` with consecutive unique integers.
        // The section string is of the form "fieldname_id1_id2-suffix", where
        // id1, id2 are platform-dependent and thus need to be substituted.
        size_t last_underscore = section.find_last_of('_');
        size_t second_last_underscore =
            section.find_last_of('_', last_underscore - 1);
        int new_section_index = static_cast<int>(section_to_index.size() + 1);
        int section_index =
            section_to_index.insert(std::make_pair(section, new_section_index))
                .first->second;
        if (second_last_underscore != std::string::npos) {
          section = base::StringPrintf(
              "%s%d", section.substr(0, second_last_underscore + 1).c_str(),
              section_index);
        }
      }

      forms_string += field->Type().ToString();
      forms_string += " | " + name;
      forms_string += " | " + base::UTF16ToUTF8(field->label);
      forms_string += " | " + base::UTF16ToUTF8(field->value);
      forms_string += " | " + section;
      forms_string += "\n";
    }
  }
  return forms_string;
}

namespace {

// To disable a data driven test, please add the name of the test file
// (i.e., "NNN_some_site.html") as a literal to the initializer_list given
// to the failing_test_names constructor.
const auto& GetFailingTestNames() {
  static std::set<std::string> failing_test_names{
      // TODO(crbug.com/1187842): These pages contains iframes. Until filling
      // across iframes is also supported on iOS, iOS has has different
      // expectations compared to non-iOS platforms.
      "049_register_ebay.com.html",
      "148_payment_dickblick.com.html",
      // TODO(crbug.com/1339277): These pages contain labels which are only
      // inferred using AutofillImprovedLabelForInference. This feature is
      // currently not available on iOS.
      "074_register_threadless.com.html",
      "097_register_alaskaair.com.html",
      "115_checkout_walgreens.com.html",
      "116_cc_checkout_walgreens.com.html",
      "150_checkout_venus.com_search_field.html",
  };
  return failing_test_names;
}

}  // namespace

// If disabling a test, prefer to add the name names of the specific test cases
// to GetFailingTestNames(), directly above, instead of renaming the test to
// DISABLED_DataDrivenHeuristics.
TEST_P(FormStructureBrowserTest, DataDrivenHeuristics) {
  bool is_expected_to_pass =
      !base::Contains(GetFailingTestNames(), GetParam().BaseName().value());
  RunOneDataDrivenTest(GetParam(), GetIOSOutputDirectory(),
                       is_expected_to_pass);
}

INSTANTIATE_TEST_SUITE_P(AllForms,
                         FormStructureBrowserTest,
                         testing::ValuesIn(GetTestFiles()));

}  // namespace autofill
