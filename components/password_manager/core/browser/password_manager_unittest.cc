// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager.h"

#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/browser/affiliation/affiliated_match_helper.h"
#include "components/password_manager/core/browser/affiliation/fake_affiliation_service.h"
#include "components/password_manager/core/browser/affiliation/mock_affiliated_match_helper.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/field_info_manager.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_check.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_check_factory.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_request_utils.h"
#include "components/password_manager/core/browser/leak_detection/mock_leak_detection_check_factory.h"
#include "components/password_manager/core/browser/mock_password_reuse_manager.h"
#include "components/password_manager/core/browser/mock_webauthn_credentials_delegate.h"
#include "components/password_manager/core/browser/password_autofill_manager.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_store_interface.h"
#include "components/password_manager/core/browser/possible_username_data.h"
#include "components/password_manager/core/browser/stub_credentials_filter.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/core/browser/votes_uploader.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/ukm/test_ukm_recorder.h"
#include "net/cert/cert_status_flags.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/network/test/test_network_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/webauthn/android/cred_man_support.h"
#include "components/webauthn/android/webauthn_cred_man_delegate.h"
#endif  // BUILDFLAG(IS_ANDROID)

using ServerPrediction = autofill::AutofillType::ServerPrediction;
using autofill::FieldGlobalId;
using autofill::FieldRendererId;
using autofill::FormData;
using autofill::FormFieldData;
using autofill::FormRendererId;
using autofill::NO_SERVER_DATA;
using autofill::NOT_USERNAME;
using autofill::PasswordFormFillData;
using autofill::ServerFieldType;
using autofill::SINGLE_USERNAME;
using ::autofill::test::CreateFieldPrediction;
using base::ASCIIToUTF16;
using base::Feature;
using base::TestMockTimeTaskRunner;
using base::test::FeatureRef;
using testing::_;
using testing::AllOf;
using testing::AnyNumber;
using testing::ByMove;
using testing::ElementsAre;
using testing::Field;
using testing::Invoke;
using testing::IsEmpty;
using testing::IsNull;
using testing::Mock;
using testing::NotNull;
using testing::Pair;
using testing::Return;
using testing::ReturnRef;
using testing::SaveArg;
using testing::SizeIs;
using testing::UnorderedElementsAre;
using testing::WithArg;

namespace password_manager {

namespace {

MATCHER_P2(FormUsernamePasswordAre, username, password, "") {
  return arg.username_value == username && arg.password_value == password;
}

MATCHER_P(FormHasUniqueKey, key, "") {
  return ArePasswordFormUniqueKeysEqual(arg, key);
}

MATCHER_P(FormIgnoreDate, expected, "") {
  PasswordForm expected_with_date = expected;
  expected_with_date.date_created = arg.date_created;
  return arg == expected_with_date;
}

MATCHER_P(HasUsernameValue, expected_username, "") {
  return arg.username_value == expected_username;
}

class FakeNetworkContext : public network::TestNetworkContext {
 public:
  FakeNetworkContext() = default;
  void IsHSTSActiveForHost(const std::string& host,
                           IsHSTSActiveForHostCallback callback) override {
    std::move(callback).Run(true);
  }
};

class MockLeakDetectionCheck : public LeakDetectionCheck {
 public:
  MOCK_METHOD(
      void,
      Start,
      (LeakDetectionInitiator, const GURL&, std::u16string, std::u16string),
      (override));
};

class MockStoreResultFilter : public StubCredentialsFilter {
 public:
  MOCK_CONST_METHOD1(ShouldSave, bool(const PasswordForm& form));
  MOCK_CONST_METHOD1(ReportFormLoginSuccess,
                     void(const PasswordFormManager& form_manager));
  MOCK_CONST_METHOD1(IsSyncAccountEmail, bool(const std::string&));
  MOCK_CONST_METHOD1(ShouldSaveGaiaPasswordHash, bool(const PasswordForm&));
  MOCK_CONST_METHOD1(ShouldSaveEnterprisePasswordHash,
                     bool(const PasswordForm&));
};

class MockPasswordManagerClient : public StubPasswordManagerClient {
 public:
  MockPasswordManagerClient() {
    ON_CALL(*this, GetStoreResultFilter()).WillByDefault(Return(&filter_));
    ON_CALL(filter_, ShouldSave(_)).WillByDefault(Return(true));
    ON_CALL(filter_, ShouldSaveGaiaPasswordHash(_))
        .WillByDefault(Return(false));
    ON_CALL(filter_, ShouldSaveEnterprisePasswordHash(_))
        .WillByDefault(Return(false));
    ON_CALL(filter_, IsSyncAccountEmail(_)).WillByDefault(Return(false));
    ON_CALL(*this, IsNewTabPage()).WillByDefault(Return(false));

    ON_CALL(*this, GetWebAuthnCredentialsDelegateForDriver)
        .WillByDefault(Return(&webauthn_credentials_delegate_));
    ON_CALL(webauthn_credentials_delegate_, GetPasskeys)
        .WillByDefault(ReturnRef(passkeys_));
    ON_CALL(webauthn_credentials_delegate_,
            OfferPasskeysFromAnotherDeviceOption)
        .WillByDefault(Return(true));
  }

  MOCK_METHOD(bool,
              IsSavingAndFillingEnabled,
              (const GURL&),
              (const, override));
  MOCK_METHOD(net::CertStatus, GetMainFrameCertStatus, (), (const, override));
  MOCK_METHOD(void,
              AutofillHttpAuth,
              (const PasswordForm&, const PasswordFormManagerForUI*),
              (override));
  MOCK_METHOD(PasswordStoreInterface*,
              GetProfilePasswordStore,
              (),
              (const, override));
  MOCK_METHOD(PasswordStoreInterface*,
              GetAccountPasswordStore,
              (),
              (const, override));
  MOCK_METHOD(PasswordReuseManager*,
              GetPasswordReuseManager,
              (),
              (const, override));
  MOCK_METHOD(bool,
              PromptUserToSaveOrUpdatePassword,
              (std::unique_ptr<PasswordFormManagerForUI>, bool),
              (override));
  MOCK_METHOD(void,
              ShowManualFallbackForSaving,
              (std::unique_ptr<PasswordFormManagerForUI>, bool, bool),
              (override));
  MOCK_METHOD(void, HideManualFallbackForSaving, (), (override));
  MOCK_METHOD(void,
              NotifySuccessfulLoginWithExistingPassword,
              (std::unique_ptr<PasswordFormManagerForUI>),
              (override));
  MOCK_METHOD(void,
              NotifyOnSuccessfulLogin,
              (const std::u16string& submitted_username),
              (override));
  MOCK_METHOD(void, ResetSubmissionTrackingAfterTouchToFill, (), (override));
  MOCK_METHOD(void,
              AutomaticPasswordSave,
              (std::unique_ptr<PasswordFormManagerForUI>,
               bool is_update_confirmation),
              (override));
  MOCK_METHOD(PrefService*, GetPrefs, (), (const, override));
  MOCK_METHOD(PrefService*, GetLocalStatePrefs, (), (const, override));
  MOCK_METHOD(const GURL&, GetLastCommittedURL, (), (const, override));
  MOCK_METHOD(url::Origin, GetLastCommittedOrigin, (), (const, override));
  MOCK_METHOD(bool, IsCommittedMainFrameSecure, (), (const, override));
  MOCK_METHOD(const MockStoreResultFilter*,
              GetStoreResultFilter,
              (),
              (const, override));
  MOCK_METHOD(PasswordManagerMetricsRecorder*,
              GetMetricsRecorder,
              (),
              (override));
  MOCK_METHOD(bool, IsNewTabPage, (), (const, override));
  MOCK_METHOD(SyncState, GetPasswordSyncState, (), (const, override));
  MOCK_METHOD(profile_metrics::BrowserProfileType,
              GetProfileType,
              (),
              (const, override));
  MOCK_METHOD(version_info::Channel, GetChannel, (), (const override));
  MOCK_METHOD(void,
              RefreshPasswordManagerSettingsIfNeeded,
              (),
              (const, override));
  MOCK_METHOD(FieldInfoManager*, GetFieldInfoManager, (), (const, override));
  MOCK_METHOD(WebAuthnCredentialsDelegate*,
              GetWebAuthnCredentialsDelegateForDriver,
              (PasswordManagerDriver*),
              (override));

  network::mojom::NetworkContext* GetNetworkContext() const override {
    return &network_context_;
  }

  void FilterAllResultsForSaving() {
    EXPECT_CALL(filter_, ShouldSave(_)).WillRepeatedly(Return(false));
  }

  testing::NiceMock<MockStoreResultFilter>* filter() { return &filter_; }

 private:
  mutable FakeNetworkContext network_context_;
  testing::NiceMock<MockStoreResultFilter> filter_;
  MockWebAuthnCredentialsDelegate webauthn_credentials_delegate_;
  absl::optional<std::vector<PasskeyCredential>> passkeys_;
};

class MockPasswordManagerDriver : public StubPasswordManagerDriver {
 public:
  MockPasswordManagerDriver() {
    ON_CALL(*this, GetId()).WillByDefault(Return(0));
    ON_CALL(*this, IsInPrimaryMainFrame()).WillByDefault(Return(true));
  }

  MOCK_METHOD(int, GetId, (), (const, override));
  MOCK_METHOD(void,
              FormEligibleForGenerationFound,
              (const autofill::PasswordFormGenerationData&),
              (override));
  MOCK_METHOD(void,
              SetPasswordFillData,
              (const autofill::PasswordFormFillData&),
              (override));
  MOCK_METHOD(PasswordManager*, GetPasswordManager, (), (override));
  MOCK_METHOD(PasswordAutofillManager*,
              GetPasswordAutofillManager,
              (),
              (override));
  MOCK_METHOD(bool, IsInPrimaryMainFrame, (), (const, override));
  MOCK_METHOD(const GURL&, GetLastCommittedURL, (), (const, override));
};

// Invokes the password store consumer with a single copy of |form|.
ACTION_P2(InvokeConsumer, store, form) {
  std::vector<std::unique_ptr<PasswordForm>> result;
  result.push_back(std::make_unique<PasswordForm>(form));
  arg0->OnGetPasswordStoreResultsOrErrorFrom(store, std::move(result));
}

void SanitizeFormData(FormData* form) {
  form->main_frame_origin = url::Origin();
  for (FormFieldData& field : form->fields) {
    field.label.clear();
    field.value.clear();
    field.autocomplete_attribute.clear();
    field.options.clear();
    field.placeholder.clear();
    field.css_classes.clear();
    field.id_attribute.clear();
    field.name_attribute.clear();
  }
}

// Verifies that |test_ukm_recorder| recorder has a single entry called |entry|
// and returns it.
const ukm::mojom::UkmEntry* GetMetricEntry(
    const ukm::TestUkmRecorder& test_ukm_recorder,
    base::StringPiece entry) {
  std::vector<const ukm::mojom::UkmEntry*> ukm_entries =
      test_ukm_recorder.GetEntriesByName(entry);
  EXPECT_EQ(1u, ukm_entries.size());
  return ukm_entries[0];
}

// Verifies the expectation that |test_ukm_recorder| recorder has a single entry
// called |entry|, and that the entry contains the metric called |metric| set
// to |value|.
template <typename T>
void CheckMetricHasValue(const ukm::TestUkmRecorder& test_ukm_recorder,
                         base::StringPiece entry,
                         base::StringPiece metric,
                         T value) {
  ukm::TestUkmRecorder::ExpectEntryMetric(
      GetMetricEntry(test_ukm_recorder, entry), metric,
      static_cast<int64_t>(value));
}

class FailingPasswordStoreBackend : public FakePasswordStoreBackend {
  void InitBackend(AffiliatedMatchHelper* affiliated_match_helper,
                   RemoteChangesReceived remote_form_changes_received,
                   base::RepeatingClosure sync_enabled_or_disabled_cb,
                   base::OnceCallback<void(bool)> completion) override {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(completion), /*success=*/false));
  }
};

// Creates a set map of `ServerPrediction`s for `form` according to the
// specified `types`. `types` is a map of the fields index in `form.fields` to
// the `ServerFieldType`.
base::flat_map<FieldGlobalId, ServerPrediction> CreateServerPredictions(
    const FormData& form,
    const base::flat_map<size_t, ServerFieldType>& types,
    bool is_override = false) {
  base::flat_map<FieldGlobalId, ServerPrediction> result;
  for (size_t i = 0; i < form.fields.size(); ++i) {
    ServerPrediction prediction;
    if (auto it = types.find(i); it != types.end()) {
      prediction.server_predictions = {
          CreateFieldPrediction(it->second, is_override)};
    }
    result.insert({form.fields[i].global_id(), std::move(prediction)});
  }
  return result;
}

}  // namespace

// The main test class which defines common mocks.
class PasswordManagerTest : public testing::Test {
 public:
  PasswordManagerTest() : task_runner_(new TestMockTimeTaskRunner) {}
  ~PasswordManagerTest() override = default;

 protected:
  void SetUp() override {
    store_ = base::MakeRefCounted<TestPasswordStore>();
    auto owning_mock_match_helper =
        std::make_unique<testing::NiceMock<MockAffiliatedMatchHelper>>(
            &fake_affiliation_service_);
    mock_match_helper_ = owning_mock_match_helper.get();
    store_->Init(
        /*prefs=*/nullptr,
        /*affiliated_match_helper=*/std::move(owning_mock_match_helper));

    ON_CALL(client_, GetProfilePasswordStore())
        .WillByDefault(Return(store_.get()));

    if (base::FeatureList::IsEnabled(
            features::kEnablePasswordsAccountStorage)) {
      account_store_ =
          base::MakeRefCounted<TestPasswordStore>(IsAccountStore(true));
      account_store_->Init(/*prefs=*/nullptr,
                           /*affiliated_match_helper=*/nullptr);

      ON_CALL(client_, GetAccountPasswordStore())
          .WillByDefault(Return(account_store_.get()));
    }

    ON_CALL(client_, GetPasswordReuseManager())
        .WillByDefault(Return(&reuse_manager_));

    manager_ = std::make_unique<PasswordManager>(&client_);
    manager_->set_leak_factory(
        std::make_unique<testing::NiceMock<MockLeakDetectionCheckFactory>>());
    password_autofill_manager_ =
        std::make_unique<PasswordAutofillManager>(&driver_, nullptr, &client_);

    EXPECT_CALL(driver_, GetPasswordManager())
        .WillRepeatedly(Return(manager_.get()));
    EXPECT_CALL(driver_, GetPasswordAutofillManager())
        .WillRepeatedly(Return(password_autofill_manager_.get()));
    ON_CALL(client_, GetMainFrameCertStatus()).WillByDefault(Return(0));

    ON_CALL(client_, GetLastCommittedURL)
        .WillByDefault(ReturnRef(test_form_url_));
    ON_CALL(client_, GetLastCommittedOrigin)
        .WillByDefault(Return(url::Origin::Create(test_form_url_)));
    ON_CALL(client_, IsCommittedMainFrameSecure()).WillByDefault(Return(true));
    ON_CALL(client_, GetMetricsRecorder()).WillByDefault(Return(nullptr));

    prefs_ = std::make_unique<TestingPrefServiceSimple>();
    prefs_->registry()->RegisterBooleanPref(
        prefs::kPasswordLeakDetectionEnabled, true);
    prefs_->registry()->RegisterBooleanPref(::prefs::kSafeBrowsingEnabled,
                                            true);
    prefs_->registry()->RegisterBooleanPref(::prefs::kSafeBrowsingEnhanced,
                                            true);
    prefs_->registry()->RegisterTimePref(
        prefs::kProfileStoreDateLastUsedForFilling, base::Time());
    prefs_->registry()->RegisterTimePref(
        prefs::kAccountStoreDateLastUsedForFilling, base::Time());
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
    prefs_->registry()->RegisterBooleanPref(
        password_manager::prefs::kBiometricAuthenticationBeforeFilling, true);
#endif
    ON_CALL(client_, GetPrefs()).WillByDefault(Return(prefs_.get()));

    field_info_manager_ = std::make_unique<FieldInfoManager>(task_runner_);
    ON_CALL(client_, GetFieldInfoManager())
        .WillByDefault(Return(field_info_manager_.get()));

    // When waiting for predictions is on, it makes tests more complicated.
    // Disable waiting, since most tests have nothing to do with predictions.
    // All tests that test working with prediction should explicitly turn
    // predictions on.
    PasswordFormManager::set_wait_for_server_predictions_for_filling(false);
#if BUILDFLAG(IS_ANDROID)
    webauthn::WebAuthnCredManDelegate::override_cred_man_support_for_testing(
        webauthn::CredManSupport::DISABLED);
#endif  // BUILDFLAG(IS_ANDROID)
  }

  void TearDown() override {
    // The PasswordManager may own objects that keep raw pointers for the
    // password store - therefore reset it first.
    manager_.reset();
    mock_match_helper_ = nullptr;
    if (account_store_) {
      account_store_->ShutdownOnUIThread();
      account_store_ = nullptr;
    }
    store_->ShutdownOnUIThread();
    store_ = nullptr;
  }

  PasswordForm MakeSavedForm() {
    PasswordForm form;
    form.url = test_form_url_;
    form.action = test_form_action_;
    form.username_element = u"Email";
    form.password_element = u"Passwd";
    form.username_value = u"googleuser";
    form.password_value = u"p4ssword";
    form.submit_element = u"signIn";
    form.signon_realm = test_signon_realm_;
    form.in_store = PasswordForm::Store::kProfileStore;
    form.match_type = PasswordForm::MatchType::kExact;
    return form;
  }

  PasswordForm MakeSimpleForm() {
    PasswordForm form = MakeSavedForm();
    form.form_data = MakeSimpleFormData();
    return form;
  }

  FormData MakeSimpleFormData() {
    FormData form_data;
    form_data.url = test_form_url_;
    form_data.action = test_form_action_;
    form_data.name = u"the-form-name";
    form_data.unique_renderer_id = FormRendererId(10);

    FormFieldData field;
    field.name = u"Email";
    field.id_attribute = field.name;
    field.name_attribute = field.name;
    field.value = u"googleuser";
    field.form_control_type = autofill::FormControlType::kInputText;
    field.unique_renderer_id = FieldRendererId(2);
    form_data.fields.push_back(field);

    field.name = u"Passwd";
    field.id_attribute = field.name;
    field.name_attribute = field.name;
    field.value = u"p4ssword";
    field.form_control_type = autofill::FormControlType::kInputPassword;
    field.unique_renderer_id = FieldRendererId(3);
    form_data.fields.push_back(field);

    return form_data;
  }

  PasswordForm MakeSimpleGAIAForm() {
    PasswordForm form = MakeSimpleForm();
    form.form_data.url = GURL("https://accounts.google.com");
    form.url = GURL("https://accounts.google.com");
    form.signon_realm = form.url.spec();
    return form;
  }

  FormData MakeSimpleGAIAFormData() {
    FormData form_data = MakeSimpleFormData();
    form_data.url = GURL("https://accounts.google.com");
    return form_data;
  }

  FormData MakeGAIAChangePasswordFormData() {
    FormData form_data(MakeSimpleFormData());
    form_data.fields[1].autocomplete_attribute = "new-password";
    form_data.url = GURL("https://accounts.google.com");
    form_data.action = GURL("http://www.google.com/a/Login");
    form_data.name = u"the-form-name";
    return form_data;
  }

  // Create a sign-up form that only has a new password field.
  PasswordForm MakeFormWithOnlyNewPasswordField() {
    PasswordForm form = MakeSimpleForm();
    form.new_password_element.swap(form.password_element);
    form.new_password_value.swap(form.password_value);
    form.form_data.fields[1].autocomplete_attribute = "new-password";
    return form;
  }

  // Create a sign-up FormData that has username and new password fields.
  FormData MakeSignUpFormData() {
    FormData form_data = MakeSimpleFormData();
    form_data.fields[1].autocomplete_attribute = "new-password";
    return form_data;
  }

  // Create a FormData that only has a new password field.
  FormData MakeFormDataWithOnlyNewPasswordField() {
    FormData form_data = MakeSignUpFormData();
    form_data.fields.erase(std::begin(form_data.fields));
    return form_data;
  }

  PasswordForm MakeAndroidCredential() {
    PasswordForm android_form;
    android_form.url = GURL("android://hash@google.com");
    android_form.signon_realm = "android://hash@google.com";
    android_form.username_value = u"google";
    android_form.password_value = u"password";
    android_form.in_store = PasswordForm::Store::kProfileStore;
    android_form.match_type = PasswordForm::MatchType::kAffiliated;
    return android_form;
  }

  FormData MakeSingleUsernameFormData() {
    FormData form_data;
    form_data.name = u"username_only_form";
    form_data.url = test_form_url_;
    form_data.unique_renderer_id = FormRendererId(30);

    FormFieldData field;
    field.name = u"Email";
    field.id_attribute = field.name;
    field.name_attribute = field.name;
    field.form_control_type = autofill::FormControlType::kInputText;
    field.unique_renderer_id = FieldRendererId(31);
    form_data.fields.push_back(field);
    return form_data;
  }

  PasswordForm MakeSimpleFormWithOnlyUsernameField() {
    PasswordForm form;
    form.url = test_form_url_;
    form.username_element = u"Email";
    form.submit_element = u"signIn";
    form.signon_realm = test_signon_realm_;
    form.form_data = MakeSingleUsernameFormData();
    return form;
  }

  PasswordForm MakeSimpleFormWithOnlyPasswordField() {
    PasswordForm form(MakeSimpleForm());
    form.username_element.clear();
    form.username_value.clear();
    // Remove username field in |form_data|.
    form.form_data.fields.erase(form.form_data.fields.begin());
    return form;
  }

  PasswordForm MakeSimpleFormWithPasswordAndCaptchaFields() {
    PasswordForm form;
    form.url = test_form_url_;
    form.signon_realm = form.url.GetWithEmptyPath().spec();
    form.password_element = u"password_element";
    form.password_value = u"password_value";
    form.form_data.url = form.url;

    FormFieldData password_field;
    password_field.name = form.password_element;
    password_field.id_attribute = form.password_element;
    password_field.value = form.password_value;
    password_field.form_control_type =
        autofill::FormControlType::kInputPassword;
    password_field.unique_renderer_id = FieldRendererId(2);
    form.form_data.fields.push_back(password_field);

    FormFieldData captcha_field;
    captcha_field.name = u"captcha_element";
    captcha_field.id_attribute = captcha_field.name;
    captcha_field.value = u"captcha_value";
    captcha_field.form_control_type = autofill::FormControlType::kInputText;
    captcha_field.unique_renderer_id = FieldRendererId(3);
    form.form_data.fields.push_back(captcha_field);

    return form;
  }

  PasswordForm MakeSimpleFormWithIntermediaryFieldInUsernameFirstFlow() {
    PasswordForm form;
    form.url = test_form_url_;
    form.username_element = u"one-time-code";
    form.signon_realm = test_signon_realm_;
    form.form_data.name = u"username_only_form";
    form.form_data.url = form.url;
    form.form_data.unique_renderer_id = FormRendererId(50);

    FormFieldData otp_field;
    otp_field.name = form.username_element;
    otp_field.name_attribute = form.username_element;
    otp_field.id_attribute = form.username_element;
    otp_field.form_control_type = autofill::FormControlType::kInputText;
    otp_field.unique_renderer_id = FieldRendererId(61);
    form.form_data.fields.push_back(otp_field);

    return form;
  }

  FormData MakeSearchBarFormData() {
    FormData form_data;
    form_data.name = u"search-bar-field";
    form_data.url = test_form_url_;
    form_data.unique_renderer_id = FormRendererId(505);

    FormFieldData search_field;
    search_field.name = u"search_bar";
    search_field.name_attribute = search_field.name;
    search_field.id_attribute = search_field.name_attribute;
    search_field.value = u"search_field_value";

    search_field.form_control_type = autofill::FormControlType::kInputText;
    search_field.unique_renderer_id = FieldRendererId(62);
    form_data.fields.push_back(search_field);

    return form_data;
  }

  PasswordForm MakeSignUpFormWithoutUsername() {
    PasswordForm form;
    form.url = test_form_url_;
    form.signon_realm = form.url.GetWithEmptyPath().spec();
    form.password_element = u"password_element";
    form.password_value = u"password_value";
    form.username_element = u"surname_element";
    form.username_value = u"Surname";
    form.form_data.url = form.url;

    FormFieldData name_field;
    name_field.name = u"name";
    name_field.id_attribute = name_field.name;
    name_field.value = u"Name";
    name_field.form_control_type = autofill::FormControlType::kInputText;
    name_field.unique_renderer_id = FieldRendererId(2);
    form.form_data.fields.push_back(name_field);

    FormFieldData surname_field;
    surname_field.name = form.username_element;
    surname_field.id_attribute = surname_field.name;
    surname_field.value = form.username_value;
    surname_field.form_control_type = autofill::FormControlType::kInputText;
    surname_field.unique_renderer_id = FieldRendererId(3);
    form.form_data.fields.push_back(surname_field);

    FormFieldData password_field;
    password_field.name = form.password_element;
    password_field.id_attribute = form.password_element;
    password_field.value = form.password_value;
    password_field.form_control_type =
        autofill::FormControlType::kInputPassword;
    password_field.unique_renderer_id = FieldRendererId(4);
    form.form_data.fields.push_back(password_field);

    return form;
  }

  PasswordForm MakeSimpleCreditCardForm() {
    PasswordForm form;
    form.url = test_form_url_;
    form.signon_realm = form.url.GetWithEmptyPath().spec();
    form.username_element = u"cc-number";
    form.password_element = u"cvc";
    form.username_value = u"1234567";
    form.password_value = u"123";
    form.form_data.url = form.url;

    FormFieldData field;
    field.name = form.username_element;
    field.id_attribute = field.name;
    field.value = form.username_value;
    field.form_control_type = autofill::FormControlType::kInputText;
    field.unique_renderer_id = FieldRendererId(2);
    field.autocomplete_attribute = "cc-name";
    form.form_data.fields.push_back(field);

    field.name = form.password_element;
    field.id_attribute = field.name;
    field.value = form.password_value;
    field.form_control_type = autofill::FormControlType::kInputPassword;
    field.unique_renderer_id = FieldRendererId(3);
    field.autocomplete_attribute = "cc-number";
    form.form_data.fields.push_back(field);

    return form;
  }

  PasswordManager* manager() { return manager_.get(); }

  void OnPasswordFormSubmitted(const FormData& form_data) {
    manager()->OnPasswordFormSubmitted(&driver_, form_data);
  }

  base::test::ScopedFeatureList feature_list_;

  const GURL test_form_url_{"https://www.google.com/a/LoginAuth"};
  const GURL test_form_action_{"https://www.google.com/a/Login"};
  const std::string test_signon_realm_ = "https://www.google.com/";
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME};
  FakeAffiliationService fake_affiliation_service_;
  scoped_refptr<TestPasswordStore> store_;
  scoped_refptr<TestPasswordStore> account_store_;
  raw_ptr<MockAffiliatedMatchHelper> mock_match_helper_ = nullptr;
  MockPasswordReuseManager reuse_manager_;
  testing::NiceMock<MockPasswordManagerClient> client_;
  MockPasswordManagerDriver driver_;
  std::unique_ptr<TestingPrefServiceSimple> prefs_;
  std::unique_ptr<PasswordAutofillManager> password_autofill_manager_;
  std::unique_ptr<PasswordManager> manager_;
  std::unique_ptr<FieldInfoManager> field_info_manager_;
  scoped_refptr<TestMockTimeTaskRunner> task_runner_;
};

MATCHER_P(FormMatches, form, "") {
  return form.signon_realm == arg.signon_realm && form.url == arg.url &&
         form.action == arg.action &&
         form.username_element == arg.username_element &&
         form.username_value == arg.username_value &&
         form.password_element == arg.password_element &&
         form.password_value == arg.password_value;
}

TEST_F(PasswordManagerTest, FormSubmitWithOnlyNewPasswordField) {
  // Test that when a form only contains a "new password" field, the form gets
  // saved and in password store, the new password value is saved as a current
  // password value.
  std::vector<FormData> observed;
  PasswordForm form(MakeFormWithOnlyNewPasswordField());
  observed.push_back(form.form_data);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();

  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.url))
      .WillRepeatedly(Return(true));
  OnPasswordFormSubmitted(form.form_data);

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword)
      .WillOnce(MoveArgAndReturn<0>(&form_manager_to_save, true));

  // Now the password manager waits for the navigation to complete.
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();

  // Simulate saving the form, as if the info bar was accepted.
  ASSERT_TRUE(form_manager_to_save);
  form_manager_to_save->Save();
  task_environment_.RunUntilIdle();

  // The value of the new password field should have been promoted to, and saved
  // to the password store as the current password.
  PasswordForm expected_form(form);
  expected_form.password_value.swap(expected_form.new_password_value);
  expected_form.password_element.swap(expected_form.new_password_element);

  EXPECT_THAT(store_->stored_passwords(),
              ElementsAre(Pair(expected_form.signon_realm,
                               ElementsAre(FormMatches(expected_form)))));
}

// Test that generated passwords are stored without asking the user.
TEST_F(PasswordManagerTest, GeneratedPasswordFormSubmitEmptyStore) {
  EXPECT_CALL(client_, IsSavingAndFillingEnabled).WillRepeatedly(Return(true));

  std::vector<FormData> observed;
  FormData form_data(MakeSignUpFormData());
  const std::u16string username = form_data.fields[0].value;
  observed.push_back(form_data);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);

  // Simulate the user generating the password and submitting the form.
  const std::u16string generated_password = u"GeNeRaTeDRaNdOmPa$$";
  manager()->OnPresaveGeneratedPassword(&driver_, form_data,
                                        generated_password);
  task_environment_.RunUntilIdle();
  EXPECT_THAT(store_->stored_passwords(), SizeIs(1));
  form_data.fields[1].value = generated_password;
  OnPasswordFormSubmitted(form_data);

  // The user should not need to confirm saving as they have already given
  // consent by using the generated password. The form should be saved once
  // navigation occurs. The client will be informed that automatic saving has
  // occurred.
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword).Times(0);
  EXPECT_CALL(client_, AutomaticPasswordSave);

  // Now the password manager waits for the navigation to complete.
  manager()->OnPasswordFormsParsed(&driver_, {});
  manager()->OnPasswordFormsRendered(&driver_, {});
  task_environment_.RunUntilIdle();

  ASSERT_THAT(store_->stored_passwords(), SizeIs(1));
  auto forms_saved = store_->stored_passwords().begin()->second;
  ASSERT_THAT(forms_saved, SizeIs(1));
  EXPECT_EQ(forms_saved[0].username_value, username);
  // What was "new password" field in the submitted form, becomes the current
  // password field in the form to save.
  EXPECT_EQ(forms_saved[0].password_value, generated_password);
}

#if BUILDFLAG(IS_IOS)
TEST_F(PasswordManagerTest, EditingGeneratedPasswordOnIOS) {
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(_))
      .WillRepeatedly(Return(true));

  FormData form_data = MakeSimpleFormData();
  std::u16string username = form_data.fields[0].value;
  std::u16string generated_password = form_data.fields[1].value + u"1";
  FieldRendererId username_element = form_data.fields[0].unique_renderer_id;
  FieldRendererId generation_element = form_data.fields[1].unique_renderer_id;

  // A form is found by PasswordManager.
  manager()->OnPasswordFormsParsed(&driver_, {form_data});

  // The user is generating the password. The password has to be presaved.
  manager()->SetGenerationElementAndTypeForForm(
      &driver_, form_data.unique_renderer_id, generation_element,
      autofill::password_generation::PasswordGenerationType::kAutomatic);
  manager()->OnPresaveGeneratedPassword(&driver_, form_data,
                                        generated_password);
  task_environment_.RunUntilIdle();
  EXPECT_THAT(
      store_->stored_passwords(),
      ElementsAre(Pair(GetSignonRealm(form_data.url),
                       ElementsAre(FormUsernamePasswordAre(
                           form_data.fields[0].value, generated_password)))));

  // Test when the user is changing the generated password, presaved credential
  // is updated.
  generated_password += u"1";
  manager()->UpdateStateOnUserInput(&driver_, form_data.unique_renderer_id,
                                    generation_element, generated_password);
  task_environment_.RunUntilIdle();
  EXPECT_THAT(
      store_->stored_passwords(),
      ElementsAre(Pair(GetSignonRealm(form_data.url),
                       ElementsAre(FormUsernamePasswordAre(
                           form_data.fields[0].value, generated_password)))));

  // Test when the user is changing the username, presaved credential is
  // updated.
  username += u"1";
  manager()->UpdateStateOnUserInput(&driver_, form_data.unique_renderer_id,
                                    username_element, username);
  task_environment_.RunUntilIdle();
  EXPECT_THAT(store_->stored_passwords(),
              ElementsAre(Pair(GetSignonRealm(form_data.url),
                               ElementsAre(FormUsernamePasswordAre(
                                   username, generated_password)))));
}

TEST_F(PasswordManagerTest, ShowHideManualFallbackOnIOS) {
  ON_CALL(client_, IsSavingAndFillingEnabled(_)).WillByDefault(Return(true));

  FormData form_data = MakeSimpleFormData();
  FieldRendererId password_element = form_data.fields[1].unique_renderer_id;

  // A form is found by PasswordManager.
  manager()->OnPasswordFormsParsed(&driver_, {form_data});
  task_environment_.RunUntilIdle();

  // Check that the saving manual fallback is shown the user typed in a password
  // field.
  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, ShowManualFallbackForSaving(_, false, false))
      .WillOnce(MoveArg<0>(&form_manager_to_save));
  std::u16string typed_password = u"password";
  manager()->UpdateStateOnUserInput(&driver_, form_data.unique_renderer_id,
                                    password_element, typed_password);
  Mock::VerifyAndClearExpectations(&client_);

  ASSERT_TRUE(form_manager_to_save);
  EXPECT_EQ(typed_password,
            form_manager_to_save->GetPendingCredentials().password_value);

  // Check that the saving manual is hidden when the user cleared the password
  // field value.
  EXPECT_CALL(client_, HideManualFallbackForSaving());
  manager()->UpdateStateOnUserInput(&driver_, form_data.unique_renderer_id,
                                    password_element, std::u16string());
}

// Tests that there is no manual fallback for saving when there is a mismatch
// between the driver and the form manager. More precisely: the driver is tied
// to the correspondent iframe, but the form manager is tied to a form in the
// main frame.
TEST_F(PasswordManagerTest,
       NoManualFallbackForSavingForDriverAndFormManagerMismatchOnIOS) {
  ON_CALL(client_, IsSavingAndFillingEnabled).WillByDefault(Return(true));

  FormData form_data = MakeSimpleFormData();
  FieldRendererId password_element = form_data.fields[1].unique_renderer_id;

  // A form is found by PasswordManager.
  manager()->OnPasswordFormsParsed(&driver_, {form_data});
  task_environment_.RunUntilIdle();

  EXPECT_CALL(client_, ShowManualFallbackForSaving(_, false, false)).Times(0);
  EXPECT_CALL(client_, HideManualFallbackForSaving).Times(0);

  std::u16string typed_password = u"password";
  MockPasswordManagerDriver fake_driver;
  ASSERT_NE(&fake_driver, &driver_);
  manager()->UpdateStateOnUserInput(&fake_driver, form_data.unique_renderer_id,
                                    password_element, typed_password);
}
#endif  // BUILDFLAG(IS_IOS)

TEST_F(PasswordManagerTest, FormSubmitNoGoodMatch) {
  // When the password store already contains credentials for a given form, new
  // credentials get still added, as long as they differ in username from the
  // stored ones.
  PasswordForm existing_different(MakeSimpleForm());
  existing_different.username_value = u"google2";
  store_->AddLogin(existing_different);

  PasswordForm form(MakeSimpleForm());
  std::vector<FormData> observed = {form.form_data};
  EXPECT_CALL(driver_, SetPasswordFillData);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(manager()->form_managers().front());
  const VotesUploader& votes_uploader =
      manager()->form_managers().front()->votes_uploader();

  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.url))
      .WillRepeatedly(Return(true));
  OnPasswordFormSubmitted(form.form_data);

  // We still expect an add, since we didn't have a good match.
  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword)
      .WillOnce(MoveArgAndReturn<0>(&form_manager_to_save, true));

  // Now the password manager waits for the navigation to complete.
  observed.clear();
  manager()->DidNavigateMainFrame(true);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();

  // Check that suggested value was properly recorded in VotesUploader.
  EXPECT_EQ(form.username_value, votes_uploader.suggested_username());

  // Simulate saving the form.
  ASSERT_TRUE(form_manager_to_save);
  form_manager_to_save->Save();
  task_environment_.RunUntilIdle();

  EXPECT_THAT(
      store_->stored_passwords(),
      ElementsAre(Pair(form.signon_realm,
                       UnorderedElementsAre(FormMatches(existing_different),
                                            FormMatches(form)))));
}

// Tests that a credential wouldn't be saved if it is already in the store.
TEST_F(PasswordManagerTest, DontSaveAlreadySavedCredential) {
  PasswordForm form(MakeSimpleForm());
  store_->AddLogin(form);
  std::vector<FormData> observed = {form.form_data};
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.url))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(driver_, SetPasswordFillData);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();

  // The user is typing a credential manually. Till the credential is different
  // from the saved one, the fallback should be available.
  PasswordForm incomplete_match(form);
  incomplete_match.password_value =
      form.password_value.substr(0, form.password_value.length() - 1);
  incomplete_match.form_data.fields[1].value = incomplete_match.password_value;
  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, ShowManualFallbackForSaving(_, false, true))
      .WillOnce(MoveArg<0>((&form_manager_to_save)));
  manager()->OnInformAboutUserInput(&driver_, incomplete_match.form_data);
  ASSERT_TRUE(form_manager_to_save);
  EXPECT_THAT(form_manager_to_save->GetPendingCredentials(),
              FormMatches(incomplete_match));

  base::UserActionTester user_action_tester;

  // The user completes typing the credential. No fallback should be available,
  // because the credential is already in the store.
  EXPECT_CALL(client_, ShowManualFallbackForSaving(_, false, true)).Times(0);
  EXPECT_CALL(client_, HideManualFallbackForSaving);
  manager()->OnInformAboutUserInput(&driver_, form.form_data);

  // The user submits the form. No prompt should pop up. The credential is
  // updated in background.
  OnPasswordFormSubmitted(form.form_data);
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword).Times(0);
  observed.clear();
  manager()->DidNavigateMainFrame(true);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1,
            user_action_tester.GetActionCount("PasswordManager_LoginPassed"));
}

TEST_F(PasswordManagerTest, DoNotSaveWhenUserDeletesPassword) {
  PasswordForm form(MakeSimpleForm());
  PasswordForm stored_form = form;
  stored_form.password_value = u"old_password";
  store_->AddLogin(stored_form);

  ON_CALL(client_, IsSavingAndFillingEnabled(form.url))
      .WillByDefault(Return(true));

  std::vector<FormData> observed = {form.form_data};
  manager()->OnPasswordFormsParsed(&driver_, observed);
  task_environment_.RunUntilIdle();

  // The user is typing a credential manually, the fallback should be available.
  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, ShowManualFallbackForSaving(_, false, true))
      .WillOnce(MoveArg<0>(&form_manager_to_save));
  manager()->OnInformAboutUserInput(&driver_, form.form_data);
  ASSERT_TRUE(form_manager_to_save);

  // The user deletes the password, no manuall fallback should be shown.
  PasswordForm empty_password_form(form);
  empty_password_form.password_value.clear();
  empty_password_form.form_data.fields[1].value.clear();
  EXPECT_CALL(client_, ShowManualFallbackForSaving).Times(0);
  EXPECT_CALL(client_, HideManualFallbackForSaving);
  manager()->OnInformAboutUserInput(&driver_, empty_password_form.form_data);

  // The user submits the form. No prompt should pop up.
  OnPasswordFormSubmitted(empty_password_form.form_data);
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword).Times(0);
  observed.clear();
  manager()->DidNavigateMainFrame(true);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  task_environment_.RunUntilIdle();
}

// Tests that on Chrome sign-in form credentials are not saved.
TEST_F(PasswordManagerTest, DoNotSaveOnChromeSignInForm) {
  FormData form_data(MakeSimpleFormData());
  form_data.is_gaia_with_skip_save_password_form = true;
  std::vector<FormData> observed = {form_data};
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form_data.url))
      .WillRepeatedly(Return(true));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);

  EXPECT_CALL(*client_.GetStoreResultFilter(), ShouldSave(_))
      .WillRepeatedly(Return(false));
  // The user is typing a credential. No fallback should be available.
  FormData typed_credentials(form_data);
  typed_credentials.fields[1].value = u"pw";
  EXPECT_CALL(client_, ShowManualFallbackForSaving).Times(0);
  manager()->OnInformAboutUserInput(&driver_, form_data);

  // The user submits the form. No prompt should pop up.
  OnPasswordFormSubmitted(form_data);
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword).Times(0);
  observed.clear();
  manager()->DidNavigateMainFrame(true);
  manager()->OnPasswordFormsRendered(&driver_, observed);
}

// Tests that a UKM metric "Login Passed" is sent when the submitted credentials
// are already in the store and OnPasswordFormsParsed is called multiple times.
TEST_F(PasswordManagerTest,
       SubmissionMetricsIsPassedWhenDontSaveAlreadySavedCredential) {
  std::vector<FormData> observed;
  PasswordForm form(MakeSimpleForm());
  store_->AddLogin(form);
  observed.push_back(form.form_data);
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.url))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(driver_, SetPasswordFillData).Times(AnyNumber());
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();

  // The user submits the form.
  OnPasswordFormSubmitted(form.form_data);

  // Another call of OnPasswordFormsParsed happens. In production it happens
  // because of some DOM updates.
  manager()->OnPasswordFormsParsed(&driver_, observed);
  task_environment_.RunUntilIdle();

  EXPECT_CALL(client_, HideManualFallbackForSaving());
  // The call to manual fallback with |form| equal to already saved should close
  // the fallback, but it should not prevent sending metrics.
  manager()->OnInformAboutUserInput(&driver_, form.form_data);
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword).Times(0);

  // Simulate successful login. Expect "Login Passed" metric.
  base::UserActionTester user_action_tester;
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  EXPECT_EQ(1,
            user_action_tester.GetActionCount("PasswordManager_LoginPassed"));
}

TEST_F(PasswordManagerTest, FormSeenThenLeftPage) {
  std::vector<FormData> observed;
  FormData form_data(MakeSimpleFormData());
  observed.push_back(form_data);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);

  // No message from the renderer that a password was submitted. No
  // expected calls.
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword).Times(0);
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
}

TEST_F(PasswordManagerTest, FormSubmit) {
  // Test that a plain form submit results in offering to save passwords.
  PasswordForm form(MakeSimpleForm());
  std::vector<FormData> observed = {form.form_data};
  EXPECT_FALSE(manager()->IsPasswordFieldDetectedOnPage());
  manager()->OnPasswordFormsParsed(&driver_, observed);
  EXPECT_TRUE(manager()->IsPasswordFieldDetectedOnPage());
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(manager()->form_managers().front());
  const VotesUploader& votes_uploader =
      manager()->form_managers().front()->votes_uploader();

  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.url))
      .WillRepeatedly(Return(true));
  OnPasswordFormSubmitted(form.form_data);

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword)
      .WillOnce(MoveArgAndReturn<0>(&form_manager_to_save, true));
  EXPECT_CALL(client_, NotifyOnSuccessfulLogin(form.username_value));

  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();

  // Check that suggested value was properly recorded in VotesUploader.
  EXPECT_EQ(form.username_value, votes_uploader.suggested_username());

  // Simulate saving the form, as if the info bar was accepted.
  ASSERT_TRUE(form_manager_to_save);
  form_manager_to_save->Save();
  task_environment_.RunUntilIdle();

  EXPECT_THAT(
      store_->stored_passwords(),
      ElementsAre(Pair(form.signon_realm, ElementsAre(FormMatches(form)))));
}

TEST_F(PasswordManagerTest, IsPasswordFieldDetectedOnPage) {
  FormData form_data(MakeSimpleFormData());
  EXPECT_FALSE(manager()->IsPasswordFieldDetectedOnPage());
  manager()->OnPasswordFormsParsed(&driver_, {form_data});
  EXPECT_TRUE(manager()->IsPasswordFieldDetectedOnPage());
  manager()->DropFormManagers();
  EXPECT_FALSE(manager()->IsPasswordFieldDetectedOnPage());
}

TEST_F(PasswordManagerTest, FormSubmitWhenPasswordsCannotBeSaved) {
  // Test that a plain form submit doesn't result in offering to save passwords.
  auto store = base::MakeRefCounted<PasswordStore>(
      std::make_unique<FailingPasswordStoreBackend>());
  store->Init(/*prefs=*/nullptr, /*affiliated_match_helper=*/nullptr);
  ON_CALL(client_, GetProfilePasswordStore())
      .WillByDefault(Return(store.get()));

  FormData form_data(MakeSimpleFormData());
  std::vector<FormData> observed = {form_data};
  EXPECT_FALSE(manager()->IsPasswordFieldDetectedOnPage());
  manager()->OnPasswordFormsParsed(&driver_, observed);
  EXPECT_TRUE(manager()->IsPasswordFieldDetectedOnPage());
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();

  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form_data.url))
      .WillRepeatedly(Return(true));
  OnPasswordFormSubmitted(form_data);

  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword).Times(0);

  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();
  // Objects owned by the manager may keep references to the store - therefore
  // destroy the manager prior to store destruction.
  manager_.reset();
  store->ShutdownOnUIThread();
}

// This test verifies a fix for http://crbug.com/236673
TEST_F(PasswordManagerTest, FormSubmitWithFormOnPreviousPage) {
  PasswordForm first_form(MakeSimpleForm());
  first_form.url = GURL("http://www.nytimes.com/");
  first_form.form_data.url = first_form.url;
  first_form.action = GURL("https://myaccount.nytimes.com/auth/login");
  first_form.form_data.action = first_form.action;
  first_form.signon_realm = "http://www.nytimes.com/";
  PasswordForm second_form(MakeSimpleForm());
  second_form.url = GURL("https://myaccount.nytimes.com/auth/login");
  second_form.form_data.url = second_form.url;
  second_form.action = GURL("https://myaccount.nytimes.com/auth/login");
  second_form.form_data.action = second_form.action;
  second_form.signon_realm = "https://myaccount.nytimes.com/";

  // Pretend that the form is hidden on the first page.
  std::vector<FormData> observed;
  observed.push_back(first_form.form_data);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  observed.clear();
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();

  // Now navigate to a second page.
  manager()->DidNavigateMainFrame(true);

  // This page contains a form with the same markup, but on a different
  // URL.
  observed = {second_form.form_data};
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();

  // Now submit this form
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(second_form.url))
      .WillRepeatedly(Return(true));
  OnPasswordFormSubmitted(second_form.form_data);

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword)
      .WillOnce(MoveArgAndReturn<0>(&form_manager_to_save, true));
  // Navigation after form submit, no forms appear.
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();

  // Simulate saving the form, as if the info bar was accepted and make sure
  // that the saved form matches the second form, not the first.
  ASSERT_TRUE(form_manager_to_save);
  form_manager_to_save->Save();
  task_environment_.RunUntilIdle();

  EXPECT_THAT(store_->stored_passwords(),
              ElementsAre(Pair(second_form.signon_realm,
                               ElementsAre(FormMatches(second_form)))));
}

TEST_F(PasswordManagerTest, FormSubmitInvisibleLogin) {
  // Tests fix of http://crbug.com/28911: if the login form reappears on the
  // subsequent page, but is invisible, it shouldn't count as a failed login.
  std::vector<FormData> observed;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form.form_data);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();

  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.url))
      .WillRepeatedly(Return(true));
  OnPasswordFormSubmitted(form.form_data);

  // Expect info bar to appear:
  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword)
      .WillOnce(MoveArgAndReturn<0>(&form_manager_to_save, true));

  // The form reappears, but is not visible in the layout:
  manager()->OnPasswordFormsParsed(&driver_, observed);
  observed.clear();
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();

  // Simulate saving the form.
  ASSERT_TRUE(form_manager_to_save);
  form_manager_to_save->Save();
  task_environment_.RunUntilIdle();

  EXPECT_THAT(
      store_->stored_passwords(),
      ElementsAre(Pair(form.signon_realm, ElementsAre(FormMatches(form)))));
}

TEST_F(PasswordManagerTest, InitiallyInvisibleForm) {
  const PasswordForm form(MakeSimpleForm());
  store_->AddLogin(form);
  // Make sure an invisible login form still gets autofilled.
  std::vector<FormData> observed = {form.form_data};
  EXPECT_CALL(driver_, SetPasswordFillData);

  manager()->OnPasswordFormsParsed(&driver_, observed);
  observed.clear();
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();
}

TEST_F(PasswordManagerTest, FillPasswordsOnDisabledManager) {
  // Test fix for http://crbug.com/158296: Passwords must be filled even if the
  // password manager is disabled.
  const PasswordForm form(MakeSimpleForm());
  store_->AddLogin(form);
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.url))
      .WillRepeatedly(Return(false));
  std::vector<FormData> observed = {form.form_data};
  EXPECT_CALL(driver_, SetPasswordFillData);

  manager()->OnPasswordFormsParsed(&driver_, observed);
  task_environment_.RunUntilIdle();
}

TEST_F(PasswordManagerTest, LoginFormReappearance) {
  // If the password form reappears after submit, PasswordManager should deduce
  // that the login failed and not offer saving.
  std::vector<FormData> observed;
  FormData form_data(MakeSimpleFormData());
  observed.push_back(form_data);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();

  EXPECT_CALL(client_, IsSavingAndFillingEnabled).WillRepeatedly(Return(true));
  OnPasswordFormSubmitted(form_data);

  observed.clear();
  // Simulate form reapperance with different path in url and different
  // renderer ids.
  FormData form_data_after_navigation = form_data;
  form_data_after_navigation.unique_renderer_id.value() += 1000;
  form_data_after_navigation.url =
      GURL("https://accounts.google.com/login/error?redirect_after_login");
  for (auto& field : form_data_after_navigation.fields)
    field.value = u"";
  observed.push_back(form_data_after_navigation);

  // A PasswordForm appears, and is visible in the layout:
  // No prompts.
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword).Times(0);
  EXPECT_CALL(client_, AutomaticPasswordSave).Times(0);
  // Stop waiting for a successful "After-TouchToFill" submission as the
  // submission has failed.
  EXPECT_CALL(client_, ResetSubmissionTrackingAfterTouchToFill());

  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);

  task_environment_.RunUntilIdle();
  EXPECT_THAT(store_->stored_passwords(), testing::IsEmpty());
}

TEST_F(PasswordManagerTest, ChangePasswordFormReappearance) {
  // If a change password form reappears, it can mean a successful submisision.
  std::vector<FormData> observed;
  FormData form_data(MakeFormDataWithOnlyNewPasswordField());
  observed.push_back(form_data);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();

  EXPECT_CALL(client_, IsSavingAndFillingEnabled).WillRepeatedly(Return(true));
  OnPasswordFormSubmitted(form_data);

  observed.clear();
  // Simulate form reapperance with different path in url and different
  // renderer ids.
  FormData form_data_after_navigation = form_data;
  form_data_after_navigation.unique_renderer_id.value() += 1000;
  form_data_after_navigation.url =
      GURL("https://accounts.google.com/login/error?redirect_after_login");
  for (auto& field : form_data_after_navigation.fields)
    field.value = u"";
  observed.push_back(form_data_after_navigation);

  // Reappeared change password form is a signal of a possibly successful
  // submission and the user should be prompted to save/update the password.
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword);
  EXPECT_CALL(client_, AutomaticPasswordSave).Times(0);

  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();
}

TEST_F(PasswordManagerTest, SyncCredentialsNotSaved) {
  // Simulate loading a simple form with no existing stored password.
  std::vector<FormData> observed;
  FormData form_data(MakeSimpleGAIAFormData());
  observed.push_back(form_data);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();

  // User should not be prompted and password should not be saved.
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword).Times(0);

  ON_CALL(*client_.GetStoreResultFilter(), ShouldSaveGaiaPasswordHash(_))
      .WillByDefault(Return(true));
  ON_CALL(*client_.GetStoreResultFilter(), IsSyncAccountEmail(_))
      .WillByDefault(Return(true));
  EXPECT_CALL(reuse_manager_,
              SaveGaiaPasswordHash(
                  "googleuser", form_data.fields[1].value,
                  /*is_primary_account=*/true,
                  metrics_util::GaiaPasswordHashChange::SAVED_IN_CONTENT_AREA));

  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form_data.url))
      .WillRepeatedly(Return(true));

  client_.FilterAllResultsForSaving();

  OnPasswordFormSubmitted(form_data);
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);

  task_environment_.RunUntilIdle();
  EXPECT_THAT(store_->stored_passwords(), testing::IsEmpty());
}

TEST_F(PasswordManagerTest, HashSavedOnGaiaFormWithSkipSavePassword) {
  EXPECT_CALL(driver_, SetPasswordFillData).Times(0);
  std::vector<FormData> observed;
  FormData form_data(MakeSimpleGAIAFormData());
  // Simulate that this is Gaia form that should be ignored for
  // saving/filling.
  form_data.is_gaia_with_skip_save_password_form = true;
  observed.push_back(form_data);

  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form_data.url))
      .WillRepeatedly(Return(true));

  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();

  ON_CALL(*client_.GetStoreResultFilter(), ShouldSaveGaiaPasswordHash(_))
      .WillByDefault(Return(true));
  ON_CALL(*client_.GetStoreResultFilter(), ShouldSave(_))
      .WillByDefault(Return(false));
  ON_CALL(*client_.GetStoreResultFilter(), IsSyncAccountEmail(_))
      .WillByDefault(Return(true));

  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword).Times(0);

  EXPECT_CALL(reuse_manager_,
              SaveGaiaPasswordHash(
                  "googleuser", form_data.fields[1].value,
                  /*is_primary_account=*/true,
                  metrics_util::GaiaPasswordHashChange::SAVED_IN_CONTENT_AREA));

  OnPasswordFormSubmitted(form_data);
  observed.clear();
  manager()->OnPasswordFormsRendered(&driver_, observed);
}

TEST_F(PasswordManagerTest,
       HashSavedOnGaiaFormWithSkipSavePasswordAndToNTPNavigation) {
  EXPECT_CALL(driver_, SetPasswordFillData).Times(0);
  FormData form_data(MakeSimpleGAIAFormData());
  // Simulate that this is Gaia form that should be ignored for
  // saving/filling.
  form_data.is_gaia_with_skip_save_password_form = true;
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form_data.url))
      .WillRepeatedly(Return(true));
  manager()->OnPasswordFormsParsed(&driver_, {form_data});

  ON_CALL(*client_.GetStoreResultFilter(), ShouldSaveGaiaPasswordHash(_))
      .WillByDefault(Return(true));
  ON_CALL(*client_.GetStoreResultFilter(), ShouldSave(_))
      .WillByDefault(Return(false));
  ON_CALL(*client_.GetStoreResultFilter(), IsSyncAccountEmail(_))
      .WillByDefault(Return(true));

  EXPECT_CALL(reuse_manager_,
              SaveGaiaPasswordHash(
                  "googleuser", form_data.fields[1].value,
                  /*is_primary_account=*/true,
                  metrics_util::GaiaPasswordHashChange::SAVED_IN_CONTENT_AREA));

  EXPECT_CALL(client_, IsNewTabPage()).WillRepeatedly(Return(true));
  OnPasswordFormSubmitted(form_data);
  manager()->DidNavigateMainFrame(false);
}

// On a successful login with an updated password,
// CredentialsFilter::ReportFormLoginSuccess and CredentialsFilter::ShouldSave
// should be called. The argument of ShouldSave should be the submitted form.
TEST_F(PasswordManagerTest, ReportFormLoginSuccessAndShouldSaveCalled) {
  PasswordForm stored_form(MakeSimpleForm());
  store_->AddLogin(stored_form);

  std::vector<FormData> observed;
  PasswordForm observed_form = stored_form;
  // Different values of |username_element| needed to ensure that it is the
  // |observed_form| and not the |stored_form| what is passed to ShouldSave.
  observed_form.username_element += u"1";
  observed_form.form_data.fields[0].name = observed_form.username_element;
  observed.push_back(observed_form.form_data);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  EXPECT_CALL(driver_, SetPasswordFillData);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();

  // Submit form and finish navigation.
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(observed_form.url))
      .WillRepeatedly(Return(true));

  OnPasswordFormSubmitted(observed_form.form_data);

  // Chrome should recognise the successful login and call
  // ReportFormLoginSuccess.
  EXPECT_CALL(*client_.GetStoreResultFilter(), ReportFormLoginSuccess(_));

  PasswordForm submitted_form = observed_form;
  submitted_form.date_last_used = base::Time::Now();
  stored_form.date_last_used = submitted_form.date_last_used;
  EXPECT_CALL(*client_.GetStoreResultFilter(),
              ShouldSave(FormMatches(submitted_form)));
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);

  task_environment_.RunUntilIdle();
  EXPECT_THAT(store_->stored_passwords(),
              ElementsAre(Pair(stored_form.signon_realm,
                               ElementsAre(FormMatches(stored_form)))));
}

// When there is a sync password saved, and the user successfully uses the
// stored version of it, PasswordManager should not drop that password.
TEST_F(PasswordManagerTest, SyncCredentialsNotDroppedIfUpToDate) {
  const PasswordForm form(MakeSimpleGAIAForm());
  store_->AddLogin(form);

  client_.FilterAllResultsForSaving();

  std::vector<FormData> observed;
  observed.push_back(form.form_data);
  EXPECT_CALL(driver_, SetPasswordFillData);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();

  // Submit form and finish navigation.
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.url))
      .WillRepeatedly(Return(true));

  ON_CALL(*client_.GetStoreResultFilter(), ShouldSaveGaiaPasswordHash(_))
      .WillByDefault(Return(true));
  ON_CALL(*client_.GetStoreResultFilter(), IsSyncAccountEmail(_))
      .WillByDefault(Return(true));
  EXPECT_CALL(
      reuse_manager_,
      SaveGaiaPasswordHash(
          "googleuser", form.password_value, /*is_primary_account=*/true,
          metrics_util::GaiaPasswordHashChange::SAVED_IN_CONTENT_AREA));

  manager()->OnPasswordFormSubmitted(&driver_, form.form_data);

  // Chrome should not remove the sync credential, because it was successfully
  // used as stored, and therefore is up to date.
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);

  task_environment_.RunUntilIdle();
  EXPECT_THAT(
      store_->stored_passwords(),
      ElementsAre(Pair(form.signon_realm, ElementsAre(FormMatches(form)))));
}

// While sync credentials are not saved, they are still filled to avoid users
// thinking they lost access to their accounts.
TEST_F(PasswordManagerTest, SyncCredentialsStillFilled) {
  const PasswordForm form(MakeSimpleForm());
  store_->AddLogin(form);

  client_.FilterAllResultsForSaving();

  // Load the page.
  autofill::PasswordFormFillData form_data;
  EXPECT_CALL(driver_, SetPasswordFillData)
      .WillRepeatedly(SaveArg<0>(&form_data));
  std::vector<FormData> observed;
  observed.push_back(form.form_data);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(form.password_value, form_data.preferred_login.password_value);
}

TEST_F(PasswordManagerTest,
       ShouldBlockPasswordForSameOriginButDifferentSchemeTest) {
  constexpr struct {
    const char* old_origin;
    const char* new_origin;
    bool result;
  } kTestData[] = {
      // Same origin and same scheme.
      {"https://example.com/login", "https://example.com/login", false},
      // Same host and same scheme, different port.
      {"https://example.com:443/login", "https://example.com:444/login", false},
      // Same host but different scheme (https to http).
      {"https://example.com/login", "http://example.com/login", true},
      // Same host but different scheme (http to https).
      {"http://example.com/login", "https://example.com/login", false},
      // Different TLD, same schemes.
      {"https://example.com/login", "https://example.org/login", false},
      // Different TLD, different schemes.
      {"https://example.com/login", "http://example.org/login", false},
      // Different subdomains, same schemes.
      {"https://sub1.example.com/login", "https://sub2.example.org/login",
       false},
  };

  for (const auto& test_case : kTestData) {
    SCOPED_TRACE(testing::Message("#test_case = ") << (&test_case - kTestData));
    manager()->submitted_form_url_ = GURL(test_case.old_origin);
    GURL origin = GURL(test_case.new_origin);
    EXPECT_EQ(
        test_case.result,
        manager()->ShouldBlockPasswordForSameOriginButDifferentScheme(origin));
  }
}

// Tests whether two submissions to the same origin but different schemes
// result in only saving the first submission, which has a secure scheme.
TEST_F(PasswordManagerTest, AttemptedSavePasswordSameOriginInsecureScheme) {
  PasswordForm secure_form(MakeSimpleForm());
  secure_form.url = GURL("https://example.com/login");
  secure_form.action = GURL("https://example.com/login");
  secure_form.form_data.url = secure_form.url;
  secure_form.form_data.action = secure_form.action;
  secure_form.signon_realm = "https://example.com/";
  secure_form.match_type = PasswordForm::MatchType::kExact;

  PasswordForm insecure_form(MakeSimpleForm());
  // If all inputs of |secure_form| and |insecure_form| are the same, then
  // |insecure_form| is considered as reappearing of |secure_form| and the
  // submission is considered to be failed.
  insecure_form.username_element += u"1";
  FormFieldData& username_field = insecure_form.form_data.fields[0];
  username_field.name = insecure_form.username_element;
  insecure_form.username_value = u"compromised_user";
  username_field.value = insecure_form.username_value;
  insecure_form.password_value = u"C0mpr0m1s3d_P4ss";
  FormFieldData& password_field = insecure_form.form_data.fields[1];
  password_field.value = insecure_form.password_value;
  insecure_form.url = GURL("http://example.com/home");
  insecure_form.action = GURL("http://example.com/home");
  insecure_form.form_data.url = insecure_form.url;
  insecure_form.form_data.action = insecure_form.action;
  insecure_form.signon_realm = "http://example.com/";
  insecure_form.match_type = PasswordForm::MatchType::kExact;

  EXPECT_CALL(client_, IsSavingAndFillingEnabled(secure_form.url))
      .WillRepeatedly(Return(true));

  // Parse, render and submit the secure form.
  std::vector<FormData> observed = {secure_form.form_data};
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();
  OnPasswordFormSubmitted(secure_form.form_data);

  // Make sure `PromptUserToSaveOrUpdatePassword` gets called, and the resulting
  // form manager is saved.
  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword)
      .WillOnce(MoveArgAndReturn<0>(&form_manager_to_save, true));

  // Parse, render and submit the insecure form.
  observed = {insecure_form.form_data};
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(insecure_form.url))
      .WillRepeatedly(Return(true));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();
  OnPasswordFormSubmitted(insecure_form.form_data);

  // Expect no further calls to `PromptUserToSaveOrUpdatePassword` due to
  // insecure origin.
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword).Times(0);

  // Trigger call to |ProvisionalSavePassword| by rendering a page without
  // forms.
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();

  // Make sure that the form saved by the user is indeed the secure form.
  ASSERT_TRUE(form_manager_to_save);
  EXPECT_THAT(form_manager_to_save->GetPendingCredentials(),
              FormMatches(secure_form));
}

// Create a form with both a new and current password element. Let the current
// password value be non-empty and the new password value be empty and submit
// the form. While normally saving the new password is preferred (on change
// password forms, that would be the reasonable choice), if the new password is
// empty, this is likely just a slightly misunderstood form, and Chrome should
// save the non-empty current password field.
TEST_F(PasswordManagerTest, DoNotSaveWithEmptyNewPasswordAndNonemptyPassword) {
  std::vector<FormData> observed;
  FormData form_data(MakeSimpleFormData());
  ASSERT_FALSE(form_data.fields[1].value.empty());
  FormFieldData field;
  field.name = u"new_password_element";
  field.id_attribute = field.name;
  field.name_attribute = field.name;
  field.form_control_type = autofill::FormControlType::kInputPassword;
  field.unique_renderer_id = FieldRendererId(4);
  form_data.fields.push_back(field);

  observed.push_back(form_data);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();

  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form_data.url))
      .WillRepeatedly(Return(true));
  OnPasswordFormSubmitted(form_data);

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword)
      .WillOnce(MoveArgAndReturn<0>(&form_manager_to_save, true));

  // Now the password manager waits for the login to complete successfully.
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(form_manager_to_save);
  EXPECT_EQ(form_data.fields[1].value,
            form_manager_to_save->GetPendingCredentials().password_value);
}

TEST_F(PasswordManagerTest, FormSubmitWithOnlyPasswordField) {
  // Test to verify that on submitting the HTML password form without having
  // username input filed shows password save promt and saves the password to
  // store.
  EXPECT_CALL(driver_, SetPasswordFillData).Times(0);
  std::vector<FormData> observed;

  // Loads passsword form without username input field.
  PasswordForm form(MakeSimpleFormWithOnlyPasswordField());
  observed.push_back(form.form_data);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();

  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.url))
      .WillRepeatedly(Return(true));
  OnPasswordFormSubmitted(form.form_data);

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword)
      .WillOnce(MoveArgAndReturn<0>(&form_manager_to_save, true));

  // Now the password manager waits for the navigation to complete.
  observed.clear();
  manager()->DidNavigateMainFrame(true);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();

  // Simulate saving the form, as if the info bar was accepted.
  ASSERT_TRUE(form_manager_to_save);
  form_manager_to_save->Save();
  task_environment_.RunUntilIdle();

  EXPECT_THAT(
      store_->stored_passwords(),
      ElementsAre(Pair(form.signon_realm, ElementsAre(FormMatches(form)))));
}

// Test that if there are two "similar" forms in different frames, both get
// filled.

// "similar" is governed by PasswordFormManager::DoesManage, which in turn
// delegates to the unique renderer ID of the forms being the same. Note,
// however, that such ID is only unique within one renderer process. If
// different frames on the page are rendered by different processes, two
// unrelated forms can end up with the same ID. The test checks that
// nevertheless each of them gets assigned its own PasswordFormManager and
// filled as expected.
TEST_F(PasswordManagerTest, FillPasswordOnManyFrames_SameId) {
  PasswordForm first_form = MakeSavedForm();
  PasswordForm second_form = first_form;
  second_form.url = GURL("http://www.example.com/");
  second_form.signon_realm = "http://www.example.com/";
  store_->AddLogin(first_form);
  store_->AddLogin(second_form);

  // Two unrelated forms...
  FormData form_data1;
  form_data1.url = first_form.url;
  form_data1.action = first_form.action;
  form_data1.fields.resize(2);
  form_data1.fields[0].name = u"Email";
  form_data1.fields[0].unique_renderer_id = FieldRendererId(1);
  form_data1.fields[0].form_control_type =
      autofill::FormControlType::kInputText;
  form_data1.fields[1].name = u"Passwd";
  form_data1.fields[1].unique_renderer_id = FieldRendererId(2);
  form_data1.fields[1].form_control_type =
      autofill::FormControlType::kInputPassword;

  FormData form_data2 = form_data1;
  form_data2.url = second_form.url;
  form_data2.action = second_form.action;
  form_data2.fields[0].name = u"User";
  form_data2.fields[0].unique_renderer_id = FieldRendererId(3);
  form_data2.fields[1].name = u"Pwd";
  form_data2.fields[1].unique_renderer_id = FieldRendererId(4);

  // Make the forms be "similar".
  form_data1.unique_renderer_id = FormRendererId(7654);
  form_data2.unique_renderer_id = FormRendererId(7654);

  // Observe the form in the first frame.
  EXPECT_CALL(driver_, SetPasswordFillData);
  manager()->OnPasswordFormsParsed(&driver_, {form_data1});
  task_environment_.RunUntilIdle();

  // Observe the form in the second frame.
  MockPasswordManagerDriver driver_b;
  EXPECT_CALL(driver_b, SetPasswordFillData);
  manager()->OnPasswordFormsParsed(&driver_b, {form_data2});
  task_environment_.RunUntilIdle();
}

TEST_F(PasswordManagerTest, SameDocumentNavigation) {
  // Test that observing a newly submitted form shows the save password bar on
  // call in page navigation.
  std::vector<FormData> observed;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form.form_data);
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.url))
      .WillRepeatedly(Return(true));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();
  manager()->OnInformAboutUserInput(&driver_, form.form_data);

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword)
      .WillOnce(MoveArgAndReturn<0>(&form_manager_to_save, true));
  manager()->OnDynamicFormSubmission(&driver_, form.submission_event);
  ASSERT_TRUE(form_manager_to_save);

  // Simulate saving the form, as if the info bar was accepted.
  form_manager_to_save->Save();
  task_environment_.RunUntilIdle();

  EXPECT_THAT(
      store_->stored_passwords(),
      ElementsAre(Pair(form.signon_realm, ElementsAre(FormMatches(form)))));
}

// This test ensures that the user does not get prompted for dynamic form
// submissions on the GAIA signon realm. GAIA will always perform a full
// redirect once the user clears the login challenge, so that other submission
// detections will trigger eventually.
TEST_F(PasswordManagerTest, DynamicFormSubmissionIgnoredForGaia) {
  // Set-up a GAIA form and simulate the user typing into the form.
  PasswordForm gaia_form = MakeSimpleGAIAForm();
  std::vector<FormData> observed = {gaia_form.form_data};
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(gaia_form.url))
      .WillRepeatedly(Return(true));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  manager()->OnInformAboutUserInput(&driver_, gaia_form.form_data);

  // Trigger a dynamic form submission, ensure that the user does not see a
  // prompt.
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword).Times(0);
  manager()->OnDynamicFormSubmission(&driver_, gaia_form.submission_event);
}

TEST_F(PasswordManagerTest, SameDocumentBlockedSite) {
  // Test that observing a newly submitted form on blocked site does notify
  // the embedder on call in page navigation.
  std::vector<FormData> observed;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form.form_data);
  // Simulate that blocked form stored in store.
  PasswordForm blocked_form(form);
  blocked_form.username_value = u"";
  blocked_form.password_value = u"";
  blocked_form.blocked_by_user = true;
  store_->AddLogin(blocked_form);

  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.url))
      .WillRepeatedly(Return(true));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();
  manager()->OnInformAboutUserInput(&driver_, form.form_data);

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword)
      .WillOnce(MoveArgAndReturn<0>(&form_manager_to_save, true));

  manager()->OnDynamicFormSubmission(&driver_, form.submission_event);
  EXPECT_TRUE(form_manager_to_save->IsBlocklisted());
}

TEST_F(PasswordManagerTest, FormSubmittedUnchangedNotifiesClient) {
  // This tests verifies that if the observed forms and provisionally saved
  // forms are the same, then successful submission notifies the client.
  std::vector<FormData> observed;
  PasswordForm form(MakeSimpleForm());
  store_->AddLogin(form);
  observed.push_back(form.form_data);
  EXPECT_CALL(driver_, SetPasswordFillData);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();

  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.url))
      .WillRepeatedly(Return(true));
  OnPasswordFormSubmitted(form.form_data);

  PasswordForm notified_form;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword).Times(0);
  EXPECT_CALL(client_, NotifySuccessfulLoginWithExistingPassword(_))
      .WillOnce([&notified_form](const auto& submitted_manager) {
        notified_form = submitted_manager->GetPendingCredentials();
      });

  // Now the password manager waits for the navigation to complete.
  observed.clear();
  manager()->DidNavigateMainFrame(true);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();

  EXPECT_THAT(form, FormMatches(notified_form));
  EXPECT_THAT(
      store_->stored_passwords(),
      ElementsAre(Pair(form.signon_realm, ElementsAre(FormMatches(form)))));
}

TEST_F(PasswordManagerTest, SaveFormFetchedAfterSubmit) {
  // Test that a password is offered for saving even if the response from the
  // PasswordStore comes after submit.
  std::vector<FormData> observed;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form.form_data);

  // GetLogins calls remain unanswered to emulate that PasswordStore did not
  // fetch a form in time before submission.
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);

  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.url))
      .WillRepeatedly(Return(true));

  OnPasswordFormSubmitted(form.form_data);

  // Emulate fetching password form from PasswordStore after submission but
  // before post-navigation load.
  task_environment_.RunUntilIdle();

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword)
      .WillOnce(MoveArgAndReturn<0>(&form_manager_to_save, true));

  // Now the password manager waits for the navigation to complete.
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);

  // Simulate saving the form, as if the info bar was accepted.
  ASSERT_TRUE(form_manager_to_save);
  form_manager_to_save->Save();
  task_environment_.RunUntilIdle();

  EXPECT_THAT(
      store_->stored_passwords(),
      ElementsAre(Pair(form.signon_realm, ElementsAre(FormMatches(form)))));
}

TEST_F(PasswordManagerTest, PasswordGeneration_FailedSubmission) {
  std::vector<FormData> observed;
  FormData form_data(MakeSignUpFormData());
  observed.push_back(form_data);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();

  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form_data.url))
      .WillRepeatedly(Return(true));
  manager()->OnPresaveGeneratedPassword(&driver_, form_data,
                                        form_data.fields[1].value);
  task_environment_.RunUntilIdle();
  task_environment_.RunUntilIdle();
  ASSERT_FALSE(store_->stored_passwords().empty());
  PasswordForm form = store_->stored_passwords().begin()->second[0];

  // Do not save generated password when the password form reappears.
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword).Times(0);
  EXPECT_CALL(client_, AutomaticPasswordSave).Times(0);

  // Simulate submission failing, with the same form being visible after
  // navigation.
  OnPasswordFormSubmitted(form_data);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();

  EXPECT_THAT(
      store_->stored_passwords(),
      ElementsAre(Pair(form.signon_realm, ElementsAre(FormMatches(form)))));
}

// If the user edits the generated password, but does not remove it completely,
// it should stay treated as a generated password.
TEST_F(PasswordManagerTest, PasswordGenerationPasswordEdited_FailedSubmission) {
  std::vector<FormData> observed;
  FormData form_data(MakeSignUpFormData());
  observed.push_back(form_data);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);

  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form_data.url))
      .WillRepeatedly(Return(true));
  manager()->OnPresaveGeneratedPassword(&driver_, form_data,
                                        form_data.fields[1].value);
  task_environment_.RunUntilIdle();
  ASSERT_FALSE(store_->stored_passwords().empty());
  PasswordForm form = store_->stored_passwords().begin()->second[0];

  // Simulate user editing and submitting a different password. Verify that
  // the edited password is the one that is saved.
  form_data.fields[1].value = u"different_password";
  OnPasswordFormSubmitted(form_data);

  // Do not save generated password when the password form reappears.
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword).Times(0);
  EXPECT_CALL(client_, AutomaticPasswordSave).Times(0);

  // Simulate submission failing, with the same form being visible after
  // navigation.
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();

  form.password_value = u"p4ssword";
  EXPECT_THAT(
      store_->stored_passwords(),
      ElementsAre(Pair(form.signon_realm, ElementsAre(FormMatches(form)))));
}

// Generated password are saved even if it looks like the submit failed (the
// form reappeared). Verify that passwords which are no longer marked as
// generated will not be automatically saved.
TEST_F(PasswordManagerTest,
       PasswordGenerationNoLongerGeneratedPasswordNotForceSaved_FailedSubmit) {
  std::vector<FormData> observed;
  FormData form_data(MakeSignUpFormData());
  observed.push_back(form_data);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();

  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form_data.url))
      .WillRepeatedly(Return(true));
  manager()->OnPresaveGeneratedPassword(&driver_, form_data,
                                        form_data.fields[1].value);
  task_environment_.RunUntilIdle();
  ASSERT_FALSE(store_->stored_passwords().empty());

  // Simulate user removing generated password and adding a new one.
  form_data.fields[1].value = u"different_password";
  manager()->OnPasswordNoLongerGenerated(&driver_, form_data);
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(store_->IsEmpty());

  OnPasswordFormSubmitted(form_data);

  // No infobar or prompt is shown if submission fails.
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword).Times(0);
  EXPECT_CALL(client_, AutomaticPasswordSave).Times(0);

  // Simulate submission failing, with the same form being visible after
  // navigation.
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();
}

// Verify that passwords which are no longer generated trigger the confirmation
// dialog when submitted.
TEST_F(PasswordManagerTest,
       PasswordGenerationNoLongerGeneratedPasswordNotForceSaved) {
  std::vector<FormData> observed;
  FormData form_data(MakeSignUpFormData());
  observed.push_back(form_data);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();

  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form_data.url))
      .WillRepeatedly(Return(true));
  manager()->OnPresaveGeneratedPassword(&driver_, form_data,
                                        form_data.fields[1].value);
  task_environment_.RunUntilIdle();
  ASSERT_FALSE(store_->stored_passwords().empty());

  // Simulate user removing generated password and adding a new one.
  form_data.fields[1].value = u"different_password";
  manager()->OnPasswordNoLongerGenerated(&driver_, form_data);
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(store_->IsEmpty());

  OnPasswordFormSubmitted(form_data);

  // Verify that a normal prompt is shown instead of the force saving UI.
  std::unique_ptr<PasswordFormManagerForUI> form_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword)
      .WillOnce(MoveArgAndReturn<0>(&form_to_save, false));
  EXPECT_CALL(client_, AutomaticPasswordSave).Times(0);

  // Simulate a successful submission.
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();
}

TEST_F(PasswordManagerTest, PasswordGenerationUsernameChanged) {
  std::vector<FormData> observed;
  FormData form_data(MakeSignUpFormData());
  observed.push_back(form_data);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();

  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form_data.url))
      .WillRepeatedly(Return(true));
  manager()->OnPresaveGeneratedPassword(&driver_, form_data,
                                        form_data.fields[1].value);

  // Simulate user changing the username, without ever completely
  // deleting the password.
  form_data.fields[0].value = u"new_username";
  OnPasswordFormSubmitted(form_data);
  task_environment_.RunUntilIdle();

  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword).Times(0);
  EXPECT_CALL(client_, AutomaticPasswordSave);

  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();

  ASSERT_THAT(store_->stored_passwords(), SizeIs(1));
  PasswordForm form = store_->stored_passwords().begin()->second[0];
  EXPECT_EQ(form_data.fields[0].value, form.username_value);
  EXPECT_EQ(form_data.fields[1].value, form.password_value);
}

TEST_F(PasswordManagerTest, PasswordGenerationPresavePassword) {
  std::vector<FormData> observed;
  PasswordForm form(MakeFormWithOnlyNewPasswordField());
  observed.push_back(form.form_data);
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.url))
      .WillRepeatedly(Return(true));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);

  base::HistogramTester histogram_tester;

  // The user accepts a generated password.
  form.password_value = u"password";
  manager()->OnPresaveGeneratedPassword(&driver_, form.form_data,
                                        form.password_value);
  task_environment_.RunUntilIdle();
  ASSERT_FALSE(store_->stored_passwords().empty());
  PasswordForm sanitized_form(form);
  SanitizeFormData(&sanitized_form.form_data);
  EXPECT_THAT(store_->stored_passwords(),
              ElementsAre(Pair(sanitized_form.signon_realm,
                               ElementsAre(FormMatches(sanitized_form)))));

  // The user updates the generated password.
  PasswordForm updated_form(form);
  updated_form.password_value = u"password_12345";
  manager()->OnPresaveGeneratedPassword(&driver_, updated_form.form_data,
                                        updated_form.password_value);
  task_environment_.RunUntilIdle();
  ASSERT_FALSE(store_->stored_passwords().empty());
  PasswordForm sanitized_updated_form(updated_form);
  SanitizeFormData(&sanitized_updated_form.form_data);
  EXPECT_THAT(
      store_->stored_passwords(),
      ElementsAre(Pair(sanitized_updated_form.signon_realm,
                       ElementsAre(FormMatches(sanitized_updated_form)))));
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.GeneratedFormHasNoFormManager", false, 2);

  // The user removes the generated password.
  manager()->OnPasswordNoLongerGenerated(&driver_, updated_form.form_data);
  task_environment_.RunUntilIdle();
  EXPECT_THAT(store_->stored_passwords(),
              ElementsAre(Pair(form.signon_realm, testing::IsEmpty())));
}

TEST_F(PasswordManagerTest, PasswordGenerationPresavePassword_NoFormManager) {
  // Checks that GeneratedFormHasNoFormManager metric is sent if there is no
  // corresponding PasswordFormManager for the given form. It should be uncommon
  // case.
  std::vector<FormData> observed;
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();

  base::HistogramTester histogram_tester;

  // The user accepts a generated password.
  FormData form_data(MakeSignUpFormData());
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form_data.url))
      .WillRepeatedly(Return(true));
  manager()->OnPresaveGeneratedPassword(&driver_, form_data,
                                        form_data.fields[1].value);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(store_->stored_passwords().empty());
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.GeneratedFormHasNoFormManager", true, 1);
}

TEST_F(PasswordManagerTest, PasswordGenerationPresavePasswordAndLogin) {
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(_))
      .WillRepeatedly(Return(true));
  const bool kFalseTrue[] = {false, true};
  for (bool found_matched_logins_in_store : kFalseTrue) {
    SCOPED_TRACE(testing::Message("found_matched_logins_in_store = ")
                 << found_matched_logins_in_store);
    store_->Clear();
    PasswordForm form(MakeFormWithOnlyNewPasswordField());
    std::vector<FormData> observed = {form.form_data};
    if (found_matched_logins_in_store) {
      store_->AddLogin(form);
      EXPECT_CALL(driver_, SetPasswordFillData);
    }
    EXPECT_CALL(client_, AutomaticPasswordSave)
        .Times(found_matched_logins_in_store ? 0 : 1);
    manager()->OnPasswordFormsParsed(&driver_, observed);
    manager()->OnPasswordFormsRendered(&driver_, observed);
    task_environment_.RunUntilIdle();

    // The user accepts generated password and makes successful login.
    form.password_value = form.new_password_value;
    PasswordForm presaved_form(form);
    if (found_matched_logins_in_store)
      presaved_form.username_value.clear();
    manager()->OnPresaveGeneratedPassword(&driver_, form.form_data,
                                          form.password_value);
    task_environment_.RunUntilIdle();
    EXPECT_THAT(
        store_->stored_passwords(),
        ElementsAre(Pair(form.signon_realm,
                         testing::Contains(FormMatches(presaved_form)))));

    OnPasswordFormSubmitted(form.form_data);
    observed.clear();
    std::unique_ptr<PasswordFormManagerForUI> form_manager;
    if (found_matched_logins_in_store) {
      EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword)
          .WillOnce(MoveArgAndReturn<0>(&form_manager, false));
    } else {
      EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword).Times(0);
    }
    manager()->DidNavigateMainFrame(true);
    manager()->OnPasswordFormsParsed(&driver_, observed);
    manager()->OnPasswordFormsRendered(&driver_, observed);
    task_environment_.RunUntilIdle();

    if (found_matched_logins_in_store) {
      // Credentials should be updated only when the user explicitly chooses.
      ASSERT_TRUE(form_manager);
      form_manager->Update(form_manager->GetPendingCredentials());
    }
    task_environment_.RunUntilIdle();
    form.password_element =
        found_matched_logins_in_store ? u"" : form.new_password_element;
    EXPECT_THAT(
        store_->stored_passwords(),
        ElementsAre(Pair(form.signon_realm, ElementsAre(FormMatches(form)))));
  }
}

TEST_F(PasswordManagerTest, SetGenerationElementAndTypeForForm) {
  PasswordForm form(MakeSimpleForm());
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.url))
      .WillRepeatedly(Return(true));

  manager()->OnPasswordFormsParsed(&driver_, {form.form_data});

  manager()->SetGenerationElementAndTypeForForm(
      &driver_, form.form_data.unique_renderer_id,
      form.form_data.fields[1].unique_renderer_id,
      autofill::password_generation::PasswordGenerationType::kAutomatic);
  manager()->OnPresaveGeneratedPassword(&driver_, form.form_data,
                                        form.password_value);

  const PasswordFormManager* form_manager =
      manager()->form_managers().front().get();

  EXPECT_TRUE(form_manager->HasGeneratedPassword());
}

TEST_F(PasswordManagerTest, UpdateFormManagers) {
  // Seeing a form should result in creating PasswordFormManager and
  // PasswordFormManager and querying PasswordStore. Calling
  // UpdateFormManagers should result in querying the store again.
  PasswordForm form(MakeSimpleForm());
  manager()->OnPasswordFormsParsed(&driver_, {form.form_data});
  task_environment_.RunUntilIdle();

  store_->AddLogin(form);
  autofill::PasswordFormFillData form_data;
  EXPECT_CALL(driver_, SetPasswordFillData).WillOnce(SaveArg<0>(&form_data));
  manager()->UpdateFormManagers();
  task_environment_.RunUntilIdle();

  EXPECT_EQ(form.username_value, form_data.preferred_login.username_value);
  EXPECT_EQ(form.password_value, form_data.preferred_login.password_value);
}

TEST_F(PasswordManagerTest, AutofillingOfAffiliatedCredentials) {
  PasswordForm android_form(MakeAndroidCredential());
  PasswordForm observed_form(MakeSimpleForm());
  std::vector<FormData> observed_forms;
  observed_forms.push_back(observed_form.form_data);

  autofill::PasswordFormFillData form_data;
  EXPECT_CALL(driver_, SetPasswordFillData)
      .WillRepeatedly(SaveArg<0>(&form_data));
  store_->AddLogin(android_form);
  mock_match_helper_->ExpectCallToGetAffiliatedAndGrouped(
      PasswordFormDigest(observed_form), {android_form.signon_realm});
  manager()->OnPasswordFormsParsed(&driver_, observed_forms);
  manager()->OnPasswordFormsRendered(&driver_, observed_forms);
  task_environment_.RunUntilIdle();

  EXPECT_EQ(android_form.username_value,
            form_data.preferred_login.username_value);
  EXPECT_EQ(android_form.password_value,
            form_data.preferred_login.password_value);
  // On Android Touch To Fill will prevent autofilling credentials on page load.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  EXPECT_TRUE(form_data.wait_for_username);
#else
  EXPECT_FALSE(form_data.wait_for_username);
#endif
  EXPECT_EQ(android_form.signon_realm, form_data.preferred_login.realm);

  EXPECT_CALL(client_, IsSavingAndFillingEnabled(observed_form.url))
      .WillRepeatedly(Return(true));

  PasswordForm filled_form(observed_form);
  filled_form.username_value = android_form.username_value;
  filled_form.form_data.fields[0].value = filled_form.username_value;
  filled_form.password_value = android_form.password_value;
  filled_form.form_data.fields[1].value = filled_form.password_value;
  OnPasswordFormSubmitted(filled_form.form_data);

  PasswordForm saved_notified_form;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword).Times(0);
  EXPECT_CALL(client_, NotifySuccessfulLoginWithExistingPassword(_))
      .WillOnce([&saved_notified_form](const auto& submitted_manager) {
        saved_notified_form = submitted_manager->GetPendingCredentials();
      });

  observed_forms.clear();
  manager()->DidNavigateMainFrame(true);
  manager()->OnPasswordFormsParsed(&driver_, observed_forms);
  manager()->OnPasswordFormsRendered(&driver_, observed_forms);
  task_environment_.RunUntilIdle();

  EXPECT_THAT(store_->stored_passwords(),
              ElementsAre(Pair(android_form.signon_realm,
                               ElementsAre(FormMatches(android_form)))));
  EXPECT_THAT(android_form, FormMatches(saved_notified_form));
}

// If the manager fills a credential originally saved from an affiliated Android
// application, and the user overwrites the password, they should be prompted if
// they want to update.  If so, the Android credential itself should be updated.
TEST_F(PasswordManagerTest, UpdatePasswordOfAffiliatedCredential) {
  PasswordForm android_form(MakeAndroidCredential());
  PasswordForm observed_form(MakeSimpleForm());
  std::vector<FormData> observed_forms = {observed_form.form_data};

  EXPECT_CALL(driver_, SetPasswordFillData);
  store_->AddLogin(android_form);
  mock_match_helper_->ExpectCallToGetAffiliatedAndGrouped(
      PasswordFormDigest(observed_form), {android_form.signon_realm});
  manager()->OnPasswordFormsParsed(&driver_, observed_forms);
  manager()->OnPasswordFormsRendered(&driver_, observed_forms);
  task_environment_.RunUntilIdle();

  EXPECT_CALL(client_, IsSavingAndFillingEnabled(observed_form.url))
      .WillRepeatedly(Return(true));

  PasswordForm filled_form(observed_form);
  filled_form.username_value = android_form.username_value;
  filled_form.form_data.fields[0].value = filled_form.username_value;
  filled_form.password_value = u"new_password";
  filled_form.form_data.fields[1].value = filled_form.password_value;
  OnPasswordFormSubmitted(filled_form.form_data);

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword)
      .WillOnce(MoveArgAndReturn<0>(&form_manager_to_save, true));

  observed_forms.clear();
  manager()->DidNavigateMainFrame(true);
  manager()->OnPasswordFormsParsed(&driver_, observed_forms);
  manager()->OnPasswordFormsRendered(&driver_, observed_forms);
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(form_manager_to_save);
  form_manager_to_save->Save();
  task_environment_.RunUntilIdle();

  PasswordForm expected_form(android_form);
  expected_form.password_value = filled_form.password_value;
  EXPECT_THAT(store_->stored_passwords(),
              ElementsAre(Pair(expected_form.signon_realm,
                               ElementsAre(FormMatches(expected_form)))));
}

TEST_F(PasswordManagerTest, ClearedFieldsSuccessCriteria) {
  // Test that a submission is considered to be successful on a change password
  // form without username when fields valued are cleared.
  PasswordForm form(MakeFormWithOnlyNewPasswordField());
  form.username_element.clear();
  form.username_value.clear();
  form.form_data.fields[0].value.clear();
  std::vector<FormData> observed = {form.form_data};

  // Emulate page load.
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.url))
      .WillRepeatedly(Return(true));

  OnPasswordFormSubmitted(form.form_data);

  // JavaScript cleared field values.
  observed[0].fields[1].value.clear();

  // Check success of the submission.
  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword)
      .WillOnce(MoveArgAndReturn<0>(&form_manager_to_save, true));

  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();
}

// Check that no sync password hash is saved when no username is available,
// because we it's not clear whether the submitted credentials are sync
// credentials.
TEST_F(PasswordManagerTest, NotSavingSyncPasswordHash_NoUsername) {
  // Simulate loading a simple form with no existing stored password.
  std::vector<FormData> observed;
  FormData form_data(MakeSimpleGAIAFormData());
  // Simulate that no username is found.
  form_data.fields[0].value.clear();
  observed.push_back(form_data);
  manager()->OnPasswordFormsRendered(&driver_, observed);

  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form_data.url))
      .WillRepeatedly(Return(true));

  // Simulate that this credentials which is similar to be sync credentials.
  client_.FilterAllResultsForSaving();

  // Check that no Gaia credential password hash is saved.
  EXPECT_CALL(reuse_manager_, SaveGaiaPasswordHash).Times(0);
  OnPasswordFormSubmitted(form_data);
  observed.clear();
  manager()->OnPasswordFormsRendered(&driver_, observed);
}

// Check that no sync password hash is saved when the submitted credentials are
// not qualified as sync credentials.
TEST_F(PasswordManagerTest, NotSavingSyncPasswordHash_NotSyncCredentials) {
  // Simulate loading a simple form with no existing stored password.
  FormData form_data(MakeSimpleGAIAFormData());
  std::vector<FormData> observed = {form_data};
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();

  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form_data.url))
      .WillRepeatedly(Return(true));

  // Check that no Gaia credential password hash is saved since these
  // credentials are eligible for saving.
  EXPECT_CALL(reuse_manager_, SaveGaiaPasswordHash).Times(0);

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword)
      .WillOnce(MoveArgAndReturn<0>(&form_manager_to_save, true));

  OnPasswordFormSubmitted(form_data);
  observed.clear();
  manager()->OnPasswordFormsRendered(&driver_, observed);
}

TEST_F(PasswordManagerTest, ManualFallbackForSaving) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  std::vector<FormData> observed;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form.form_data);
  PasswordForm stored_form = form;
  stored_form.password_value = u"old_password";
  store_->AddLogin(stored_form);
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.url))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(driver_, SetPasswordFillData);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();

  // The username of the stored form is the same, there should be update bubble.
  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, ShowManualFallbackForSaving(_, false, true))
      .WillOnce(MoveArg<0>(&form_manager_to_save));
  manager()->OnInformAboutUserInput(&driver_, form.form_data);
  ASSERT_TRUE(form_manager_to_save);
  EXPECT_THAT(form_manager_to_save->GetPendingCredentials(), FormMatches(form));

  // The username of the stored form is different, there should be save bubble.
  PasswordForm new_form = form;
  new_form.username_value = u"another_username";
  new_form.form_data.fields[0].value = new_form.username_value;
  EXPECT_CALL(client_, ShowManualFallbackForSaving(_, false, false))
      .WillOnce(MoveArg<0>(&form_manager_to_save));
  manager()->OnInformAboutUserInput(&driver_, new_form.form_data);
  ASSERT_TRUE(form_manager_to_save);
  EXPECT_THAT(form_manager_to_save->GetPendingCredentials(),
              FormMatches(new_form));

  // Hide the manual fallback.
  EXPECT_CALL(client_, HideManualFallbackForSaving());
  manager()->HideManualFallbackForSaving();

  // Two PasswordFormManagers instances hold references to a shared
  // PasswordFormMetrics recorder. These need to be freed to flush the metrics
  // into the test_ukm_recorder.
  manager_.reset();
  form_manager_to_save.reset();

  // Verify that the last state is recorded.
  CheckMetricHasValue(
      test_ukm_recorder, ukm::builders::PasswordForm::kEntryName,
      ukm::builders::PasswordForm::kSaving_ShowedManualFallbackForSavingName,
      1);
}

// Tests that the manual fallback for saving isn't shown if there is no response
// from the password storage. When crbug.com/741537 is fixed, change this test.
TEST_F(PasswordManagerTest, ManualFallbackForSaving_SlowBackend) {
  std::vector<FormData> observed;
  FormData form_data(MakeSimpleFormData());
  observed.push_back(form_data);
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form_data.url))
      .WillRepeatedly(Return(true));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);

  // There is no response from the store. Don't show the fallback.
  EXPECT_CALL(client_, ShowManualFallbackForSaving).Times(0);
  manager()->OnInformAboutUserInput(&driver_, form_data);

  // The storage responded. The fallback can be shown.
  task_environment_.RunUntilIdle();
  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, ShowManualFallbackForSaving(_, false, false))
      .WillOnce(MoveArg<0>(&form_manager_to_save));
  manager()->OnInformAboutUserInput(&driver_, form_data);
}

// Test is not applicable to iOS, because there is no manual fallback for
// generated passwords.
#if !BUILDFLAG(IS_IOS)
TEST_F(PasswordManagerTest, ManualFallbackForSaving_GeneratedPassword) {
  std::vector<FormData> observed;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form.form_data);
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.url))
      .WillRepeatedly(Return(true));
  // TODO(https://crbug.com/949519): replace WillRepeatedly with WillOnce when
  // the old parser is gone.
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();

  // A user accepts a password generated by Chrome. It triggers password
  // presaving and showing manual fallback.
  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, ShowManualFallbackForSaving(_, true, false))
      .WillOnce(MoveArg<0>(&form_manager_to_save));
  manager()->OnPresaveGeneratedPassword(&driver_, form.form_data,
                                        form.password_value);
  manager()->OnInformAboutUserInput(&driver_, form.form_data);
  ASSERT_TRUE(form_manager_to_save);
  EXPECT_THAT(form_manager_to_save->GetPendingCredentials(), FormMatches(form));

  // A user edits the generated password. And again it causes password presaving
  // and showing manual fallback.
  EXPECT_CALL(client_, ShowManualFallbackForSaving(_, true, false))
      .WillOnce(MoveArg<0>(&form_manager_to_save));
  manager()->OnPresaveGeneratedPassword(&driver_, form.form_data,
                                        form.password_value);
  manager()->OnInformAboutUserInput(&driver_, form.form_data);

  // A user removes the generated password. The presaved password is removed,
  // the fallback is disabled.
  EXPECT_CALL(client_, HideManualFallbackForSaving());
  manager()->OnPasswordNoLongerGenerated(&driver_, form.form_data);
  manager()->HideManualFallbackForSaving();
  task_environment_.RunUntilIdle();

  EXPECT_THAT(store_->stored_passwords(),
              ElementsAre(Pair(form.signon_realm, testing::IsEmpty())));
}
#endif  // !BUILDFLAG(IS_IOS)

// Sync password hash should be updated upon submission of change password page.
TEST_F(PasswordManagerTest, SaveSyncPasswordHashOnChangePasswordPage) {
  FormData form_data(MakeGAIAChangePasswordFormData());

  std::vector<FormData> observed;
  observed.push_back(form_data);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();

  // Submit form and finish navigation.
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form_data.url))
      .WillRepeatedly(Return(true));

  ON_CALL(*client_.GetStoreResultFilter(), ShouldSaveGaiaPasswordHash(_))
      .WillByDefault(Return(true));
  ON_CALL(*client_.GetStoreResultFilter(), IsSyncAccountEmail(_))
      .WillByDefault(Return(true));
  EXPECT_CALL(
      reuse_manager_,
      SaveGaiaPasswordHash(
          "googleuser", form_data.fields[1].value,
          /*is_primary_account=*/true,
          metrics_util::GaiaPasswordHashChange::CHANGED_IN_CONTENT_AREA));

  client_.FilterAllResultsForSaving();
  OnPasswordFormSubmitted(form_data);

  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();
}

// Non-Sync Gaia password hash should be saved upon submission of Gaia login
// page.
TEST_F(PasswordManagerTest, SaveOtherGaiaPasswordHash) {
  FormData form_data(MakeSimpleGAIAFormData());

  std::vector<FormData> observed;
  observed.push_back(form_data);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();
  // Submit form and finish navigation.
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form_data.url))
      .WillRepeatedly(Return(true));

  ON_CALL(*client_.GetStoreResultFilter(), ShouldSaveGaiaPasswordHash(_))
      .WillByDefault(Return(true));
  EXPECT_CALL(
      reuse_manager_,
      SaveGaiaPasswordHash(
          "googleuser", form_data.fields[1].value, /*is_primary_account=*/false,
          metrics_util::GaiaPasswordHashChange::SAVED_IN_CONTENT_AREA));

  client_.FilterAllResultsForSaving();
  OnPasswordFormSubmitted(form_data);

  observed.clear();
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();
}

// Non-Sync Gaia password hash should be saved upon submission of change
// password page.
TEST_F(PasswordManagerTest, SaveOtherGaiaPasswordHashOnChangePasswordPage) {
  FormData form_data(MakeGAIAChangePasswordFormData());

  std::vector<FormData> observed;
  observed.push_back(form_data);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();
  // Submit form and finish navigation.
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form_data.url))
      .WillRepeatedly(Return(true));

  ON_CALL(*client_.GetStoreResultFilter(), ShouldSaveGaiaPasswordHash(_))
      .WillByDefault(Return(true));
  EXPECT_CALL(
      reuse_manager_,
      SaveGaiaPasswordHash(
          "googleuser", form_data.fields[1].value, /*is_primary_account=*/false,
          metrics_util::GaiaPasswordHashChange::NOT_SYNC_PASSWORD_CHANGE));

  client_.FilterAllResultsForSaving();
  OnPasswordFormSubmitted(form_data);

  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();
}

// Enterprise password hash should be saved upon submission of enterprise login
// page.
TEST_F(PasswordManagerTest, SaveEnterprisePasswordHash) {
  FormData form_data(MakeSimpleFormData());

  std::vector<FormData> observed;
  observed.push_back(form_data);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();

  // Submit form and finish navigation.
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form_data.url))
      .WillRepeatedly(Return(true));

  ON_CALL(*client_.GetStoreResultFilter(), ShouldSaveEnterprisePasswordHash(_))
      .WillByDefault(Return(true));
  ON_CALL(*client_.GetStoreResultFilter(), IsSyncAccountEmail(_))
      .WillByDefault(Return(false));
  EXPECT_CALL(reuse_manager_, SaveEnterprisePasswordHash(
                                  "googleuser", form_data.fields[1].value));
  client_.FilterAllResultsForSaving();
  OnPasswordFormSubmitted(form_data);

  observed.clear();
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();
}

TEST_F(PasswordManagerTest, CreatingFormManagers) {
  FormData form_data(MakeSimpleFormData());
  std::vector<FormData> observed;
  observed.push_back(form_data);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  // Check that the form manager is created.
  EXPECT_EQ(1u, manager()->form_managers().size());
  EXPECT_TRUE(manager()->form_managers()[0]->DoesManage(
      form_data.unique_renderer_id, &driver_));

  // Check that receiving the same form the second time does not lead to
  // creating new form manager.
  manager()->OnPasswordFormsParsed(&driver_, observed);
  EXPECT_EQ(1u, manager()->form_managers().size());
}

// Tests that processing normal HTML form submissions works properly with the
// new parsing. For details see scheme 1 in comments before
// |form_managers_| in password_manager.h.
TEST_F(PasswordManagerTest, ProcessingNormalFormSubmission) {
  for (bool successful_submission : {false, true}) {
    SCOPED_TRACE(testing::Message("successful_submission = ")
                 << successful_submission);

    EXPECT_CALL(client_, IsSavingAndFillingEnabled(_))
        .WillRepeatedly(Return(true));

    FormData form_data(MakeSimpleFormData());

    std::vector<FormData> observed;
    observed.push_back(form_data);
    manager()->OnPasswordFormsParsed(&driver_, observed);
    manager()->OnPasswordFormsRendered(&driver_, observed);
    task_environment_.RunUntilIdle();

    auto submitted_form_data = form_data;
    submitted_form_data.fields[0].value = u"username";
    submitted_form_data.fields[1].value = u"password1";

    OnPasswordFormSubmitted(submitted_form_data);
    EXPECT_TRUE(manager()->GetSubmittedManagerForTest());

    std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
    // Simulate submission.
    if (successful_submission) {
      EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword)
          .WillOnce(MoveArgAndReturn<0>(&form_manager_to_save, true));
      // The form disappeared, so the submission is condered to be successful.
      observed.clear();
    } else {
      EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword).Times(0);
    }
    manager()->OnPasswordFormsRendered(&driver_, observed);

    // Multiple calls of OnPasswordFormsRendered should be handled gracefully.
    manager()->OnPasswordFormsRendered(&driver_, observed);
    task_environment_.RunUntilIdle();
    testing::Mock::VerifyAndClearExpectations(&client_);
  }
}

// Tests that processing form submissions without navigations works properly
// with the new parsing. For details see scheme 2 in comments before
// |form_managers_| in password_manager.h.
TEST_F(PasswordManagerTest, ProcessingOtherSubmissionTypes) {
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(_))
      .WillRepeatedly(Return(true));

  FormData form_data(MakeSimpleFormData());

  std::vector<FormData> observed;
  observed.push_back(form_data);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();
  manager()->OnInformAboutUserInput(&driver_, form_data);

  auto submitted_form_data = form_data;
  submitted_form_data.fields[0].value = u"username";
  submitted_form_data.fields[1].value = u"strong_password";

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword)
      .WillOnce(MoveArgAndReturn<0>(&form_manager_to_save, true));
  manager()->OnDynamicFormSubmission(&driver_,
                                     submitted_form_data.submission_event);
  EXPECT_TRUE(manager()->form_managers().empty());
}

TEST_F(PasswordManagerTest, SubmittedGaiaFormWithoutVisiblePasswordField) {
  // Tests that a submitted GAIA sign-in form which does not contain a visible
  // password field is skipped.
  std::vector<FormData> observed;
  FormData form_data(MakeSimpleGAIAFormData());
  observed.push_back(form_data);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);

  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form_data.url))
      .WillRepeatedly(Return(true));

  form_data.fields[0].value = u"username";
  form_data.fields[1].value = u"password";
  form_data.fields[1].is_focusable = false;

  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword).Times(0);
  manager()->OnDynamicFormSubmission(&driver_, form_data.submission_event);
}

TEST_F(PasswordManagerTest, MetricForSchemeOfSuccessfulLogins) {
  for (bool origin_is_secure : {false, true}) {
    SCOPED_TRACE(testing::Message("origin_is_secure = ") << origin_is_secure);
    FormData form_data(MakeSimpleFormData());
    form_data.url =
        GURL(origin_is_secure ? "https://example.com" : "http://example.com");
    std::vector<FormData> observed = {form_data};
    EXPECT_CALL(client_, IsSavingAndFillingEnabled(form_data.url))
        .WillRepeatedly(Return(true));
    manager()->OnPasswordFormsParsed(&driver_, observed);
    manager()->OnPasswordFormsRendered(&driver_, observed);
    task_environment_.RunUntilIdle();

    OnPasswordFormSubmitted(form_data);

    std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
    EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword)
        .WillOnce(MoveArgAndReturn<0>(&form_manager_to_save, true));

    observed.clear();
    base::HistogramTester histogram_tester;
    manager()->OnPasswordFormsParsed(&driver_, observed);
    manager()->OnPasswordFormsRendered(&driver_, observed);
    histogram_tester.ExpectUniqueSample(
        "PasswordManager.SuccessfulLoginHappened", origin_is_secure, 1);
  }
}

TEST_F(PasswordManagerTest, ManualFallbackForSavingNewParser) {
  PasswordFormManager::set_wait_for_server_predictions_for_filling(false);

  std::vector<FormData> observed;
  PasswordForm form(MakeSimpleForm());
  observed.push_back(form.form_data);
  PasswordForm stored_form = form;
  stored_form.password_value = u"old_password";
  store_->AddLogin(stored_form);
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.url))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(driver_, SetPasswordFillData);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();

  // The username of the stored form is the same, there should be update bubble.
  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, ShowManualFallbackForSaving(_, false, true))
      .WillOnce(MoveArg<0>(&form_manager_to_save));
  manager()->OnInformAboutUserInput(&driver_, form.form_data);
  ASSERT_TRUE(form_manager_to_save);
  EXPECT_THAT(form_manager_to_save->GetPendingCredentials(), FormMatches(form));

  // The username of the stored form is different, there should be save bubble.
  PasswordForm new_form = form;
  new_form.username_value = u"another_username";
  new_form.form_data.fields[0].value = new_form.username_value;
  EXPECT_CALL(client_, ShowManualFallbackForSaving(_, false, false))
      .WillOnce(MoveArg<0>(&form_manager_to_save));
  manager()->OnInformAboutUserInput(&driver_, new_form.form_data);
  ASSERT_TRUE(form_manager_to_save);
  EXPECT_THAT(form_manager_to_save->GetPendingCredentials(),
              FormMatches(new_form));

  // Hide the manual fallback.
  EXPECT_CALL(client_, HideManualFallbackForSaving());
  manager()->HideManualFallbackForSaving();
}

TEST_F(PasswordManagerTest, NoSavePromptWhenPasswordManagerDisabled) {
  FormData form_data(MakeSimpleFormData());
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form_data.url))
      .WillRepeatedly(Return(false));
  manager()->OnPasswordFormsParsed(&driver_, {form_data});

  auto submitted_form_data = form_data;
  submitted_form_data.fields[0].value = u"username";
  submitted_form_data.fields[1].value = u"strong_password";

  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword).Times(0);
  manager()->OnDynamicFormSubmission(&driver_,
                                     submitted_form_data.submission_event);
}

TEST_F(PasswordManagerTest, NoSavePromptForNotPasswordForm) {
  FormData form_data(MakeSimpleFormData());
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form_data.url))
      .WillRepeatedly(Return(true));
  // Make the form to be credit card form.
  form_data.fields[1].autocomplete_attribute = "cc-csc";

  manager()->OnPasswordFormsParsed(&driver_, {form_data});

  auto submitted_form_data = form_data;
  submitted_form_data.fields[0].value = u"text";
  submitted_form_data.fields[1].value = u"1234";

  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword).Times(0);
  manager()->OnDynamicFormSubmission(&driver_,
                                     submitted_form_data.submission_event);
}

// Check that when autofill predictions are received before a form is found then
// server predictions are not ignored and used for filling.
TEST_F(PasswordManagerTest, AutofillPredictionBeforeFormParsed) {
  PasswordFormManager::set_wait_for_server_predictions_for_filling(true);
  EXPECT_CALL(client_, IsSavingAndFillingEnabled).WillRepeatedly(Return(true));

  PasswordForm form(MakeSimpleForm());
  store_->AddLogin(form);

  // Server predictions says that this is a sign-in form. Since they have higher
  // priority than autocomplete attributes then the form should be filled.
  manager()->ProcessAutofillPredictions(
      &driver_, form.form_data,
      CreateServerPredictions(form.form_data,
                              {{1, ServerFieldType::PASSWORD}}));

  EXPECT_CALL(driver_, SetPasswordFillData);

  // Simulate that the form is incorrectly marked as sign-up, which means it can
  // not be filled without server predictions.
  form.form_data.fields[1].autocomplete_attribute = "new-password";

  manager()->OnPasswordFormsParsed(&driver_, {form.form_data});
  task_environment_.RunUntilIdle();
}

// Username first flows are not yet available on iOS (crbug.com/1064560).
#if !BUILDFLAG(IS_IOS)
// Check that when autofill predictions for a `SINGLE_USERNAME` field are
// received, a fill password request is sent to the renderer even
// if `OnPasswordFormsParsed` is never called.
TEST_F(PasswordManagerTest,
       AutofillPredictionBeforeFormParsedWithoutAutocompleteAttribute) {
  PasswordFormManager::set_wait_for_server_predictions_for_filling(true);

  PasswordForm form(MakeSimpleFormWithOnlyUsernameField());
  store_->AddLogin(form);

  manager()->ProcessAutofillPredictions(
      &driver_, form.form_data,
      CreateServerPredictions(form.form_data,
                              {{0, ServerFieldType::SINGLE_USERNAME}}));

  EXPECT_CALL(driver_, SetPasswordFillData);

  // We do not call `OnPasswordFormsParsed()` (to simulate the renderer) because
  // the renderer does not recognize it as a relevant form, since the field does
  // not have an autocomplete="username" attribute.
  task_environment_.RunUntilIdle();
}

// Check that when autofill predictions for a `SINGLE_USERNAME` field with an
// autocomplete attribute set to "username" are received, no fill password
// request is sent to the renderer until `OnPasswordFormsParsed()` is called,
// since we expect the renderer to see the form and notify `PasswordManager`
// about it.
TEST_F(PasswordManagerTest,
       AutofillPredictionBeforeFormParsedWithAutocompleteAttribute) {
  PasswordFormManager::set_wait_for_server_predictions_for_filling(true);

  PasswordForm form(MakeSimpleFormWithOnlyUsernameField());
  store_->AddLogin(form);

  // The renderer would detect the autocomplete attribute below.
  form.form_data.fields[0].autocomplete_attribute = "username";

  // No fill call is sent to the renderer during prediction processing.
  EXPECT_CALL(driver_, SetPasswordFillData).Times(0);
  manager()->ProcessAutofillPredictions(
      &driver_, form.form_data,
      CreateServerPredictions(form.form_data,
                              {{0, ServerFieldType::SINGLE_USERNAME}}));
  Mock::VerifyAndClearExpectations(&driver_);

  // But once the renderer notifies `PasswordManager` that the forms have been
  // parsed, the fill call is made.
  EXPECT_CALL(driver_, SetPasswordFillData);
  manager()->OnPasswordFormsParsed(&driver_, {form.form_data});

  task_environment_.RunUntilIdle();
}

// Check that an update from a login form to a form with a `SINGLE_USERNAME`
// prediction results in a new fill attempt even if the renderer does not call
// `OnFormsParsed()` after the dynamic update because the username field does
// not have an autocomplete attribute.
TEST_F(PasswordManagerTest,
       DynamicFormUpdateToSingleUsernameFormTriggersFillingFromPredictions) {
  PasswordFormManager::set_wait_for_server_predictions_for_filling(true);

  PasswordForm form(MakeSimpleForm());
  store_->AddLogin(form);

  // The first time we see the form, it has a password field and is therefore
  // recognized by the renderer (which calls `OnPasswordFormsParsed`).
  EXPECT_CALL(driver_, SetPasswordFillData);
  manager()->ProcessAutofillPredictions(
      &driver_, form.form_data,
      CreateServerPredictions(
          form.form_data,
          {{0, ServerFieldType::USERNAME}, {1, ServerFieldType::PASSWORD}}));
  manager()->OnPasswordFormsParsed(&driver_, {form.form_data});
  task_environment_.RunUntilIdle();
  Mock::VerifyAndClearExpectations(&driver_);

  // Simulate removing the password field. The remaining username field could
  // now be a `SINGLE_USERNAME` field in a "Forgot password?" flow.
  FormData modified_form_data = form.form_data;
  modified_form_data.fields.pop_back();

  EXPECT_CALL(driver_, SetPasswordFillData);
  manager()->ProcessAutofillPredictions(
      &driver_, modified_form_data,
      CreateServerPredictions(modified_form_data,
                              {{0, ServerFieldType::SINGLE_USERNAME}}));

  task_environment_.RunUntilIdle();
}
#endif  // !BUIDFLAG(IS_IOS)

// Check that when autofill predictions are received before a form is found then
// server predictions are not ignored and used for filling in case there are
// multiple forms on a page, including forms that have UsernameFirstFlow votes.
TEST_F(PasswordManagerTest, AutofillPredictionBeforeMultipleFormsParsed) {
#if BUILDFLAG(IS_IOS)
  feature_list_.InitAndEnableFeature(features::kIOSPasswordSignInUff);
#endif  // BUIDFLAG(IS_IOS)

  PasswordFormManager::set_wait_for_server_predictions_for_filling(true);
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(_))
      .WillRepeatedly(Return(true));

  PasswordForm form1(MakeSimpleFormWithOnlyUsernameField());
  PasswordForm form2(MakeSimpleForm());
  store_->AddLogin(form2);

  // Server predictions say that this is a sign-in form. Since they have higher
  // priority than autocomplete attributes, the form should be filled.
  manager()->ProcessAutofillPredictions(
      &driver_, form1.form_data,
      CreateServerPredictions(form1.form_data,
                              {{0, ServerFieldType::SINGLE_USERNAME}}));
  manager()->ProcessAutofillPredictions(
      &driver_, form2.form_data,
      CreateServerPredictions(form2.form_data,
                              {{1, ServerFieldType::PASSWORD}}));

  EXPECT_CALL(driver_, SetPasswordFillData).Times(2);

  // Simulate that the form is incorrectly marked as sign-up, which means it can
  // not be filled without server predictions.
  form2.form_data.fields[1].autocomplete_attribute = "new-password";
  manager()->OnPasswordFormsParsed(&driver_,
                                   {form1.form_data, form2.form_data});
  task_environment_.RunUntilIdle();
}

// Checks the following scenario:
// 1. The user is typing in a password form.
// 2. Navigation happens.
// 3. The password disappeared after navigation.
// 4. A save prompt is shown.
TEST_F(PasswordManagerTest, SavingAfterUserTypingAndNavigation) {
  for (bool form_may_be_submitted : {false, true}) {
    SCOPED_TRACE(testing::Message()
                 << "form_may_be_submitted = " << form_may_be_submitted);
    PasswordForm form(MakeSimpleForm());
    EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.url))
        .WillRepeatedly(Return(true));
    manager()->OnPasswordFormsParsed(&driver_, {form.form_data});
    task_environment_.RunUntilIdle();

    // The user is typing as a result the saving manual fallback is shown.
    std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
    EXPECT_CALL(client_, ShowManualFallbackForSaving(_, false, false))
        .WillOnce(MoveArg<0>(&form_manager_to_save));
    manager()->OnInformAboutUserInput(&driver_, form.form_data);
    ASSERT_TRUE(form_manager_to_save);
    EXPECT_THAT(form_manager_to_save->GetPendingCredentials(),
                FormMatches(form));

    // Check that a save prompt is shown when there is no password form after
    // the navigation (which suggests that the submission was successful).
    if (form_may_be_submitted) {
      EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword)
          .WillOnce(MoveArgAndReturn<0>(&form_manager_to_save, true));
    } else {
      EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword).Times(0);
    }

    manager()->DidNavigateMainFrame(form_may_be_submitted);
    manager()->OnPasswordFormsRendered(&driver_, {});
    task_environment_.RunUntilIdle();

    EXPECT_THAT(form_manager_to_save->GetPendingCredentials(),
                FormMatches(form));
    testing::Mock::VerifyAndClearExpectations(&client_);
  }
}

// Check that when a form is submitted and a PasswordFormManager not present,
// this ends up reported in ProvisionallySaveFailure UMA and UKM.
TEST_F(PasswordManagerTest, ProvisionallySaveFailure) {
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(_))
      .WillRepeatedly(Return(true));
  manager()->OnPasswordFormsParsed(nullptr, {});

  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  auto metrics_recorder =
      std::make_unique<PasswordManagerMetricsRecorder>(1234);
  EXPECT_CALL(client_, GetMetricsRecorder())
      .WillRepeatedly(Return(metrics_recorder.get()));

  FormData unobserved_form_data = MakeSimpleFormData();
  manager()->OnPasswordFormSubmitted(nullptr, unobserved_form_data);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.ProvisionalSaveFailure2",
      PasswordManagerMetricsRecorder::NO_MATCHING_FORM, 1);
  // Flush the UKM reports.
  EXPECT_CALL(client_, GetMetricsRecorder()).WillRepeatedly(Return(nullptr));
  metrics_recorder.reset();
  CheckMetricHasValue(
      test_ukm_recorder, ukm::builders::PageWithPassword::kEntryName,
      ukm::builders::PageWithPassword::kProvisionalSaveFailureName,
      PasswordManagerMetricsRecorder::NO_MATCHING_FORM);
}

namespace {

// A convenience helper for type conversions.
template <typename T>
absl::optional<int64_t> MetricValue(T value) {
  return absl::optional<int64_t>(static_cast<int64_t>(value));
}

struct MissingFormManagerTestCase {
  // Description for logging.
  const char* const description = nullptr;
  // Is Chrome allowed to save passwords?
  enum class Saving { Enabled, Disabled } saving = Saving::Enabled;
  // What signal does Chrome have for saving?
  enum class Signal { Automatic, Manual, None } save_signal = Signal::Automatic;
  // All the forms which are parsed at once.
  std::vector<FormData> parsed_forms_data;
  // A list of forms to be processed for saving, one at a time.
  std::vector<FormData> processed_form_data;
  // The expected value of the PageWithPassword::kFormManagerAvailableName
  // metric, or absl::nullopt if no value should be logged.
  absl::optional<int64_t> expected_metric_value;
};

}  // namespace

// Test that presence of form managers in various situations is appropriately
// reported through UKM.
TEST_F(PasswordManagerTest, ReportMissingFormManager) {
  const FormData form_data = MakeSimpleFormData();
  FormData other_form_data = MakeSimpleFormData();
  other_form_data.unique_renderer_id.value() += 1;

  const MissingFormManagerTestCase kTestCases[] = {
      {
          .description =
              "A form is submitted and a PasswordFormManager not present.",
          .save_signal = MissingFormManagerTestCase::Signal::Automatic,
          .parsed_forms_data = {},
          // .parsed_forms is empty, so the processed form below was not
          // observed and has no form manager associated.
          .processed_form_data = {form_data},
          .expected_metric_value =
              MetricValue(PasswordManagerMetricsRecorder::FormManagerAvailable::
                              kMissingProvisionallySave),
      },
      {
          .description = "Manual saving is requested and a "
                         "PasswordFormManager is created.",
          .save_signal = MissingFormManagerTestCase::Signal::Manual,
          .parsed_forms_data = {},
          // .parsed_forms is empty, so the processed form below was not
          // observed and has no form manager associated.
          .processed_form_data = {form_data},
          .expected_metric_value = MetricValue(
              PasswordManagerMetricsRecorder::FormManagerAvailable::kSuccess),
      },
      {
          .description = "Manual saving is successfully requested.",
          .save_signal = MissingFormManagerTestCase::Signal::Manual,
          .parsed_forms_data = {form_data},
          .processed_form_data = {form_data},
          .expected_metric_value = MetricValue(
              PasswordManagerMetricsRecorder::FormManagerAvailable::kSuccess),
      },
      {
          .description =
              "A form is submitted and a PasswordFormManager present.",
          .save_signal = MissingFormManagerTestCase::Signal::Automatic,
          .parsed_forms_data = {form_data},
          .processed_form_data = {form_data},
          .expected_metric_value = MetricValue(
              PasswordManagerMetricsRecorder::FormManagerAvailable::kSuccess),
      },
      {
          .description = "First failure, then success.",
          .save_signal = MissingFormManagerTestCase::Signal::Automatic,
          .parsed_forms_data = {form_data},
          // Processing |other_form| first signals a failure value in the
          // metric, but processing |form| after that should overwrite that with
          // kSuccess.
          .processed_form_data = {other_form_data, form_data},
          .expected_metric_value = MetricValue(
              PasswordManagerMetricsRecorder::FormManagerAvailable::kSuccess),
      },
      {
          .description = "No forms, no report.",
          .save_signal = MissingFormManagerTestCase::Signal::None,
          .parsed_forms_data = {},
          .processed_form_data = {},
          .expected_metric_value = absl::nullopt,
      },
      {
          .description = "Not enabled, no report.",
          .saving = MissingFormManagerTestCase::Saving::Disabled,
          .save_signal = MissingFormManagerTestCase::Signal::Automatic,
          .parsed_forms_data = {form_data},
          .processed_form_data = {form_data},
          .expected_metric_value = absl::nullopt,
      },
  };

  for (const MissingFormManagerTestCase& test_case : kTestCases) {
    EXPECT_CALL(client_, IsSavingAndFillingEnabled(_))
        .WillRepeatedly(Return(test_case.saving ==
                               MissingFormManagerTestCase::Saving::Enabled));

    SCOPED_TRACE(testing::Message() << "test case = " << test_case.description);

    manager()->OnPasswordFormsParsed(nullptr, test_case.parsed_forms_data);
    task_environment_.RunUntilIdle();

    ukm::TestAutoSetUkmRecorder test_ukm_recorder;
    auto metrics_recorder =
        std::make_unique<PasswordManagerMetricsRecorder>(1234);
    EXPECT_CALL(client_, GetMetricsRecorder())
        .WillRepeatedly(Return(metrics_recorder.get()));

    for (const FormData& processed_form_data : test_case.processed_form_data) {
      switch (test_case.save_signal) {
        case MissingFormManagerTestCase::Signal::Automatic:
          manager()->OnPasswordFormSubmitted(nullptr, processed_form_data);
          break;
        case MissingFormManagerTestCase::Signal::Manual:
          manager()->OnInformAboutUserInput(nullptr, processed_form_data);
          break;
        case MissingFormManagerTestCase::Signal::None:
          break;
      }
    }

    // Flush the UKM reports.
    EXPECT_CALL(client_, GetMetricsRecorder()).WillRepeatedly(Return(nullptr));
    metrics_recorder.reset();
    if (test_case.expected_metric_value) {
      CheckMetricHasValue(
          test_ukm_recorder, ukm::builders::PageWithPassword::kEntryName,
          ukm::builders::PageWithPassword::kFormManagerAvailableName,
          test_case.expected_metric_value.value());
    } else {
      EXPECT_FALSE(ukm::TestUkmRecorder::EntryHasMetric(
          GetMetricEntry(test_ukm_recorder,
                         ukm::builders::PageWithPassword::kEntryName),
          ukm::builders::PageWithPassword::kFormManagerAvailableName));
    }
  }
}

// Tests that despite there a form was not seen on a page load, new
// |PasswordFormManager| is created in process of saving.
TEST_F(PasswordManagerTest, CreatePasswordFormManagerOnSaving) {
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(_))
      .WillRepeatedly(Return(true));

  PasswordForm form(MakeSimpleForm());
  manager()->OnPasswordFormsParsed(&driver_, {form.form_data});
  task_environment_.RunUntilIdle();

  // Simulate that JavaScript creates a new form, fills username/password and
  // submits it.
  auto submitted_form = form;
  submitted_form.form_data.unique_renderer_id.value() += 1000;
  submitted_form.username_value = u"username1";
  submitted_form.form_data.fields[0].value = submitted_form.username_value;
  submitted_form.password_value = u"password1";
  submitted_form.form_data.fields[1].value = submitted_form.password_value;

  OnPasswordFormSubmitted(submitted_form.form_data);
  EXPECT_TRUE(manager()->GetSubmittedManagerForTest());
  task_environment_.RunUntilIdle();

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword)
      .WillOnce(MoveArgAndReturn<0>(&form_manager_to_save, true));

  // The form disappeared, so the submission is considered to be successful.
  manager()->OnPasswordFormsRendered(&driver_, {});
  ASSERT_TRUE(form_manager_to_save);
  EXPECT_THAT(form_manager_to_save->GetPendingCredentials(),
              FormMatches(submitted_form));
}

// Tests that no save prompt from form manager is shown when Credentials
// Management API function store is called.
TEST_F(PasswordManagerTest, NoSavePromptAfterStoreCalled) {
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(_))
      .WillRepeatedly(Return(true));

  FormData form_data(MakeSimpleFormData());
  manager()->OnPasswordFormsParsed(&driver_, {form_data});

  // Simulate that navigator.credentials.store function is called.
  manager()->NotifyStorePasswordCalled();

  OnPasswordFormSubmitted(form_data);
  EXPECT_FALSE(manager()->GetSubmittedManagerForTest());
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword).Times(0);

  manager()->OnPasswordFormsRendered(&driver_, {});
}

// Check that on non-password form, saving and filling fallbacks are available
// but no automatic filling and saving are available.
TEST_F(PasswordManagerTest, FillingAndSavingFallbacksOnNonPasswordForm) {
  PasswordFormManager::set_wait_for_server_predictions_for_filling(false);
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(_))
      .WillRepeatedly(Return(true));

  PasswordForm saved_match(MakeSimpleForm());
  store_->AddLogin(saved_match);
  PasswordForm non_password_form(MakeSimpleForm());
  non_password_form.username_value = u"+1 650 000 000";  // Phone number.
  non_password_form.form_data.fields[0].value =
      non_password_form.username_value;
  non_password_form.password_value = u"379 390";  // One time SMS code.
  non_password_form.form_data.fields[1].value =
      non_password_form.password_value;
  non_password_form.form_data.fields[1].id_attribute = u"one-time-code";
  non_password_form.only_for_fallback = true;

  PasswordFormFillData form_data;
  EXPECT_CALL(driver_, SetPasswordFillData)
      .WillRepeatedly(SaveArg<0>(&form_data));

  manager()->OnPasswordFormsParsed(&driver_, {non_password_form.form_data});
  task_environment_.RunUntilIdle();

  // Check that manual filling fallback available.
  EXPECT_EQ(saved_match.username_value,
            form_data.preferred_login.username_value);
  EXPECT_EQ(saved_match.password_value,
            form_data.preferred_login.password_value);
  // Check that no automatic filling available.
  EXPECT_TRUE(form_data.username_element_renderer_id.is_null());
  EXPECT_TRUE(form_data.password_element_renderer_id.is_null());

  // Check that saving fallback is available.
  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, ShowManualFallbackForSaving)
      .WillOnce(MoveArg<0>(&form_manager_to_save));
  manager()->OnInformAboutUserInput(&driver_, non_password_form.form_data);
  ASSERT_TRUE(form_manager_to_save);
  EXPECT_THAT(form_manager_to_save->GetPendingCredentials(),
              FormMatches(non_password_form));

  // Check that no automatic save prompt is shown.
  OnPasswordFormSubmitted(non_password_form.form_data);
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword).Times(0);
  manager()->DidNavigateMainFrame(true);
  manager()->OnPasswordFormsRendered(&driver_, {});
  task_environment_.RunUntilIdle();
}

// Check that on a credit card form, there is no password filling (can be
// overwritten with a server override, but it is not tested in this test). For
// saving, only the fallback is available.
TEST_F(PasswordManagerTest, FillingAndSavingFallbacksOnCreditCardForm) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      password_manager::features::kDisablePasswordsDropdownForCvcFields);
  PasswordFormManager::set_wait_for_server_predictions_for_filling(false);
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(_))
      .WillRepeatedly(Return(true));

  PasswordForm saved_match(MakeSimpleForm());
  store_->AddLogin(saved_match);
  PasswordForm credit_card_form(MakeSimpleCreditCardForm());
  credit_card_form.only_for_fallback = true;

  PasswordFormFillData form_data;
  // No filling fallback in order to let non-password Autofill to handle the
  // form.
  EXPECT_CALL(driver_, SetPasswordFillData).Times(0);

  manager()->OnPasswordFormsParsed(&driver_, {credit_card_form.form_data});
  task_environment_.RunUntilIdle();

  // Check that saving fallback is available.
  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, ShowManualFallbackForSaving(_, false, false))
      .WillOnce(MoveArg<0>(&form_manager_to_save));
  manager()->OnInformAboutUserInput(&driver_, credit_card_form.form_data);
  ASSERT_TRUE(form_manager_to_save);
  EXPECT_THAT(form_manager_to_save->GetPendingCredentials(),
              FormMatches(credit_card_form));

  // Check that no automatic save prompt is shown.
  OnPasswordFormSubmitted(credit_card_form.form_data);
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword).Times(0);
  manager()->DidNavigateMainFrame(true);
  manager()->OnPasswordFormsRendered(&driver_, {});
  task_environment_.RunUntilIdle();
}

// TODO(crbug.com/1425028): Remove the test once
// |kDisablePasswordsDropdownForCvcFields| is launched.
// Same as |FillingAndSavingFallbacksOnCreditCardForm|, but password filling is
// suggested because |kDisablePasswordsDropdownForCvcFields| is disabled.
TEST_F(PasswordManagerTest,
       FillingAndSavingFallbacksOnCreditCardForm_OldBehavior) {
  PasswordFormManager::set_wait_for_server_predictions_for_filling(false);
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(_))
      .WillRepeatedly(Return(true));

  PasswordForm saved_match(MakeSimpleForm());
  store_->AddLogin(saved_match);
  PasswordForm credit_card_form(MakeSimpleCreditCardForm());
  credit_card_form.only_for_fallback = true;

  PasswordFormFillData form_data;
  EXPECT_CALL(driver_, SetPasswordFillData)
      .WillRepeatedly(SaveArg<0>(&form_data));

  manager()->OnPasswordFormsParsed(&driver_, {credit_card_form.form_data});
  task_environment_.RunUntilIdle();

  // Check that manual filling fallback available.
  EXPECT_EQ(saved_match.username_value,
            form_data.preferred_login.username_value);
  EXPECT_EQ(saved_match.password_value,
            form_data.preferred_login.password_value);
  // Check that no automatic filling available.
  EXPECT_TRUE(form_data.username_element_renderer_id.is_null());
  EXPECT_TRUE(form_data.password_element_renderer_id.is_null());

  // Check that saving fallback is available.
  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, ShowManualFallbackForSaving(_, false, false))
      .WillOnce(MoveArg<0>(&form_manager_to_save));
  manager()->OnInformAboutUserInput(&driver_, credit_card_form.form_data);
  ASSERT_TRUE(form_manager_to_save);
  EXPECT_THAT(form_manager_to_save->GetPendingCredentials(),
              FormMatches(credit_card_form));

  // Check that no automatic save prompt is shown.
  OnPasswordFormSubmitted(credit_card_form.form_data);
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword).Times(0);
  manager()->DidNavigateMainFrame(true);
  manager()->OnPasswordFormsRendered(&driver_, {});
  task_environment_.RunUntilIdle();
}

// Check that on a single field OTP form, update bubble is not shown
// automatically, but is available on manual trigger.
TEST_F(PasswordManagerTest, FillingAndSavingFallbacksOnOtpFormWithoutUsername) {
  PasswordFormManager::set_wait_for_server_predictions_for_filling(false);
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(_))
      .WillRepeatedly(Return(true));

  PasswordForm saved_match(MakeSimpleForm());
  store_->AddLogin(saved_match);

  // Create form with an OTP field only.
  PasswordForm one_time_code_form;
  one_time_code_form.url = test_form_url_;
  one_time_code_form.only_for_fallback = true;
  one_time_code_form.password_value = u"123456";
  one_time_code_form.password_element = u"one-time-code";
  one_time_code_form.form_data.url = one_time_code_form.url;
  FormFieldData field;
  field.name_attribute = one_time_code_form.password_element;
  field.value = one_time_code_form.password_value;
  field.form_control_type = autofill::FormControlType::kInputPassword;
  one_time_code_form.form_data.fields.push_back(field);

  PasswordFormFillData form_data;
  EXPECT_CALL(driver_, SetPasswordFillData)
      .WillRepeatedly(SaveArg<0>(&form_data));

  manager()->OnPasswordFormsParsed(&driver_, {one_time_code_form.form_data});
  task_environment_.RunUntilIdle();

  // Check that manual filling fallback available.
  EXPECT_EQ(saved_match.username_value,
            form_data.preferred_login.username_value);
  EXPECT_EQ(saved_match.password_value,
            form_data.preferred_login.password_value);
  // Check that no automatic filling available.
  EXPECT_TRUE(form_data.username_element_renderer_id.is_null());
  EXPECT_TRUE(form_data.password_element_renderer_id.is_null());

  // Check that saving fallback is available.
  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, ShowManualFallbackForSaving(_, false, true))
      .WillOnce(MoveArg<0>(&form_manager_to_save));
  manager()->OnInformAboutUserInput(&driver_, one_time_code_form.form_data);
  ASSERT_TRUE(form_manager_to_save);
  PasswordForm expected_pending_form = saved_match;
  // Only password value change is expected.
  expected_pending_form.password_value = one_time_code_form.password_value;
  EXPECT_THAT(form_manager_to_save->GetPendingCredentials(),
              FormMatches(expected_pending_form));

  // Check that no automatic save prompt is shown.
  OnPasswordFormSubmitted(one_time_code_form.form_data);
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword).Times(0);
  manager()->DidNavigateMainFrame(true);
  manager()->OnPasswordFormsRendered(&driver_, {});
  task_environment_.RunUntilIdle();
}

// Check that on non-password form, update bubble is not shown automatically,
// but is available on manual trigger.
TEST_F(PasswordManagerTest, FillingAndSavingFallbacksOnOtpFormWithUsername) {
  PasswordFormManager::set_wait_for_server_predictions_for_filling(false);
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(_))
      .WillRepeatedly(Return(true));

  PasswordForm saved_match(MakeSimpleForm());
  store_->AddLogin(saved_match);

  PasswordForm one_time_code_form(MakeSimpleForm());
  // Note that the username value was initialized in |MakeSimpleForm| and
  // coincides with the username of the saved match.
  one_time_code_form.only_for_fallback = true;
  one_time_code_form.password_value = u"379 390";
  one_time_code_form.password_element = u"one-time-code";
  one_time_code_form.form_data.fields[1].value =
      one_time_code_form.password_value;
  one_time_code_form.form_data.fields[1].name_attribute =
      one_time_code_form.password_element;

  PasswordFormFillData form_data;
  EXPECT_CALL(driver_, SetPasswordFillData)
      .WillRepeatedly(SaveArg<0>(&form_data));

  manager()->OnPasswordFormsParsed(&driver_, {one_time_code_form.form_data});
  task_environment_.RunUntilIdle();

  // Check that manual filling fallback available.
  EXPECT_EQ(saved_match.username_value,
            form_data.preferred_login.username_value);
  EXPECT_EQ(saved_match.password_value,
            form_data.preferred_login.password_value);
  // Check that no automatic filling available.
  EXPECT_TRUE(form_data.username_element_renderer_id.is_null());
  EXPECT_TRUE(form_data.password_element_renderer_id.is_null());

  // Check that saving fallback is available.
  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, ShowManualFallbackForSaving(_, false, true))
      .WillOnce(MoveArg<0>(&form_manager_to_save));
  manager()->OnInformAboutUserInput(&driver_, one_time_code_form.form_data);
  ASSERT_TRUE(form_manager_to_save);
  PasswordForm expected_pending_form = saved_match;
  // Only password value change is expected.
  expected_pending_form.password_value = one_time_code_form.password_value;
  EXPECT_THAT(form_manager_to_save->GetPendingCredentials(),
              FormMatches(expected_pending_form));

  // Check that no automatic save prompt is shown.
  OnPasswordFormSubmitted(one_time_code_form.form_data);
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword).Times(0);
  manager()->DidNavigateMainFrame(true);
  manager()->OnPasswordFormsRendered(&driver_, {});
  task_environment_.RunUntilIdle();
}

// Check that on successful login the credentials are checked for leak.
TEST_F(PasswordManagerTest, StartLeakDetection) {
  auto mock_factory =
      std::make_unique<testing::StrictMock<MockLeakDetectionCheckFactory>>();
  MockLeakDetectionCheckFactory* weak_factory = mock_factory.get();
  manager()->set_leak_factory(std::move(mock_factory));

  const FormData form_data = MakeSimpleFormData();
  std::vector<FormData> observed = {form_data};
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();

  EXPECT_CALL(client_, IsSavingAndFillingEnabled).WillRepeatedly(Return(true));
  OnPasswordFormSubmitted(form_data);

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword)
      .WillOnce(MoveArgAndReturn<0>(&form_manager_to_save, true));
  auto check_instance = std::make_unique<MockLeakDetectionCheck>();
  EXPECT_CALL(*check_instance,
              Start(LeakDetectionInitiator::kSignInCheck, form_data.url,
                    form_data.fields[0].value, form_data.fields[1].value));
  EXPECT_CALL(*weak_factory, TryCreateLeakCheck)
      .WillOnce(Return(ByMove(std::move(check_instance))));

  // Now the password manager waits for the navigation to complete.
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();
}

// Check that a non-password form with SINGLE_USERNAME prediction is filled.
TEST_F(PasswordManagerTest, FillSingleUsername) {
#if BUILDFLAG(IS_IOS)
  feature_list_.InitAndEnableFeature(features::kIOSPasswordSignInUff);
#endif  // BUIDFLAG(IS_IOS)

  base::HistogramTester histogram_tester;
  PasswordFormManager::set_wait_for_server_predictions_for_filling(true);
  EXPECT_CALL(client_, IsSavingAndFillingEnabled).WillRepeatedly(Return(true));
  const PasswordForm saved_match(MakeSavedForm());
  store_->AddLogin(saved_match);

  // Create FormData for a form with 1 text field.
  FormData form_data = MakeSingleUsernameFormData();

  PasswordFormFillData fill_data;
  EXPECT_CALL(driver_, SetPasswordFillData).WillOnce(SaveArg<0>(&fill_data));
  manager()->ProcessAutofillPredictions(
      &driver_, form_data,
      CreateServerPredictions(form_data,
                              {{0, ServerFieldType::SINGLE_USERNAME}}));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(form_data.unique_renderer_id, fill_data.form_renderer_id);
  EXPECT_EQ(saved_match.username_value,
            fill_data.preferred_login.username_value);
  EXPECT_EQ(form_data.fields[0].unique_renderer_id,
            fill_data.username_element_renderer_id);
  EXPECT_EQ(saved_match.password_value,
            fill_data.preferred_login.password_value);
  EXPECT_TRUE(fill_data.password_element_renderer_id.is_null());

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.SingleUsername.ForgotPasswordServerPredictionUsed",
      false, 1);
}

// Check that a non-password form with SINGLE_USERNAME_FORGOT_PASSWORD
// prediction is filled.
TEST_F(PasswordManagerTest, FillSingleUsernameForgotPassword) {
  std::vector<FeatureRef> enabled_features = {
      password_manager::features::kForgotPasswordFormSupport};
#if BUILDFLAG(IS_IOS)
  enabled_features.push_back(features::kIOSPasswordSignInUff);
#endif  // BUIDFLAG(IS_IOS)
  feature_list_.InitWithFeatures(enabled_features, /*disabled_features=*/{});

  base::HistogramTester histogram_tester;
  PasswordFormManager::set_wait_for_server_predictions_for_filling(true);
  EXPECT_CALL(client_, IsSavingAndFillingEnabled).WillRepeatedly(Return(true));
  const PasswordForm saved_match(MakeSavedForm());
  store_->AddLogin(saved_match);

  // Create FormData for a form with 1 text field.
  FormData form_data = MakeSingleUsernameFormData();

  PasswordFormFillData fill_data;
  EXPECT_CALL(driver_, SetPasswordFillData).WillOnce(SaveArg<0>(&fill_data));
  manager()->ProcessAutofillPredictions(
      &driver_, form_data,
      CreateServerPredictions(
          form_data, {{0, ServerFieldType::SINGLE_USERNAME_FORGOT_PASSWORD}}));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(form_data.unique_renderer_id, fill_data.form_renderer_id);
  EXPECT_EQ(saved_match.username_value,
            fill_data.preferred_login.username_value);
  EXPECT_EQ(form_data.fields[0].unique_renderer_id,
            fill_data.username_element_renderer_id);
  EXPECT_EQ(saved_match.password_value,
            fill_data.preferred_login.password_value);
  EXPECT_TRUE(fill_data.password_element_renderer_id.is_null());

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.SingleUsername.ForgotPasswordServerPredictionUsed", true,
      1);
}

// Checks that a password form with a clear-text account creation field results
// in marking the password field as eligible for password generation.
TEST_F(PasswordManagerTest,
       MarkServerPredictedClearTextPasswordFieldEligibleForGeneration) {
  PasswordFormManager::set_wait_for_server_predictions_for_filling(true);
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(_))
      .WillRepeatedly(Return(true));
  PasswordForm saved_match(MakeSavedForm());
  store_->AddLogin(saved_match);

  // Create FormdData for a form with 1 text field.
  FormData form_data;
  constexpr FormRendererId form_id(1001);
  form_data.unique_renderer_id = form_id;
  form_data.url = GURL("http://example.com");

  FormFieldData username_field;
  username_field.form_control_type = autofill::FormControlType::kInputText;
  constexpr FieldRendererId username_field_id(10);
  username_field.unique_renderer_id = username_field_id;
  form_data.fields.push_back(username_field);

  FormFieldData password_field;
  password_field.form_control_type = autofill::FormControlType::kInputText;
  constexpr FieldRendererId password_field_id(11);
  password_field.unique_renderer_id = password_field_id;
  form_data.fields.push_back(password_field);

  autofill::PasswordFormGenerationData form_generation_data;
  EXPECT_CALL(driver_, FormEligibleForGenerationFound)
      .WillOnce(SaveArg<0>(&form_generation_data));
  manager()->ProcessAutofillPredictions(
      &driver_, form_data,
      CreateServerPredictions(
          form_data, {{1, ServerFieldType::ACCOUNT_CREATION_PASSWORD}}));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(password_field_id, form_generation_data.new_password_renderer_id);
}

// Checks that username is suggested in the save prompt and saved on username
// first flow if SINGLE_USERNAME server prediction is available.
TEST_F(PasswordManagerTest, UsernameFirstFlowSavingWithServerPredictions) {
  PasswordForm saved_form(MakeSavedForm());
  store_->AddLogin(saved_form);

  // Simulate the user typed a previously not saved username in the username
  // form.
  PasswordForm username_form(MakeSimpleFormWithOnlyUsernameField());
  const std::u16string username = u"newusername@gmail.com";
  ASSERT_NE(saved_form.username_value, username);
  EXPECT_CALL(driver_, GetLastCommittedURL())
      .WillOnce(ReturnRef(username_form.url));
  manager()->OnUserModifiedNonPasswordField(
      &driver_, username_form.form_data.fields[0].unique_renderer_id,
      /*value=*/username,
      /*autocomplete_attribute_has_username=*/false,
      /*is_likely_otp=*/false);
  task_environment_.RunUntilIdle();

  // Set up a server prediction for the single username field.
  manager()->ProcessAutofillPredictions(
      &driver_, username_form.form_data,
      CreateServerPredictions(username_form.form_data,
                              {{0, ServerFieldType::SINGLE_USERNAME}}));
  task_environment_.RunUntilIdle();

  // Simulate that a form which contains only 1 password field is added
  // to the page.
  PasswordForm password_form(MakeSimpleFormWithOnlyPasswordField());
  manager()->OnPasswordFormsParsed(&driver_,
                                   {password_form.form_data} /* observed */);
  task_environment_.RunUntilIdle();
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(password_form.url))
      .WillRepeatedly(Return(true));

  // Simulate that the user typed a password and submitted the password form.
  const std::u16string password = u"newpassword";
  password_form.form_data.fields[0].value = password;
  OnPasswordFormSubmitted(password_form.form_data);

  // Simulate successful submission and expect a save prompt.
  std::unique_ptr<PasswordFormManagerForUI> form_manager;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword)
      .WillOnce(MoveArgAndReturn<0>(&form_manager, false));
  manager()->OnPasswordFormsRendered(&driver_, {} /* observed */);
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(form_manager);

  // Simulate accepting the prompt and expect saving the new credential.
  form_manager->Save();
  task_environment_.RunUntilIdle();

  password_form.username_value = username;
  password_form.password_value = password;
  EXPECT_THAT(store_->stored_passwords(),
              ElementsAre(Pair(password_form.signon_realm,
                               UnorderedElementsAre(FormMatches(password_form),
                                                    FormMatches(saved_form)))));
}

// Tests complex case of UFF on sign-up form that also has intermediary fields.
// This test asserts that if there are server predictions for single username
// field and no username server prediction inside the password form, username is
// overwritten.
TEST_F(PasswordManagerTest, UsernameFirstFlowSignUpFormWithIntermediaryFields) {
  feature_list_.InitWithFeatures(
      /*enabled_features=*/
      {password_manager::features::kUsernameFirstFlowStoreSeveralValues,
       password_manager::features::kUsernameFirstFlowWithIntermediateValues},
      /*disabled_features_*/ {});
  // Simulate the user typed a previously not saved username in the username
  // form.
  PasswordForm username_form(MakeSimpleFormWithOnlyUsernameField());
  const std::u16string single_username = u"newusername@gmail.com";
  EXPECT_CALL(driver_, GetLastCommittedURL())
      .WillRepeatedly(ReturnRef(username_form.url));
  manager()->OnUserModifiedNonPasswordField(
      &driver_, username_form.form_data.fields[0].unique_renderer_id,
      /*value=*/single_username,
      /*autocomplete_attribute_has_username=*/false,
      /*is_likely_otp=*/false);

  PasswordForm intermediary_form(
      MakeSimpleFormWithIntermediaryFieldInUsernameFirstFlow());
  // User enters OTP.
  manager()->OnUserModifiedNonPasswordField(
      &driver_, intermediary_form.form_data.fields[0].unique_renderer_id,
      /*value=*/intermediary_form.form_data.fields[0].value,
      /*autocomplete_attribute_has_username=*/false,
      /*is_likely_otp=*/true);

  // Simulate a password form which contains no username, but other text fields.
  PasswordForm password_form(MakeSignUpFormWithoutUsername());
  manager()->OnPasswordFormsParsed(&driver_, {password_form.form_data});

  // Set up a server prediction for the single username field and password form.
  manager()->ProcessAutofillPredictions(
      &driver_, username_form.form_data,
      CreateServerPredictions(username_form.form_data,
                              {{0, ServerFieldType::SINGLE_USERNAME}}));
  manager()->ProcessAutofillPredictions(
      &driver_, password_form.form_data,
      CreateServerPredictions(password_form.form_data,
                              {{0, ServerFieldType::NAME_FIRST},
                               {1, ServerFieldType::NAME_LAST},
                               {2, ServerFieldType::NEW_PASSWORD}}));

  manager()->OnUserModifiedNonPasswordField(
      &driver_, password_form.form_data.fields[0].unique_renderer_id,
      /*value=*/password_form.form_data.fields[0].value,
      /*autocomplete_attribute_has_username=*/false,
      /*is_likely_otp=*/false);

  manager()->OnUserModifiedNonPasswordField(
      &driver_, password_form.form_data.fields[1].unique_renderer_id,
      /*value=*/password_form.form_data.fields[1].value,
      /*autocomplete_attribute_has_username=*/false,
      /*is_likely_otp=*/false);

  task_environment_.RunUntilIdle();

  EXPECT_CALL(client_, IsSavingAndFillingEnabled(password_form.url))
      .WillRepeatedly(Return(true));

  // Simulate that the user typed a password and submitted the password form.
  manager()->OnInformAboutUserInput(&driver_, password_form.form_data);

  OnPasswordFormSubmitted(password_form.form_data);

  // Simulate successful submission and expect a save prompt.
  std::unique_ptr<PasswordFormManagerForUI> form_manager;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword)
      .WillOnce(MoveArgAndReturn<0>(&form_manager, false));
  manager()->OnPasswordFormsRendered(&driver_, /*visible_forms_data=*/{});
  ASSERT_TRUE(form_manager);

  // Simulate accepting the prompt and expect saving the new credential.
  form_manager->Save();
  task_environment_.RunUntilIdle();

  // Possible usernames are cleared after successful login.
  EXPECT_THAT(manager()->possible_usernames(), IsEmpty());

  PasswordForm expected_form(password_form);
  expected_form.username_value = single_username;
  expected_form.username_element = u"";
  expected_form.username_element_renderer_id = FieldRendererId(0);
  EXPECT_THAT(
      store_->stored_passwords(),
      ElementsAre(Pair(password_form.signon_realm,
                       UnorderedElementsAre(FormMatches(expected_form)))));
}

// Tests complex case of UFF on sign-in form that also has intermediary fields.
// This test asserts that possible username with server prediction is
// prioritized, even if there is more recently user typed possible username
// candidate that is without server prediction.
TEST_F(PasswordManagerTest, UsernameFirstFlowSignInFormWithIntermediaryFields) {
  feature_list_.InitWithFeatures(
      /*enabled_features=*/
      {password_manager::features::kUsernameFirstFlowStoreSeveralValues,
       password_manager::features::kUsernameFirstFlowWithIntermediateValues},
      /*disabled_features_*/ {});
  // Simulate the user typed a previously not saved username in the username
  // form.
  PasswordForm username_form(MakeSimpleFormWithOnlyUsernameField());
  const std::u16string single_username = u"newusername@gmail.com";
  EXPECT_CALL(driver_, GetLastCommittedURL())
      .WillRepeatedly(ReturnRef(username_form.url));
  manager()->OnUserModifiedNonPasswordField(
      &driver_, username_form.form_data.fields[0].unique_renderer_id,
      /*value=*/single_username,
      /*autocomplete_attribute_has_username=*/false,
      /*is_likely_otp=*/false);

  PasswordForm intermediary_form(
      MakeSimpleFormWithIntermediaryFieldInUsernameFirstFlow());
  // User enters OTP.
  manager()->OnUserModifiedNonPasswordField(
      &driver_, intermediary_form.form_data.fields[0].unique_renderer_id,
      /*value=*/intermediary_form.form_data.fields[0].value,
      /*autocomplete_attribute_has_username=*/false,
      /*is_likely_otp=*/true);

  // Simulate a password form which contains no username, but other text fields.
  PasswordForm password_form(MakeSimpleFormWithOnlyPasswordField());
  manager()->OnPasswordFormsParsed(&driver_, {password_form.form_data});

  // Set up a server prediction for the single username field and password form.
  manager()->ProcessAutofillPredictions(
      &driver_, username_form.form_data,
      CreateServerPredictions(username_form.form_data,
                              {{0, ServerFieldType::SINGLE_USERNAME}}));

  task_environment_.RunUntilIdle();

  EXPECT_CALL(client_, IsSavingAndFillingEnabled(password_form.url))
      .WillRepeatedly(Return(true));

  // Simulate that the user typed a password and submitted the password form.
  manager()->OnInformAboutUserInput(&driver_, password_form.form_data);

  OnPasswordFormSubmitted(password_form.form_data);

  // Simulate successful submission and expect a save prompt.
  std::unique_ptr<PasswordFormManagerForUI> form_manager;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword)
      .WillOnce(MoveArgAndReturn<0>(&form_manager, false));
  manager()->OnPasswordFormsRendered(&driver_, /*visible_forms_data=*/{});
  ASSERT_TRUE(form_manager);

  // Simulate accepting the prompt and expect saving the new credential.
  form_manager->Save();
  task_environment_.RunUntilIdle();
  // Possible usernames are cleared after successful login.
  EXPECT_THAT(manager()->possible_usernames(), IsEmpty());

  PasswordForm expected_form(password_form);
  expected_form.username_value = single_username;
  EXPECT_THAT(
      store_->stored_passwords(),
      ElementsAre(Pair(password_form.signon_realm,
                       UnorderedElementsAre(FormMatches(expected_form)))));
}

// Checks that a server predicted single username outside of the form is
// suggested as a username in "save password" prompt, even though there are text
// fields in the password form.
TEST_F(PasswordManagerTest, UsernameFirstFlowSavingOnPasswordFormWithCaptcha) {
  feature_list_.InitWithFeatures(
      /*enabled_features=*/
      {password_manager::features::kUsernameFirstFlowStoreSeveralValues,
       password_manager::features::kUsernameFirstFlowWithIntermediateValues},
      /*disabled_features_*/ {});
  // Simulate the user typed a previously not saved username in the username
  // form.
  PasswordForm username_form(MakeSimpleFormWithOnlyUsernameField());
  const std::u16string single_username = u"newusername@gmail.com";
  EXPECT_CALL(driver_, GetLastCommittedURL())
      .WillRepeatedly(ReturnRef(username_form.url));
  manager()->OnUserModifiedNonPasswordField(
      &driver_, username_form.form_data.fields[0].unique_renderer_id,
      /*value=*/single_username,
      /*autocomplete_attribute_has_username=*/false,
      /*is_likely_otp=*/false);

  // Set up a server prediction for the single username field.
  manager()->ProcessAutofillPredictions(
      &driver_, username_form.form_data,
      CreateServerPredictions(username_form.form_data,
                              {{0, ServerFieldType::SINGLE_USERNAME}}));

  // Simulate a password form which contains no username, but other text field
  // that can't be parsed as a username.
  PasswordForm password_form(MakeSimpleFormWithPasswordAndCaptchaFields());
  manager()->OnPasswordFormsParsed(&driver_, {password_form.form_data});
  task_environment_.RunUntilIdle();

  EXPECT_CALL(client_, IsSavingAndFillingEnabled(password_form.url))
      .WillRepeatedly(Return(true));

  // Simulate that the user typed a password and submitted the password form.
  manager()->OnInformAboutUserInput(&driver_, password_form.form_data);

  // User enters CAPTCHA.
  manager()->OnUserModifiedNonPasswordField(
      &driver_, password_form.form_data.fields[1].unique_renderer_id,
      /*value=*/password_form.form_data.fields[1].value,
      /*autocomplete_attribute_has_username=*/false,
      /*is_likely_otp=*/false);

  manager()->OnInformAboutUserInput(&driver_, password_form.form_data);
  OnPasswordFormSubmitted(password_form.form_data);

  // Simulate successful submission and expect a save prompt.
  std::unique_ptr<PasswordFormManagerForUI> form_manager;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword)
      .WillOnce(MoveArgAndReturn<0>(&form_manager, false));
  manager()->OnPasswordFormsRendered(&driver_, {});
  ASSERT_TRUE(form_manager);

  // Simulate accepting the prompt and expect saving the new credential.
  form_manager->Save();
  task_environment_.RunUntilIdle();

  EXPECT_THAT(password_form.username_value, IsEmpty());
  PasswordForm expected_form(password_form);
  expected_form.username_value = single_username;
  EXPECT_THAT(
      store_->stored_passwords(),
      ElementsAre(Pair(expected_form.signon_realm,
                       UnorderedElementsAre(FormMatches(expected_form)))));
}

// This test simulates user journey of single username form, followed by OTP
// form, and then sign-up form (without a username field). The single username
// form has `SINGLE_USERNAME` server override and sign-up form has `USERNAME`
// server prediction inside the form. Tests that `SINGLE_USERNAME` value will be
// picked as a username, even if there is a username server prediction in
// sign-up form.
TEST_F(PasswordManagerTest,
       UsernameFirstFlowSavingSignUpFormWithServerOverride) {
  feature_list_.InitWithFeatures(
      /*enabled_features=*/
      {password_manager::features::kUsernameFirstFlowStoreSeveralValues,
       password_manager::features::kUsernameFirstFlowWithIntermediateValues},
      /*disabled_features_*/ {});

  // Simulate the user typed a previously not saved username in the username
  // form.
  PasswordForm username_form(MakeSimpleFormWithOnlyUsernameField());
  const std::u16string single_username = u"newusername@gmail.com";
  EXPECT_CALL(driver_, GetLastCommittedURL())
      .WillRepeatedly(ReturnRef(username_form.url));
  manager()->OnUserModifiedNonPasswordField(
      &driver_, username_form.form_data.fields[0].unique_renderer_id,
      /*value=*/single_username,
      /*autocomplete_attribute_has_username=*/false,
      /*is_likely_otp=*/false);

  PasswordForm intermediary_form(
      MakeSimpleFormWithIntermediaryFieldInUsernameFirstFlow());
  // User enters OTP.
  manager()->OnUserModifiedNonPasswordField(
      &driver_, intermediary_form.form_data.fields[0].unique_renderer_id,
      /*value=*/intermediary_form.form_data.fields[0].value,
      /*autocomplete_attribute_has_username=*/false,
      /*is_likely_otp=*/true);

  // Simulate that a password form which contains no username, but other text
  // field, that can be parsed as username.
  PasswordForm signup_form(MakeSignUpFormWithoutUsername());

  manager()->OnPasswordFormsParsed(&driver_, {signup_form.form_data});
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(signup_form.url))
      .WillRepeatedly(Return(true));

  // Set up a override prediction for the single username field.
  manager()->ProcessAutofillPredictions(
      &driver_, username_form.form_data,
      CreateServerPredictions(username_form.form_data,
                              {{0, ServerFieldType::SINGLE_USERNAME}},
                              /*is_override=*/true));

  manager()->ProcessAutofillPredictions(
      &driver_, signup_form.form_data,
      CreateServerPredictions(signup_form.form_data,
                              {{1, ServerFieldType::USERNAME},
                               {2, ServerFieldType::NEW_PASSWORD}}));

  task_environment_.RunUntilIdle();

  // Simulate user modifying the text fields inside password form.
  manager()->OnUserModifiedNonPasswordField(
      &driver_, signup_form.form_data.fields[0].unique_renderer_id,
      /*value=*/signup_form.form_data.fields[0].value,
      /*autocomplete_attribute_has_username=*/false,
      /*is_likely_otp=*/false);
  manager()->OnUserModifiedNonPasswordField(
      &driver_, signup_form.form_data.fields[1].unique_renderer_id,
      /*value=*/signup_form.form_data.fields[1].value,
      /*autocomplete_attribute_has_username=*/false,
      /*is_likely_otp=*/false);

  OnPasswordFormSubmitted(signup_form.form_data);

  // Simulate successful submission and expect a save prompt.
  std::unique_ptr<PasswordFormManagerForUI> form_manager;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword)
      .WillOnce(MoveArgAndReturn<0>(&form_manager, false));
  manager()->OnPasswordFormsRendered(&driver_, /*visible_forms_data=*/{});
  ASSERT_TRUE(form_manager);

  // Simulate accepting the prompt and expect saving the new credential.
  form_manager->Save();
  task_environment_.RunUntilIdle();

  PasswordForm expected_form(signup_form);
  expected_form.username_value = single_username;
  expected_form.username_element.clear();
  expected_form.username_element_renderer_id = FieldRendererId();
  EXPECT_THAT(
      store_->stored_passwords(),
      ElementsAre(Pair(expected_form.signon_realm,
                       UnorderedElementsAre(FormMatches(expected_form)))));
}

// This test simulates the user journey consisting of single username form that
// user typed in a long time ago, followed by sign-in form (without a username
// field). Checks that for the simulated user journey password manager will not
// show username in save password prompt.
TEST_F(PasswordManagerTest,
       UsernameFirstFlowSavingSignInFormWithStaleServerOverride) {
  feature_list_.InitWithFeatures(
      /*enabled_features=*/
      {password_manager::features::kUsernameFirstFlowStoreSeveralValues,
       password_manager::features::kUsernameFirstFlowWithIntermediateValues},
      /*disabled_features=*/{});

  // Simulate that user typed into a text field outside of the password form.
  PasswordForm stale_username_form(MakeSimpleFormWithOnlyUsernameField());
  const std::u16string stale_single_username = u"stale_single_username";
  EXPECT_CALL(driver_, GetLastCommittedURL())
      .WillRepeatedly(ReturnRef(stale_username_form.url));
  manager()->OnUserModifiedNonPasswordField(
      &driver_, stale_username_form.form_data.fields[0].unique_renderer_id,
      /*value=*/stale_single_username,
      /*autocomplete_attribute_has_username=*/false,
      /*is_likely_otp=*/false);

  // Make single username form stale by fast forwarding time.
  task_environment_.FastForwardBy(kPossibleUsernameExtendedExpirationTimeout +
                                  base::Seconds(5));
  EXPECT_TRUE(manager()->possible_usernames().begin()->second.IsStale());

  // Simulate that a password form which contains no username, but other text
  // field, that can be parsed as username.
  PasswordForm signin_form(MakeSimpleFormWithOnlyPasswordField());

  manager()->OnPasswordFormsParsed(&driver_, {signin_form.form_data});
  manager()->OnPasswordFormsRendered(&driver_, {signin_form.form_data});
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(signin_form.url))
      .WillRepeatedly(Return(true));

  // Set up an override prediction for the stale single username field.
  manager()->ProcessAutofillPredictions(
      &driver_, stale_username_form.form_data,
      CreateServerPredictions(stale_username_form.form_data,
                              {{0, ServerFieldType::SINGLE_USERNAME}},
                              /*is_override=*/true));

  // Simulate successful submission and expect a save prompt.
  task_environment_.RunUntilIdle();
  OnPasswordFormSubmitted(signin_form.form_data);
  std::unique_ptr<PasswordFormManagerForUI> form_manager;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword)
      .WillOnce(MoveArgAndReturn<0>(&form_manager, false));
  manager()->OnPasswordFormsRendered(&driver_, /*visible_forms_data=*/{});
  ASSERT_TRUE(form_manager);

  // Simulate accepting the prompt and expect saving the new credential.
  form_manager->Save();
  task_environment_.RunUntilIdle();

  PasswordForm expected_form(signin_form);
  EXPECT_THAT(expected_form.username_value, IsEmpty());
  EXPECT_THAT(
      store_->stored_passwords(),
      ElementsAre(Pair(expected_form.signon_realm,
                       UnorderedElementsAre(FormMatches(expected_form)))));
}

// Tests complex case of UFF on sign-in form that also has intermediary fields.
// This test asserts that possible username with autocomplete="username" is
// prioritized, even if there is more recently user typed possible username
// candidate that is without server prediction.
TEST_F(PasswordManagerTest,
       UsernameFirstFlowWithIntermediaryFieldsAndAutocomplete) {
  feature_list_.InitWithFeatures(
      /*enabled_features=*/
      {password_manager::features::kUsernameFirstFlowStoreSeveralValues,
       password_manager::features::kUsernameFirstFlowWithIntermediateValues,
       password_manager::features::kUsernameFirstFlowHonorAutocomplete},
      /*disabled_features_*/ {});
  // Simulate the user typed a previously not saved username in the username
  // form.
  PasswordForm username_form(MakeSimpleFormWithOnlyUsernameField());
  const std::u16string single_username = u"newusername@gmail.com";
  EXPECT_CALL(driver_, GetLastCommittedURL())
      .WillRepeatedly(ReturnRef(username_form.url));
  manager()->OnUserModifiedNonPasswordField(
      &driver_, username_form.form_data.fields[0].unique_renderer_id,
      /*value=*/single_username,
      /*autocomplete_attribute_has_username=*/true,
      /*is_likely_otp=*/false);

  // Form with text field and password field.
  FormData search_bar(MakeSearchBarFormData());
  // User enters something to search bar.
  manager()->OnUserModifiedNonPasswordField(
      &driver_, search_bar.fields[0].unique_renderer_id,
      /*value=*/search_bar.fields[0].value,
      /*autocomplete_attribute_has_username=*/false,
      /*is_likely_otp=*/false);

  // Simulate a password form which contains no username, but other text fields.
  PasswordForm password_form(MakeSimpleFormWithOnlyPasswordField());
  manager()->OnPasswordFormsParsed(&driver_, {password_form.form_data});
  manager()->OnPasswordFormsRendered(
      &driver_, /*visible_forms_data=*/{password_form.form_data});

  EXPECT_CALL(client_, IsSavingAndFillingEnabled(password_form.url))
      .WillRepeatedly(Return(true));

  // Simulate that the user typed a password and submitted the password form.
  manager()->OnInformAboutUserInput(&driver_, password_form.form_data);

  // Simulate successful submission and expect a save prompt.
  task_environment_.RunUntilIdle();
  OnPasswordFormSubmitted(password_form.form_data);
  std::unique_ptr<PasswordFormManagerForUI> form_manager;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword)
      .WillOnce(MoveArgAndReturn<0>(&form_manager, false));
  manager()->OnPasswordFormsRendered(&driver_, /*visible_forms_data=*/{});
  ASSERT_TRUE(form_manager);

  // Simulate accepting the prompt and expect saving the new credential.
  form_manager->Save();
  task_environment_.RunUntilIdle();

  // Possible usernames are cleared after successful login.
  EXPECT_THAT(manager()->possible_usernames(), IsEmpty());

  PasswordForm expected_form(password_form);
  expected_form.username_value = single_username;
  EXPECT_THAT(
      store_->stored_passwords(),
      ElementsAre(Pair(password_form.signon_realm,
                       UnorderedElementsAre(FormMatches(expected_form)))));
}

// Checks that possible single username value is not suggested in the save
// prompt if SINGLE_USERNAME server prediction is not available.
TEST_F(PasswordManagerTest, UsernameFirstFlowSavingWithoutServerPredictions) {
  PasswordForm saved_form(MakeSavedForm());
  store_->AddLogin(saved_form);

  // Simulate the user typed a previously not saved username in username form.
  PasswordForm username_form(MakeSimpleFormWithOnlyUsernameField());
  const std::u16string username = u"newusername@gmail.com";
  ASSERT_TRUE(saved_form.username_value != username);
  EXPECT_CALL(driver_, GetLastCommittedURL())
      .WillOnce(ReturnRef(username_form.url));
  manager()->OnUserModifiedNonPasswordField(
      &driver_, username_form.form_data.fields[0].unique_renderer_id,
      /*value=*/username,
      /*autocomplete_attribute_has_username=*/false,
      /*is_likely_otp=*/false);

  // Simulate that a form which contains only 1 field which is password is added
  // to the page.
  PasswordForm password_form(MakeSimpleFormWithOnlyPasswordField());
  manager()->OnPasswordFormsParsed(&driver_,
                                   {password_form.form_data} /* observed */);
  task_environment_.RunUntilIdle();
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(password_form.url))
      .WillRepeatedly(Return(true));

  // Simulate that the user typed previously not saved password and submitted
  // the password form.
  const std::u16string password = u"newpassword";
  ASSERT_TRUE(saved_form.password_value != password);
  password_form.form_data.fields[0].value = password;
  OnPasswordFormSubmitted(password_form.form_data);

  // Simulate successful submission and expect a prompt.
  std::unique_ptr<PasswordFormManagerForUI> form_manager;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword)
      .WillOnce(MoveArgAndReturn<0>(&form_manager, false));
  manager()->OnPasswordFormsRendered(&driver_, {} /* observed */);
  ASSERT_TRUE(form_manager);

  // Simulate accepting the prompt and expect updating the credential (without
  // a server prediction, the single |username| is ignored, so the new password
  // is associated with the old username).
  form_manager->Save();
  task_environment_.RunUntilIdle();

  saved_form.password_value = password;
  EXPECT_THAT(store_->stored_passwords(),
              ElementsAre(Pair(saved_form.signon_realm,
                               ElementsAre(FormMatches(saved_form)))));
}

// Tests that Password Manager's behavior on usernames outside of the password
// form can be controlled by Finch config param. Modifies LRU cache's size that
// stores possible single usernames by a finch param. Then, single username
// override will not be considered as a candidate for password form with
// username and password fields.
TEST_F(PasswordManagerTest, UsernameFirstFlowCacheSizeFromFinchParam) {
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(test_form_url_))
      .WillRepeatedly(Return(true));
  base::test::ScopedFeatureList features;
  base::FieldTrialParams feature_parameters{
      {features::kMaxSingleUsernameFieldsToStore.name, "1"}};
  features.InitAndEnableFeatureWithParameters(
      features::kUsernameFirstFlowStoreSeveralValues, feature_parameters);
  // Reinitialize `PasswordManager` class, so that cache size is picked from the
  // finch feature param.
  manager_ = std::make_unique<PasswordManager>(&client_);
  manager_->set_leak_factory(
      std::make_unique<testing::NiceMock<MockLeakDetectionCheckFactory>>());

  // Simulate the user typed in a username form.
  PasswordForm username_form(MakeSimpleFormWithOnlyUsernameField());
  const std::u16string username = u"newusername@gmail.com";
  EXPECT_CALL(driver_, GetLastCommittedURL())
      .WillRepeatedly(ReturnRef(username_form.url));
  manager()->OnUserModifiedNonPasswordField(
      &driver_, username_form.form_data.fields[0].unique_renderer_id,
      /*value=*/username,
      /*autocomplete_attribute_has_username=*/false,
      /*is_likely_otp=*/false);
  manager()->ProcessAutofillPredictions(
      &driver_, username_form.form_data,
      CreateServerPredictions(username_form.form_data,
                              {{0, ServerFieldType::SINGLE_USERNAME}},
                              /*is_override=*/true));

  // Simulate that a form which contains username and password fields is added
  // to the page.
  PasswordForm password_form(MakeSimpleForm());
  manager()->OnPasswordFormsParsed(&driver_, {password_form.form_data});
  manager()->OnPasswordFormsRendered(
      &driver_, /*visible_forms_data=*/{password_form.form_data});

  // Simulate user modifying the text fields inside password form.
  manager()->OnUserModifiedNonPasswordField(
      &driver_, password_form.form_data.fields[0].unique_renderer_id,
      /*value=*/password_form.form_data.fields[0].value,
      /*autocomplete_attribute_has_username=*/false,
      /*is_likely_otp=*/false);

  std::unique_ptr<PasswordFormManagerForUI> form_manager;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword)
      .WillOnce(MoveArgAndReturn<0>(&form_manager, false));
  task_environment_.RunUntilIdle();
  OnPasswordFormSubmitted(password_form.form_data);
  manager()->OnPasswordFormsRendered(&driver_, /*visible_forms_data=*/{});
  ASSERT_TRUE(form_manager);
  // Even though there was a text field with single username override, it
  // wasn't picked since the LRU cache size is limited to 1.
  EXPECT_THAT(form_manager->GetPendingCredentials(),
              FormMatches(password_form));
}

// Tests submitting an OTP password form after a single username form.
// `OnLoginSuccessful` doesn't get called, so `possible_usernames_` are not
// cleared, and the potential single username value is proposed in the manual
// fallback for saving.
TEST_F(PasswordManagerTest, UsernameFirstFlowOTPPasswordForm) {
  // Simulate the user typed a previously not saved username in username form.
  PasswordForm username_form(MakeSimpleFormWithOnlyUsernameField());
  const std::u16string kUsername = u"newusername@gmail.com";
  EXPECT_CALL(driver_, GetLastCommittedURL())
      .WillOnce(ReturnRef(username_form.url));
  manager()->OnUserModifiedNonPasswordField(
      &driver_, username_form.form_data.fields[0].unique_renderer_id,
      /*value=*/kUsername,
      /*autocomplete_attribute_has_username=*/false,
      /*is_likely_otp=*/false);
  // Set up a server prediction for the single username field.
  manager()->ProcessAutofillPredictions(
      &driver_, username_form.form_data,
      CreateServerPredictions(username_form.form_data,
                              {{0, ServerFieldType::SINGLE_USERNAME}}));

  PasswordForm otp_form = MakeSimpleFormWithOnlyPasswordField();
  otp_form.password_element = u"one-time-code";
  otp_form.only_for_fallback = true;
  otp_form.form_data.fields[0].name = otp_form.password_element;
  otp_form.form_data.fields[0].name_attribute = otp_form.password_element;

  manager()->OnPasswordFormsParsed(&driver_, {otp_form.form_data});
  manager()->OnPasswordFormsRendered(&driver_, {otp_form.form_data});
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(otp_form.url))
      .WillRepeatedly(Return(true));

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, ShowManualFallbackForSaving)
      .WillOnce(MoveArg<0>(&form_manager_to_save));
  task_environment_.RunUntilIdle();
  manager()->OnInformAboutUserInput(&driver_, otp_form.form_data);
  ASSERT_TRUE(form_manager_to_save);

  // Username and password are still available for saving in case of a manual
  // fallback. Currently, we don't have excellent confidence in OTP detection.
  PasswordForm expected_pending_form(otp_form);
  expected_pending_form.username_value = kUsername;
  EXPECT_THAT(form_manager_to_save->GetPendingCredentials(),
              FormMatches(expected_pending_form));

  // Simulate that the user submitted the password form.
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword).Times(0);
  OnPasswordFormSubmitted(otp_form.form_data);
  manager()->OnPasswordFormsRendered(&driver_, /*visible_forms_data=*/{});
  // Possible usernames are not cleared after OTP form submission.
  EXPECT_THAT(manager()->possible_usernames(),
              ElementsAre(Pair(
                  PossibleUsernameFieldIdentifier(
                      driver_.GetId(),
                      username_form.form_data.fields[0].unique_renderer_id),
                  _)));
}

// Checks that possible single username value is not used to build pending
// credentials if a navigation that cannot be a result of form submission
// happens between submitting single password and single username forms.
TEST_F(PasswordManagerTest, UsernameFirstFlowWithNavigationInTheMiddle) {
  // Simulate the user typed a previously not saved username in username form.
  PasswordForm username_form(MakeSimpleFormWithOnlyUsernameField());
  EXPECT_CALL(driver_, GetLastCommittedURL())
      .WillOnce(ReturnRef(username_form.url));
  manager()->OnUserModifiedNonPasswordField(
      &driver_, username_form.form_data.fields[0].unique_renderer_id,
      /*value=*/u"newusername@gmail.com",
      /*autocomplete_attribute_has_username=*/false, /*is_likely_otp=*/false);

  // Setup a server prediction for the single username field to
  // allow using possible username value for pending credentials.
  manager()->ProcessAutofillPredictions(
      &driver_, username_form.form_data,
      CreateServerPredictions(username_form.form_data,
                              {{0, ServerFieldType::SINGLE_USERNAME}}));

  // Simulate navigation to a single password form that cannot be a result of a
  // form submisison.
  manager()->DidNavigateMainFrame(/*form_may_be_submitted=*/false);
  PasswordForm password_form(MakeSimpleFormWithOnlyPasswordField());
  FormData password_form_data = password_form.form_data;
  manager()->OnPasswordFormsParsed(&driver_, {password_form_data});
  task_environment_.RunUntilIdle();
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(password_form.url))
      .WillRepeatedly(Return(true));

  // Simulate that the user typed previously not saved password.
  password_form_data.fields[0].value = u"new_password";
  manager()->OnInformAboutUserInput(&driver_, password_form_data);

  // Check that single username is not used to build pending credentials.
  PasswordFormManager* submitted_form_manager =
      manager()->GetSubmittedManagerForTest();
  ASSERT_TRUE(submitted_form_manager);
  EXPECT_TRUE(
      submitted_form_manager->GetPendingCredentials().username_value.empty());
}

TEST_F(PasswordManagerTest, FormSubmittedOnPrimaryMainFrame) {
  EXPECT_CALL(client_, IsSavingAndFillingEnabled).WillRepeatedly(Return(true));
  FormData form_data(MakeSimpleFormData());

  // Submit |form| on a main frame.
  manager()->OnPasswordFormsParsed(&driver_, {form_data} /* observed */);
  manager()->OnPasswordFormSubmitted(&driver_, form_data);
  task_environment_.RunUntilIdle();

  // Simulate finish loading of some iframe.
  MockPasswordManagerDriver iframe_driver;
  EXPECT_CALL(iframe_driver, IsInPrimaryMainFrame())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(iframe_driver, GetId()).WillRepeatedly(Return(123));
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword).Times(0);
  manager()->OnPasswordFormsRendered(&iframe_driver, {} /* observed */);
  task_environment_.RunUntilIdle();
  Mock::VerifyAndClearExpectations(&client_);

  // Simulate finish loading of some iframe. Check that the prompt is shown.
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword);
  EXPECT_CALL(client_, IsSavingAndFillingEnabled).WillRepeatedly(Return(true));
  manager()->OnPasswordFormsRendered(&driver_, {} /* observed */);
}

TEST_F(PasswordManagerTest, FormSubmittedOnIFrame) {
  EXPECT_CALL(client_, IsSavingAndFillingEnabled).WillRepeatedly(Return(true));
  FormData form_data(MakeSimpleFormData());

  // Submit |form| on an iframe.
  MockPasswordManagerDriver iframe_driver;
  ON_CALL(iframe_driver, IsInPrimaryMainFrame()).WillByDefault(Return(false));
  ON_CALL(iframe_driver, GetId()).WillByDefault(Return(123));
  manager()->OnPasswordFormsParsed(&iframe_driver, {form_data});
  manager()->OnPasswordFormSubmitted(&iframe_driver, form_data);
  task_environment_.RunUntilIdle();

  // Simulate finish loading of another iframe.
  MockPasswordManagerDriver another_iframe_driver;
  EXPECT_CALL(another_iframe_driver, IsInPrimaryMainFrame())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(another_iframe_driver, GetId()).WillRepeatedly(Return(456));
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword).Times(0);
  manager()->OnPasswordFormsRendered(&another_iframe_driver, {} /* observed */);
  task_environment_.RunUntilIdle();
  Mock::VerifyAndClearExpectations(&client_);

  // Simulate finish loading of the submitted form iframe. Check that the prompt
  // is shown.
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword);
  EXPECT_CALL(client_, IsSavingAndFillingEnabled).WillRepeatedly(Return(true));
  manager()->OnPasswordFormsRendered(&iframe_driver, {} /* observed */);
  task_environment_.RunUntilIdle();
}

TEST_F(PasswordManagerTest, FormSubmittedOnIFramePrimaryMainFrameLoaded) {
  EXPECT_CALL(client_, IsSavingAndFillingEnabled).WillRepeatedly(Return(true));
  FormData form_data(MakeSimpleFormData());

  // Simulate a form submission on an iframe.
  MockPasswordManagerDriver iframe_driver;
  ON_CALL(iframe_driver, IsInPrimaryMainFrame()).WillByDefault(Return(false));
  ON_CALL(iframe_driver, GetId()).WillByDefault(Return(123));
  manager()->OnPasswordFormsParsed(&iframe_driver, {form_data});
  manager()->OnPasswordFormSubmitted(&iframe_driver, form_data);
  task_environment_.RunUntilIdle();

  // Simulate finish loading of the main frame. Check that the prompt is shown.
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword);
  manager()->OnPasswordFormsRendered(&driver_, {} /* observed */);
}

TEST_F(PasswordManagerTest, GenerationOnChangedForm) {
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(_))
      .WillRepeatedly(Return(true));

  // Create FormdData for a form with 1 password field and process it.
  FormData form_data;
  form_data.is_form_tag = false;
  form_data.url = GURL("http://www.testwebsite.com");

  FormFieldData old_password_field;
  old_password_field.form_control_type =
      autofill::FormControlType::kInputPassword;
  old_password_field.unique_renderer_id = FieldRendererId(0);
  old_password_field.name = u"oldpass";
  form_data.fields.push_back(old_password_field);

  manager()->OnPasswordFormsParsed(&driver_, {form_data});
  task_environment_.RunUntilIdle();

  // Form changes: new and confirmation password fields are added by the
  // website's scripts.
  FormFieldData new_password_field;
  new_password_field.form_control_type =
      autofill::FormControlType::kInputPassword;
  new_password_field.unique_renderer_id = FieldRendererId(1);
  new_password_field.name = u"newpass";
  form_data.fields.push_back(new_password_field);

  FormFieldData confirm_password_field;
  confirm_password_field.form_control_type =
      autofill::FormControlType::kInputPassword;
  confirm_password_field.unique_renderer_id = FieldRendererId(2);
  confirm_password_field.name = u"confpass";
  form_data.fields.push_back(confirm_password_field);

  // Server predictions may arrive before the form is parsed by PasswordManager.
  manager()->ProcessAutofillPredictions(
      &driver_, form_data,
      CreateServerPredictions(form_data,
                              {{1, ServerFieldType::ACCOUNT_CREATION_PASSWORD},
                               {2, ServerFieldType::CONFIRMATION_PASSWORD}}));

  autofill::PasswordFormGenerationData form_generation_data;
  EXPECT_CALL(driver_, FormEligibleForGenerationFound)
      .WillOnce(SaveArg<0>(&form_generation_data));
  // The change is discovered by PasswordManager.
  manager()->OnPasswordFormsParsed(&driver_, {form_data});
  task_environment_.RunUntilIdle();
  EXPECT_EQ(new_password_field.unique_renderer_id,
            form_generation_data.new_password_renderer_id);
}

TEST_F(PasswordManagerTest, SubmissionDetectedOnClearedForm) {
  base::test::ScopedFeatureList feature_list;
  EXPECT_CALL(client_, IsSavingAndFillingEnabled).WillRepeatedly(Return(true));
  PasswordForm saved_match(MakeSavedForm());
  store_->AddLogin(saved_match);

  // Create FormData for a form with 3 password fields and process it.
  FormData form_data;
  form_data.unique_renderer_id = FormRendererId(0);
  form_data.url = test_form_url_;

  FormFieldData old_password_field;
  old_password_field.form_control_type =
      autofill::FormControlType::kInputPassword;
  old_password_field.unique_renderer_id = FieldRendererId(1);
  old_password_field.name = u"oldpass";
  old_password_field.value = u"oldpass";
  form_data.fields.push_back(old_password_field);

  FormFieldData new_password_field;
  new_password_field.form_control_type =
      autofill::FormControlType::kInputPassword;
  new_password_field.unique_renderer_id = FieldRendererId(2);
  new_password_field.name = u"newpass";
  new_password_field.autocomplete_attribute = "new-password";
  form_data.fields.push_back(new_password_field);

  FormFieldData confirm_password_field;
  confirm_password_field.form_control_type =
      autofill::FormControlType::kInputPassword;
  confirm_password_field.unique_renderer_id = FieldRendererId(3);
  confirm_password_field.name = u"confpass";
  form_data.fields.push_back(confirm_password_field);

  manager()->OnPasswordFormsParsed(&driver_, {form_data});
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(manager()->form_managers().front());
  const VotesUploader& votes_uploader =
      manager()->form_managers().front()->votes_uploader();

  form_data.fields[0].value = u"oldpass";
  form_data.fields[1].value = u"newpass";
  form_data.fields[2].value = u"newpass";

  manager()->OnInformAboutUserInput(&driver_, form_data);

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword)
      .WillOnce(MoveArgAndReturn<0>(&form_manager_to_save, true));
  manager()->OnPasswordFormCleared(&driver_, form_data);

  // Check that suggested username was properly recorded in VotesUploader.
  EXPECT_EQ(saved_match.username_value, votes_uploader.suggested_username());
}

TEST_F(PasswordManagerTest,
       SubmissionDetectedOnClearedForm_OnlySavingFallback) {
  base::test::ScopedFeatureList feature_list;
  EXPECT_CALL(client_, IsSavingAndFillingEnabled).WillRepeatedly(Return(true));
  PasswordForm saved_match(MakeSavedForm());
  store_->AddLogin(saved_match);

  // Create FormData for a form with 1 password field and process it.
  FormData form_data;
  form_data.unique_renderer_id = FormRendererId(0);
  form_data.url = test_form_url_;

  FormFieldData password_field;
  password_field.form_control_type = autofill::FormControlType::kInputPassword;
  password_field.unique_renderer_id = FieldRendererId(1);
  password_field.name = u"one-time-code";
  password_field.value = u"123456";
  form_data.fields.push_back(password_field);

  manager()->OnPasswordFormsParsed(&driver_, {form_data});
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(manager()->form_managers().front());

  manager()->OnInformAboutUserInput(&driver_, form_data);

  // Don't expect an automatic prompt. Only the manual fallback for saving is
  // available.
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword).Times(0);
  manager()->OnPasswordFormCleared(&driver_, form_data);
}

// Similar test as above with fields that have empty names.
TEST_F(PasswordManagerTest, SubmissionDetectedOnClearedNamelessForm) {
  constexpr char16_t kEmptyName[] = u"";
  base::test::ScopedFeatureList feature_list;
  EXPECT_CALL(client_, IsSavingAndFillingEnabled).WillRepeatedly(Return(true));
  PasswordForm saved_match(MakeSavedForm());
  store_->AddLogin(saved_match);

  FormData form_data;
  form_data.unique_renderer_id = FormRendererId(0);
  form_data.url = test_form_url_;

  FormFieldData old_password_field;
  old_password_field.form_control_type =
      autofill::FormControlType::kInputPassword;
  old_password_field.unique_renderer_id = FieldRendererId(1);
  old_password_field.name = kEmptyName;
  old_password_field.value = u"oldpass";
  form_data.fields.push_back(old_password_field);

  FormFieldData new_password_field;
  new_password_field.form_control_type =
      autofill::FormControlType::kInputPassword;
  new_password_field.unique_renderer_id = FieldRendererId(2);
  new_password_field.name = kEmptyName;
  new_password_field.autocomplete_attribute = "new-password";
  form_data.fields.push_back(new_password_field);

  manager()->OnPasswordFormsParsed(&driver_, {form_data});
  task_environment_.RunUntilIdle();

  form_data.fields[0].value = u"oldpass";
  form_data.fields[1].value = u"newpass";

  manager()->OnInformAboutUserInput(&driver_, form_data);

  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword)
      .WillOnce(MoveArgAndReturn<0>(&form_manager_to_save, true));
  manager()->OnPasswordFormCleared(&driver_, form_data);
}

TEST_F(PasswordManagerTest, SubmissionDetectedOnClearedFormlessFields) {
  base::test::ScopedFeatureList feature_list;
  EXPECT_CALL(client_, IsSavingAndFillingEnabled).WillRepeatedly(Return(true));
  PasswordForm saved_match(MakeSavedForm());
  store_->AddLogin(saved_match);

  for (bool new_password_field_was_cleared : {true, false}) {
    SCOPED_TRACE(testing::Message("#new password field was cleared = ")
                 << new_password_field_was_cleared);

    // Create FormData for a form with 1 password field and process it.
    FormData form_data;
    form_data.is_form_tag = false;
    form_data.unique_renderer_id = FormRendererId(0);
    form_data.url = test_form_url_;

    FormFieldData old_password_field;
    old_password_field.form_control_type =
        autofill::FormControlType::kInputPassword;
    old_password_field.unique_renderer_id = FieldRendererId(1);
    old_password_field.name = u"oldpass";
    old_password_field.value = u"oldpass";
    form_data.fields.push_back(old_password_field);

    FormFieldData new_password_field;
    new_password_field.form_control_type =
        autofill::FormControlType::kInputPassword;
    new_password_field.unique_renderer_id = FieldRendererId(2);
    new_password_field.name = u"newpass";
    new_password_field.autocomplete_attribute = "new-password";
    form_data.fields.push_back(new_password_field);

    FormFieldData confirm_password_field;
    confirm_password_field.form_control_type =
        autofill::FormControlType::kInputPassword;
    confirm_password_field.unique_renderer_id = FieldRendererId(3);
    confirm_password_field.name = u"confpass";
    form_data.fields.push_back(confirm_password_field);

    manager()->OnPasswordFormsParsed(&driver_, {form_data});
    task_environment_.RunUntilIdle();

    form_data.fields[0].value = u"oldpass";
    form_data.fields[1].value = u"newpass";
    form_data.fields[2].value = u"newpass";

    manager()->OnInformAboutUserInput(&driver_, form_data);

    form_data.fields[0].value = std::u16string();
    form_data.fields[2].value = std::u16string();
    if (new_password_field_was_cleared)
      form_data.fields[1].value = std::u16string();

    std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
    if (new_password_field_was_cleared) {
      EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword)
          .WillOnce(MoveArgAndReturn<0>(&form_manager_to_save, true));
    } else {
      EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword).Times(0);
    }

    manager()->OnPasswordFormCleared(&driver_, form_data);
  }
}

// Similar test as above with fields that have empty names.
TEST_F(PasswordManagerTest, SubmissionDetectedOnClearedNameAndFormlessFields) {
  constexpr char16_t kEmptyName[] = u"";
  base::test::ScopedFeatureList feature_list;
  EXPECT_CALL(client_, IsSavingAndFillingEnabled).WillRepeatedly(Return(true));
  PasswordForm saved_match(MakeSavedForm());
  store_->AddLogin(saved_match);

  for (bool new_password_field_was_cleared : {true, false}) {
    SCOPED_TRACE(testing::Message("#new password field was cleared = ")
                 << new_password_field_was_cleared);

    // Create FormData for a form with 1 password field and process it.
    FormData form_data;
    form_data.is_form_tag = false;
    form_data.unique_renderer_id = FormRendererId(0);
    form_data.url = test_form_url_;

    FormFieldData old_password_field;
    old_password_field.form_control_type =
        autofill::FormControlType::kInputPassword;
    old_password_field.unique_renderer_id = FieldRendererId(1);
    old_password_field.name = kEmptyName;
    old_password_field.value = u"oldpass";
    form_data.fields.push_back(old_password_field);

    FormFieldData new_password_field;
    new_password_field.form_control_type =
        autofill::FormControlType::kInputPassword;
    new_password_field.unique_renderer_id = FieldRendererId(2);
    new_password_field.name = kEmptyName;
    new_password_field.autocomplete_attribute = "new-password";
    form_data.fields.push_back(new_password_field);

    manager()->OnPasswordFormsParsed(&driver_, {form_data});
    task_environment_.RunUntilIdle();

    form_data.fields[0].value = u"oldpass";
    form_data.fields[1].value = u"newpass";

    manager()->OnInformAboutUserInput(&driver_, form_data);

    form_data.fields[0].value.clear();
    if (new_password_field_was_cleared)
      form_data.fields[1].value.clear();

    std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
    if (new_password_field_was_cleared) {
      EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword)
          .WillOnce(MoveArgAndReturn<0>(&form_manager_to_save, true));
    } else {
      EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword).Times(0);
    }

    manager()->OnPasswordFormCleared(&driver_, form_data);
  }
}

TEST_F(PasswordManagerTest, IsFormManagerPendingPasswordUpdate) {
  PasswordForm form(MakeSimpleForm());
  store_->AddLogin(form);

  std::vector<FormData> observed = {form.form_data};
  EXPECT_CALL(client_, IsSavingAndFillingEnabled).WillRepeatedly(Return(true));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();

  // Password was not updated yet.
  EXPECT_FALSE(manager()->IsFormManagerPendingPasswordUpdate());

  // The user updates the password.
  FormData updated_data(form.form_data);
  updated_data.fields[1].value = u"new_password";
  manager()->OnInformAboutUserInput(&driver_, updated_data);
  EXPECT_TRUE(manager()->IsFormManagerPendingPasswordUpdate());

  // The user submits the form.
  OnPasswordFormSubmitted(updated_data);
  EXPECT_TRUE(manager()->GetSubmittedManagerForTest());
  // OnFormManagerPendingPasswordUpdate() still returns true after submission.
  EXPECT_TRUE(manager()->IsFormManagerPendingPasswordUpdate());
}

// Test submission of "PasswordManager.FormVisited.PerProfileType" for
// Incognito mode.
TEST_F(PasswordManagerTest, IncognitoProfileTypeMetricSubmission) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(client_, GetProfileType())
      .WillRepeatedly(Return(profile_metrics::BrowserProfileType::kIncognito));

  std::vector<FormData> observed;
  PasswordForm form(MakeFormWithOnlyNewPasswordField());
  observed.push_back(form.form_data);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);

  // Test if visit is properly recorded and submission is not marked.
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.FormVisited.PerProfileType",
      profile_metrics::BrowserProfileType::kIncognito, 1);

  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.url))
      .WillRepeatedly(Return(true));
  OnPasswordFormSubmitted(form.form_data);

  // And nothing in other buckets.
  histogram_tester.ExpectTotalCount(
      "PasswordManager.FormVisited.PerProfileType", 1);
}

// Test submission of "PasswordManager.FormVisited.PerProfileType" for Guest
// mode.
TEST_F(PasswordManagerTest, GuestProfileTypeMetricSubmission) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(client_, GetProfileType())
      .WillRepeatedly(Return(profile_metrics::BrowserProfileType::kGuest));

  std::vector<FormData> observed;
  PasswordForm form(MakeFormWithOnlyNewPasswordField());
  observed.push_back(form.form_data);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);

  // Test if visit is properly recorded and submission is not marked.
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.FormVisited.PerProfileType",
      profile_metrics::BrowserProfileType::kGuest, 1);

  EXPECT_CALL(client_, IsSavingAndFillingEnabled(form.url))
      .WillRepeatedly(Return(true));
  OnPasswordFormSubmitted(form.form_data);

  // And nothing in other buckets.
  histogram_tester.ExpectTotalCount(
      "PasswordManager.FormVisited.PerProfileType", 1);
}

// Tests that the login is not detected twice.
TEST_F(PasswordManagerTest, SubmittedManagerClearingOnSuccessfulLogin) {
  PasswordForm saved_match(MakeSavedForm());
  store_->AddLogin(saved_match);

  PasswordForm observed_form(MakeSimpleForm());
  std::vector<FormData> observed_forms = {observed_form.form_data};
  manager()->OnPasswordFormsParsed(&driver_, observed_forms);
  manager()->OnPasswordFormsRendered(&driver_, observed_forms);
  task_environment_.RunUntilIdle();
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(observed_form.url))
      .WillRepeatedly(Return(true));
  OnPasswordFormSubmitted(observed_form.form_data);

  // Simulate observing no forms, which leads to a successful login detection.
  EXPECT_TRUE(manager()->GetSubmittedManagerForTest());
  observed_forms.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed_forms);
  manager()->OnPasswordFormsRendered(&driver_, observed_forms);
  task_environment_.RunUntilIdle();

  // There should be no submitted manager after the login detection.
  EXPECT_FALSE(manager()->GetSubmittedManagerForTest());
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(PasswordManagerTest, FormSubmissionTrackingAfterTouchToFill) {
  PasswordForm saved_match(MakeSavedForm());
  store_->AddLogin(saved_match);

  // A navigation without a submitted manager clears the state of submission
  // tracking.
  EXPECT_CALL(client_, ResetSubmissionTrackingAfterTouchToFill());

  PasswordForm observed_form(MakeSimpleForm());
  std::vector<FormData> observed_forms = {observed_form.form_data};
  manager()->OnPasswordFormsParsed(&driver_, observed_forms);
  manager()->OnPasswordFormsRendered(&driver_, observed_forms);
  task_environment_.RunUntilIdle();
  EXPECT_CALL(client_, IsSavingAndFillingEnabled(observed_form.url))
      .WillRepeatedly(Return(true));
  OnPasswordFormSubmitted(observed_form.form_data);

  // No forms means a successful login, which reports a metric and stops
  // tracking.
  EXPECT_CALL(client_, NotifyOnSuccessfulLogin(_));
  EXPECT_CALL(client_, ResetSubmissionTrackingAfterTouchToFill());

  observed_forms.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed_forms);
  manager()->OnPasswordFormsRendered(&driver_, observed_forms);
}
#endif  // BUILDFLAG(IS_ANDROID)

// Check that on successful login the credentials are checked for leak depending
// on mute state of insecure credential.
TEST_F(PasswordManagerTest, DontStartLeakDetectionWhenMuted) {
  auto mock_factory =
      std::make_unique<testing::StrictMock<MockLeakDetectionCheckFactory>>();
  manager()->set_leak_factory(std::move(mock_factory));

  PasswordForm form = MakeSimpleForm();
  form.password_issues.insert(
      {InsecureType::kLeaked,
       InsecurityMetadata(base::Time(), IsMuted(true),
                          TriggerBackendNotification(false))});
  store_->AddLogin(form);
  std::vector<FormData> observed = {form.form_data};
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();

  EXPECT_CALL(client_, IsSavingAndFillingEnabled).WillRepeatedly(Return(true));
  OnPasswordFormSubmitted(form.form_data);

  auto check_instance = std::make_unique<MockLeakDetectionCheck>();
  EXPECT_CALL(*check_instance, Start).Times(0);

  // Now the password manager waits for the navigation to complete.
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();
}

// Tests that check for leaks happens even if there are muted credentials for
// the same domain, but with different username.
TEST_F(PasswordManagerTest, StartLeakCheckWhenForUsernameNotMuted) {
  auto mock_factory =
      std::make_unique<testing::StrictMock<MockLeakDetectionCheckFactory>>();
  MockLeakDetectionCheckFactory* weak_factory = mock_factory.get();
  manager()->set_leak_factory(std::move(mock_factory));

  PasswordForm form = MakeSimpleForm();
  std::vector<FormData> observed = {form.form_data};

  form.username_value = u"different_username";
  form.password_issues.insert(
      {InsecureType::kLeaked,
       InsecurityMetadata(base::Time(), IsMuted(true),
                          TriggerBackendNotification(false))});

  store_->AddLogin(form);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();

  EXPECT_CALL(client_, IsSavingAndFillingEnabled).WillRepeatedly(Return(true));
  OnPasswordFormSubmitted(form.form_data);

  auto check_instance = std::make_unique<MockLeakDetectionCheck>();
  EXPECT_CALL(*check_instance, Start);
  EXPECT_CALL(*weak_factory, TryCreateLeakCheck)
      .WillOnce(Return(ByMove(std::move(check_instance))));

  // Now the password manager waits for the navigation to complete.
  observed.clear();
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();
}

// Check that a leak warning is not shown for a single username submission.
TEST_F(PasswordManagerTest, DontStartLeakDetectionForSingleUsernameSubmission) {
  auto mock_factory =
      std::make_unique<testing::StrictMock<MockLeakDetectionCheckFactory>>();
  MockLeakDetectionCheckFactory* weak_factory = mock_factory.get();
  manager()->set_leak_factory(std::move(mock_factory));
  EXPECT_CALL(client_, IsSavingAndFillingEnabled).WillRepeatedly(Return(true));

  // User sees a single username field. The password field is hidden.
  PasswordForm username_form = MakeSimpleForm();
  username_form.form_data.fields[1].is_focusable = false;
  std::vector<FormData> observed = {username_form.form_data};
  EXPECT_CALL(*weak_factory, TryCreateLeakCheck).Times(0);
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();

  // User submits the single username. No leak warning expected.
  OnPasswordFormSubmitted(username_form.form_data);

  // User sees a single password field. The username field is hidden.
  PasswordForm password_form = MakeSimpleForm();
  password_form.form_data.fields[0].is_focusable = false;
  observed = {password_form.form_data};
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
  task_environment_.RunUntilIdle();

  // User submits the single password field.
  OnPasswordFormSubmitted(password_form.form_data);
  // Wait until the password form manager created above receives the stored
  // logins.
  task_environment_.RunUntilIdle();

  // As no forms are seen, login has completed. Now expect a leak warning.
  observed.clear();
  auto check_instance = std::make_unique<MockLeakDetectionCheck>();
  EXPECT_CALL(*check_instance, Start);
  EXPECT_CALL(*weak_factory, TryCreateLeakCheck)
      .WillOnce(Return(ByMove(std::move(check_instance))));
  manager()->OnPasswordFormsParsed(&driver_, observed);
  manager()->OnPasswordFormsRendered(&driver_, observed);
}

TEST_F(PasswordManagerTest, ParsingNewFormsTriggersSettingFetch) {
  // Check that seeing the form for the first time triggers fetching settings.
  std::vector<FormData> observed;
  observed.emplace_back(MakeSignUpFormData());
  EXPECT_CALL(client_, RefreshPasswordManagerSettingsIfNeeded);
  manager()->OnPasswordFormsParsed(&driver_, observed);

  // Check that settings are not refetched if the already seen form dynamically
  // changes and is parsed again.
  FormFieldData new_field;
  observed[0].fields.push_back(new_field);
  EXPECT_CALL(client_, RefreshPasswordManagerSettingsIfNeeded).Times(0);
  manager()->OnPasswordFormsParsed(&driver_, observed);
}

TEST_F(PasswordManagerTest, HaveFormManagersReceivedDataDependsOnDriver) {
  FormData observed_form_data = MakeSimpleFormData();
  // GetLogins calls remain unanswered to emulate that |PasswordStore| did not
  // fetch logins yet for this form.
  manager()->OnPasswordFormsParsed(&driver_, {observed_form_data});
  EXPECT_FALSE(manager()->HaveFormManagersReceivedData(&driver_));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(manager()->HaveFormManagersReceivedData(&driver_));

  FormData observed_form_data_other_frame = MakeSimpleFormData();
  MockPasswordManagerDriver other_driver;
  manager()->OnPasswordFormsParsed(&other_driver,
                                   {observed_form_data_other_frame});

  EXPECT_TRUE(manager()->HaveFormManagersReceivedData(&driver_));
  EXPECT_FALSE(manager()->HaveFormManagersReceivedData(&other_driver));
}

TEST_F(PasswordManagerTest,
       FieldInfoManagerHasDataPredictionsInitiallyAvailable) {
  feature_list_.InitAndEnableFeature(
      password_manager::features::kForgotPasswordFormSupport);

  // Process server predictions.
  PasswordForm username_form(MakeSimpleFormWithOnlyUsernameField());
  const ServerFieldType kFieldType =
      ServerFieldType::SINGLE_USERNAME_FORGOT_PASSWORD;
  manager()->ProcessAutofillPredictions(
      &driver_, username_form.form_data,
      CreateServerPredictions(username_form.form_data, {{0, kFieldType}}));

  // Simulate the user typed in a text field..
  ON_CALL(driver_, GetLastCommittedURL())
      .WillByDefault(ReturnRef(username_form.url));
  std::u16string potential_username_value = u"is_that_a_username?";
  manager()->OnUserModifiedNonPasswordField(
      &driver_, username_form.form_data.fields[0].unique_renderer_id,
      /*value=*/potential_username_value,
      /*autocomplete_attribute_has_username=*/false, /*is_likely_otp=*/false);

  std::string signon_realm = GetSignonRealm(username_form.url);
  ASSERT_EQ(field_info_manager_->GetFieldInfo(signon_realm).size(), 1u);

  FieldInfo info = field_info_manager_->GetFieldInfo(signon_realm)[0];
  EXPECT_EQ(info.driver_id, driver_.GetId());
  EXPECT_EQ(info.field_id,
            username_form.form_data.fields[0].unique_renderer_id);
  EXPECT_EQ(info.signon_realm, signon_realm);
  EXPECT_EQ(info.value, potential_username_value);
  EXPECT_EQ(info.type, kFieldType);
}

TEST_F(PasswordManagerTest, FieldInfoManagerHasDataPredictionsPropagatedLater) {
  feature_list_.InitAndEnableFeature(
      password_manager::features::kForgotPasswordFormSupport);

  // Simulate the user typed in a text field.
  PasswordForm username_form(MakeSimpleFormWithOnlyUsernameField());
  ON_CALL(driver_, GetLastCommittedURL())
      .WillByDefault(ReturnRef(username_form.url));
  std::u16string potential_username_value = u"is_that_a_username?";
  manager()->OnUserModifiedNonPasswordField(
      &driver_, username_form.form_data.fields[0].unique_renderer_id,
      /*value=*/potential_username_value,
      /*autocomplete_attribute_has_username=*/false, /*is_likely_otp=*/false);

  // Process server predictions.
  const ServerFieldType kFieldType =
      ServerFieldType::SINGLE_USERNAME_FORGOT_PASSWORD;
  manager()->ProcessAutofillPredictions(
      &driver_, username_form.form_data,
      CreateServerPredictions(username_form.form_data, {{0, kFieldType}}));

  std::string signon_realm = GetSignonRealm(username_form.url);
  ASSERT_EQ(field_info_manager_->GetFieldInfo(signon_realm).size(), 1u);
  FieldInfo info = field_info_manager_->GetFieldInfo(signon_realm)[0];
  EXPECT_EQ(info.driver_id, driver_.GetId());
  EXPECT_EQ(info.field_id,
            username_form.form_data.fields[0].unique_renderer_id);
  EXPECT_EQ(info.signon_realm, signon_realm);
  EXPECT_EQ(info.value, potential_username_value);
  EXPECT_EQ(info.type, kFieldType);
}

// Check that a UKM for the difference on form parsing during filling and
// saving is recorded.
TEST_F(PasswordManagerTest, ParsingDifferenceFillingAndSavingUKM) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  EXPECT_CALL(client_, IsSavingAndFillingEnabled).WillRepeatedly(Return(true));
  FormData observed_form = MakeSimpleFormData();
  manager()->OnPasswordFormsParsed(&driver_, {observed_form});
  manager()->OnPasswordFormsRendered(&driver_, {observed_form});
  task_environment_.RunUntilIdle();

  // Simulate successfully submitting the form.
  manager()->OnPasswordFormSubmitted(&driver_, observed_form);
  manager()->OnPasswordFormsRendered(&driver_, {});

  CheckMetricHasValue(
      test_ukm_recorder, ukm::builders::PasswordForm::kEntryName,
      ukm::builders::PasswordForm::kParsingDiffFillingAndSavingName,
      PasswordFormMetricsRecorder::ParsingDifference::kNone);
}

enum class PredictionSource {
  ID_ATTRIBUTE = 0,
  NAME_ATTRIBUTE = 1,
  AUTOCOMPLETE = 2,
  SERVER = 3
};

class PasswordManagerWithOtpVariationsTest
    : public PasswordManagerTest,
      public testing::WithParamInterface<std::tuple<
          /*saved_form.username=*/std::u16string,
          /*saved_form.password=*/absl::optional<std::u16string>,
          /*another_saved_form.password_value=*/absl::optional<std::u16string>,
          /*one_time_code_form.username_value=*/std::u16string,
          PredictionSource>> {
 protected:
  const std::u16string test_form_password_element_ = u"one-time-code";
  const std::u16string test_form_otp_value_ = u"123456";
  const std::u16string test_form_username_element_ = u"username";
};

// Tests that only filling and saving fallbacks are available for a field
// classified as an OTP field. The test is similar to non-parametrized
// FillingAndSavingFallbacksOnOtpForm*, but tries many different circumstances.
TEST_P(PasswordManagerWithOtpVariationsTest,
       FillingAndSavingFallbacksOnOtpForm) {
  auto [saved_form_username, saved_form_password, another_saved_form_password,
        one_time_code_form_username_value, prediction_type] = GetParam();
  PasswordFormManager::set_wait_for_server_predictions_for_filling(
      prediction_type == PredictionSource::SERVER);

  EXPECT_CALL(client_, IsSavingAndFillingEnabled(_))
      .WillRepeatedly(Return(true));
  absl::optional<PasswordForm> saved_form;
  absl::optional<PasswordForm> another_saved_form;
  // No saved password means no saved credential.
  if (saved_form_password.has_value()) {
    saved_form = PasswordForm();
    saved_form.value().url = test_form_url_;
    saved_form.value().signon_realm = test_signon_realm_;
    saved_form.value().username_value = saved_form_username;
    saved_form.value().password_value = saved_form_password.value();
    store_->AddLogin(saved_form.value());

    // To avoid complex logic in tests (go/unit-testing-practices#logic),
    // only add the second credential if the first is added.
    if (another_saved_form_password.has_value()) {
      another_saved_form = PasswordForm();
      another_saved_form.value().url = test_form_url_;
      another_saved_form.value().signon_realm = test_signon_realm_;
      another_saved_form.value().username_value = u"another_username";
      another_saved_form.value().password_value =
          another_saved_form_password.value();
      store_->AddLogin(another_saved_form.value());
    } else {
      another_saved_form = absl::nullopt;
    }
  }

  PasswordForm one_time_code_form;
  one_time_code_form.url = test_form_url_;
  one_time_code_form.signon_realm = test_signon_realm_;
  one_time_code_form.form_data.url = test_form_url_;
  one_time_code_form.form_data.unique_renderer_id = FormRendererId(10);

  const bool otp_form_has_username = one_time_code_form_username_value != u"";
  if (otp_form_has_username) {
    one_time_code_form.username_value = one_time_code_form_username_value;
    one_time_code_form.username_element = test_form_username_element_;
    FormFieldData username_field;
    username_field.name = test_form_username_element_;
    username_field.value = one_time_code_form_username_value;
    username_field.form_control_type = autofill::FormControlType::kInputText;
    username_field.unique_renderer_id = FieldRendererId(1);
    one_time_code_form.form_data.fields.push_back(username_field);
  }

  FormFieldData otp_field;
  otp_field.value = test_form_otp_value_;
  otp_field.form_control_type = autofill::FormControlType::kInputPassword;
  otp_field.unique_renderer_id = FieldRendererId(2);
  one_time_code_form.form_data.fields.push_back(otp_field);
  switch (prediction_type) {
    case PredictionSource::ID_ATTRIBUTE:
      one_time_code_form.form_data.fields[otp_form_has_username].id_attribute =
          test_form_password_element_;
      break;
    case PredictionSource::NAME_ATTRIBUTE:
      one_time_code_form.form_data.fields[otp_form_has_username]
          .name_attribute = test_form_password_element_;
      break;
    case PredictionSource::AUTOCOMPLETE:
      one_time_code_form.form_data.fields[otp_form_has_username]
          .autocomplete_attribute =
          base::UTF16ToUTF8(test_form_password_element_);
      break;
    case PredictionSource::SERVER:
      manager()->ProcessAutofillPredictions(
          &driver_, one_time_code_form.form_data,
          CreateServerPredictions(one_time_code_form.form_data,
                                  {{otp_form_has_username ? 1 : 0,
                                    ServerFieldType::NOT_PASSWORD}}));
      break;
  }

  PasswordFormFillData form_data;
  EXPECT_CALL(driver_, SetPasswordFillData)
      .WillRepeatedly(SaveArg<0>(&form_data));

  manager()->OnPasswordFormsParsed(&driver_, {one_time_code_form.form_data});
  task_environment_.RunUntilIdle();

  // Check that manual filling fallback available.
  if (another_saved_form.has_value()) {
    // Two credentials are present, one of them is picked.
    if (saved_form.value().username_value ==
        form_data.preferred_login.username_value) {
      EXPECT_TRUE(saved_form.value().password_value ==
                  form_data.preferred_login.password_value);
    } else if (another_saved_form.value().username_value ==
               form_data.preferred_login.username_value) {
      EXPECT_TRUE(another_saved_form.value().password_value ==
                  form_data.preferred_login.password_value);
    } else {
      ADD_FAILURE() << "No manual filling fallback available";
    }
  } else if (saved_form.has_value()) {
    EXPECT_EQ(saved_form.value().username_value,
              form_data.preferred_login.username_value);
    EXPECT_EQ(saved_form.value().password_value,
              form_data.preferred_login.password_value);
  } else {
    EXPECT_EQ(form_data.preferred_login.username_value, u"");
    EXPECT_EQ(form_data.preferred_login.password_value, u"");
  }
  // Check that no automatic filling available.
  EXPECT_TRUE(form_data.username_element_renderer_id.is_null());
  EXPECT_TRUE(form_data.password_element_renderer_id.is_null());
  // Check that saving fallback is available.
  std::unique_ptr<PasswordFormManagerForUI> form_manager_to_save;
  EXPECT_CALL(client_, ShowManualFallbackForSaving)
      .WillOnce(MoveArg<0>(&form_manager_to_save));
  manager()->OnInformAboutUserInput(&driver_, one_time_code_form.form_data);
  ASSERT_TRUE(form_manager_to_save);

  PasswordForm expected_pending_form;
  if (otp_form_has_username
          ? (saved_form.has_value() &&
             one_time_code_form_username_value == saved_form_username)
          : saved_form.has_value()) {
    // If there is no username or the username matches existing credential,
    // password manager tries to update the existing credential. If a user
    // clicks a manual fallback for updating, update password bubble is shown.
    expected_pending_form = saved_form.value();
  } else {
    // No matching credential in password manager. If a user clicks a manual
    // fallback for saving, save password bubble is shown.
    expected_pending_form = one_time_code_form;

    // Set values that is expected from parsing |form_data|.
    expected_pending_form.password_element_renderer_id = FieldRendererId(2);
    expected_pending_form.password_value = test_form_otp_value_;
    if (otp_form_has_username) {
      expected_pending_form.username_value = one_time_code_form_username_value;
      expected_pending_form.username_element = test_form_username_element_;
    }
  }
  // Just in case a user uses the manual fallback for saving, the otp value will
  // be saved as password.
  expected_pending_form.password_value = test_form_otp_value_;
  expected_pending_form.only_for_fallback = true;
  EXPECT_THAT(form_manager_to_save->GetPendingCredentials(),
              FormMatches(expected_pending_form));

  // Check that neither save or update prompt is shown automatically.
  EXPECT_CALL(client_, PromptUserToSaveOrUpdatePassword).Times(0);
  OnPasswordFormSubmitted(one_time_code_form.form_data);
  manager()->DidNavigateMainFrame(true);
  manager()->OnPasswordFormsRendered(&driver_, {});
  task_environment_.RunUntilIdle();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    PasswordManagerWithOtpVariationsTest,
    testing::Combine(testing::Values(u"", u"username"),
                     testing::Values(u"password", absl::nullopt),
                     testing::Values(u"another_password", absl::nullopt),
                     testing::Values(u"", u"username", u"+1 650 000 000"),
                     testing::Values(PredictionSource::ID_ATTRIBUTE,
                                     PredictionSource::NAME_ATTRIBUTE,
                                     PredictionSource::AUTOCOMPLETE,
                                     PredictionSource::SERVER)));

}  // namespace password_manager
