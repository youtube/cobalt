// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/browser_autofill_manager.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/hash/hash.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/metrics_hashes.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/autofill/core/browser/autocomplete_history_manager.h"
#include "components/autofill/core/browser/autofill_download_manager.h"
#include "components/autofill/core/browser/autofill_form_test_utils.h"
#include "components/autofill/core/browser/autofill_suggestion_generator.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/geo/alternative_state_name_map_test_utils.h"
#include "components/autofill/core/browser/metrics/form_events/form_events.h"
#include "components/autofill/core/browser/metrics/log_event.h"
#include "components/autofill/core/browser/mock_autocomplete_history_manager.h"
#include "components/autofill/core/browser/mock_autofill_optimization_guide.h"
#include "components/autofill/core/browser/mock_iban_manager.h"
#include "components/autofill/core/browser/mock_merchant_promo_code_manager.h"
#include "components/autofill/core/browser/mock_single_field_form_fill_router.h"
#include "components/autofill/core/browser/payments/test_credit_card_save_manager.h"
#include "components/autofill/core/browser/payments/test_payments_client.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/strike_databases/payments/test_credit_card_save_strike_database.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_autofill_external_delegate.h"
#include "components/autofill/core/browser/test_autofill_manager_waiter.h"
#include "components/autofill/core/browser/test_autofill_tick_clock.h"
#include "components/autofill/core/browser/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/test_form_data_importer.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/test_utils/vote_uploads_test_matchers.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autocomplete_parsing_util.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/autofill_tick_clock.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/signatures.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/prefs/pref_service.h"
#include "components/security_interstitials/core/pref_names.h"
#include "components/security_state/core/security_state.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/test/test_sync_service.h"
#include "components/translate/core/common/language_detection_details.h"
#include "components/variations/variations_associated_data.h"
#include "components/version_info/channel.h"
#include "net/base/url_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"
#include "url/url_canon.h"

using base::UTF8ToUTF16;
using testing::_;
using testing::AllOf;
using testing::AnyOf;
using testing::AtLeast;
using testing::Contains;
using testing::DoAll;
using testing::Each;
using testing::ElementsAre;
using testing::Field;
using testing::HasSubstr;
using testing::Matcher;
using testing::NiceMock;
using testing::Not;
using testing::Return;
using testing::SaveArg;
using testing::UnorderedElementsAre;
using testing::VariantWith;

namespace autofill {

using mojom::SubmissionIndicatorEvent;
using mojom::SubmissionSource;

namespace {

const std::string kArbitraryNickname = "Grocery Card";
const std::u16string kArbitraryNickname16 = u"Grocery Card";
const std::string kAddressEntryIcon = "accountIcon";

class MockAutofillClient : public TestAutofillClient {
 public:
  MockAutofillClient() {
    ON_CALL(*this, GetChannel())
        .WillByDefault(Return(version_info::Channel::UNKNOWN));
    ON_CALL(*this, IsPasswordManagerEnabled()).WillByDefault(Return(true));
  }
  MockAutofillClient(const MockAutofillClient&) = delete;
  MockAutofillClient& operator=(const MockAutofillClient&) = delete;
  ~MockAutofillClient() override = default;

  MOCK_METHOD(version_info::Channel, GetChannel, (), (const override));
  MOCK_METHOD(AutofillOptimizationGuide*,
              GetAutofillOptimizationGuide,
              (),
              (const override));
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  MOCK_METHOD(void,
              ConfirmSaveUpiIdLocally,
              (const std::string& upi_id,
               base::OnceCallback<void(bool user_decision)> callback),
              (override));
#endif
  MOCK_METHOD(profile_metrics::BrowserProfileType,
              GetProfileType,
              (),
              (const override));
  MOCK_METHOD(void, HideAutofillPopup, (PopupHidingReason reason), (override));
  MOCK_METHOD(bool, IsPasswordManagerEnabled, (), (override));
  MOCK_METHOD(void,
              DidFillOrPreviewForm,
              (mojom::RendererFormDataAction action,
               AutofillTriggerSource trigger_source,
               bool is_refill),
              (override));
};

class MockAutofillDownloadManager : public AutofillDownloadManager {
 public:
  explicit MockAutofillDownloadManager(AutofillClient* client)
      : AutofillDownloadManager(client,
                                /*api_key=*/"",
                                /*is_raw_metadata_uploading_enabled=*/false,
                                /*log_manager=*/nullptr) {}

  MockAutofillDownloadManager(const MockAutofillDownloadManager&) = delete;
  MockAutofillDownloadManager& operator=(const MockAutofillDownloadManager&) =
      delete;

  MOCK_METHOD(bool,
              StartUploadRequest,
              (const FormStructure&,
               bool,
               const ServerFieldTypeSet&,
               const std::string&,
               bool,
               PrefService*,
               base::WeakPtr<Observer>),
              (override));

  bool StartQueryRequest(const std::vector<FormStructure*>& forms,
                         net::IsolationInfo isolation_info,
                         base::WeakPtr<Observer> observer) override {
    last_queried_forms_ = forms;
    return true;
  }

  // Verify that the last queried forms equal |expected_forms|.
  void VerifyLastQueriedForms(const std::vector<FormData>& expected_forms) {
    ASSERT_EQ(expected_forms.size(), last_queried_forms_.size());
    for (size_t i = 0; i < expected_forms.size(); ++i) {
      EXPECT_EQ(last_queried_forms_[i]->global_id().renderer_id,
                expected_forms[i].global_id().renderer_id);
    }
  }

 private:
  std::vector<FormStructure*> last_queried_forms_;
};

class MockTouchToFillDelegate : public TouchToFillDelegate {
 public:
  MockTouchToFillDelegate() = default;
  MockTouchToFillDelegate(const MockTouchToFillDelegate&) = delete;
  MockTouchToFillDelegate& operator=(const MockTouchToFillDelegate&) = delete;
  ~MockTouchToFillDelegate() override = default;
  MOCK_METHOD(BrowserAutofillManager*, GetManager, (), (override));
  MOCK_METHOD(bool,
              IntendsToShowTouchToFill,
              (FormGlobalId, FieldGlobalId),
              (override));
  MOCK_METHOD(bool,
              TryToShowTouchToFill,
              (const FormData&, const FormFieldData&),
              (override));
  MOCK_METHOD(bool, IsShowingTouchToFill, (), (override));
  MOCK_METHOD(void, HideTouchToFill, (), (override));
  MOCK_METHOD(void, Reset, (), (override));
  MOCK_METHOD(bool, ShouldShowScanCreditCard, (), (override));
  MOCK_METHOD(void, ScanCreditCard, (), (override));
  MOCK_METHOD(void, OnCreditCardScanned, (const CreditCard& card), (override));
  MOCK_METHOD(void, ShowCreditCardSettings, (), (override));
  MOCK_METHOD(void,
              SuggestionSelected,
              (std::string unique_id, bool is_virtual),
              (override));
  MOCK_METHOD(void, OnDismissed, (bool dismissed_by_user), (override));
  MOCK_METHOD(void,
              LogMetricsAfterSubmission,
              (const FormStructure&),
              (override));
};

class MockFastCheckoutDelegate : public FastCheckoutDelegate {
 public:
  MockFastCheckoutDelegate() = default;
  MockFastCheckoutDelegate(const MockFastCheckoutDelegate&) = delete;
  MockFastCheckoutDelegate& operator=(const MockFastCheckoutDelegate&) = delete;
  ~MockFastCheckoutDelegate() override = default;

  MOCK_METHOD(bool,
              TryToShowFastCheckout,
              (const FormData&,
               const FormFieldData&,
               base::WeakPtr<AutofillManager>),
              (override));
  MOCK_METHOD(bool,
              IntendsToShowFastCheckout,
              (AutofillManager&, FormGlobalId, FieldGlobalId),
              (const, override));
  MOCK_METHOD(bool, IsShowingFastCheckoutUI, (), (const, override));
  MOCK_METHOD(void, HideFastCheckout, (bool), (override));
};

void ExpectFilledField(const char* expected_label,
                       const char* expected_name,
                       const char* expected_value,
                       const char* expected_form_control_type,
                       const FormFieldData& field) {
  SCOPED_TRACE(expected_label);
  EXPECT_EQ(UTF8ToUTF16(expected_label), field.label);
  EXPECT_EQ(UTF8ToUTF16(expected_name), field.name);
  EXPECT_EQ(UTF8ToUTF16(expected_value), field.value);
  EXPECT_EQ(expected_form_control_type, field.form_control_type);
}

// Verifies that the |filled_form| has been filled with the given data.
// Verifies address fields if |has_address_fields| is true, and verifies
// credit card fields if |has_credit_card_fields| is true. Verifies both if both
// are true. |use_month_type| is used for credit card input month type.
void ExpectFilledForm(const FormData& filled_form,
                      const char* first,
                      const char* middle,
                      const char* last,
                      const char* address1,
                      const char* address2,
                      const char* city,
                      const char* state,
                      const char* postal_code,
                      const char* country,
                      const char* phone,
                      const char* email,
                      const char* name_on_card,
                      const char* card_number,
                      const char* expiration_month,
                      const char* expiration_year,
                      bool has_address_fields,
                      bool has_credit_card_fields,
                      bool use_month_type) {
  // The number of fields in the address and credit card forms created above.
  const size_t kAddressFormSize = 11;
  const size_t kCreditCardFormSize = use_month_type ? 4 : 5;

  EXPECT_EQ(u"MyForm", filled_form.name);
  EXPECT_EQ(GURL("https://myform.com/form.html"), filled_form.url);
  EXPECT_EQ(GURL("https://myform.com/submit.html"), filled_form.action);

  size_t form_size = 0;
  if (has_address_fields)
    form_size += kAddressFormSize;
  if (has_credit_card_fields)
    form_size += kCreditCardFormSize;
  ASSERT_EQ(form_size, filled_form.fields.size());

  if (has_address_fields) {
    ExpectFilledField("First Name", "firstname", first, "text",
                      filled_form.fields[0]);
    ExpectFilledField("Middle Name", "middlename", middle, "text",
                      filled_form.fields[1]);
    ExpectFilledField("Last Name", "lastname", last, "text",
                      filled_form.fields[2]);
    ExpectFilledField("Address Line 1", "addr1", address1, "text",
                      filled_form.fields[3]);
    ExpectFilledField("Address Line 2", "addr2", address2, "text",
                      filled_form.fields[4]);
    ExpectFilledField("City", "city", city, "text", filled_form.fields[5]);
    ExpectFilledField("State", "state", state, "text", filled_form.fields[6]);
    ExpectFilledField("Postal Code", "zipcode", postal_code, "text",
                      filled_form.fields[7]);
    ExpectFilledField("Country", "country", country, "text",
                      filled_form.fields[8]);
    ExpectFilledField("Phone Number", "phonenumber", phone, "tel",
                      filled_form.fields[9]);
    ExpectFilledField("Email", "email", email, "email", filled_form.fields[10]);
  }

  if (has_credit_card_fields) {
    size_t offset = has_address_fields ? kAddressFormSize : 0;
    ExpectFilledField("Name on Card", "nameoncard", name_on_card, "text",
                      filled_form.fields[offset + 0]);
    ExpectFilledField("Card Number", "cardnumber", card_number, "text",
                      filled_form.fields[offset + 1]);
    if (use_month_type) {
      std::string exp_year = expiration_year;
      std::string exp_month = expiration_month;
      std::string date;
      if (!exp_year.empty() && !exp_month.empty())
        date = exp_year + "-" + exp_month;

      ExpectFilledField("Expiration Date", "ccmonth", date.c_str(), "month",
                        filled_form.fields[offset + 2]);
    } else {
      ExpectFilledField("Expiration Date", "ccmonth", expiration_month, "text",
                        filled_form.fields[offset + 2]);
      ExpectFilledField("", "ccyear", expiration_year, "text",
                        filled_form.fields[offset + 3]);
    }
  }
}

void ExpectFilledAddressFormElvis(const FormData& filled_form,
                                  bool has_credit_card_fields) {
  ExpectFilledForm(filled_form, "Elvis", "Aaron", "Presley",
                   "3734 Elvis Presley Blvd.", "Apt. 10", "Memphis",
                   "Tennessee", "38116", "United States", "12345678901",
                   "theking@gmail.com", "", "", "", "", true,
                   has_credit_card_fields, false);
}

void ExpectFilledCreditCardFormElvis(const FormData& filled_form,
                                     bool has_address_fields) {
  ExpectFilledForm(filled_form, "", "", "", "", "", "", "", "", "", "", "",
                   "Elvis Presley", "4234567890123456", "04", "2999",
                   has_address_fields, true, false);
}

void ExpectFilledCreditCardYearMonthWithYearMonth(const FormData& filled_form,
                                                  bool has_address_fields,
                                                  const char* year,
                                                  const char* month) {
  ExpectFilledForm(filled_form, "", "", "", "", "", "", "", "", "", "", "",
                   "Miku Hatsune", "4234567890654321", month, year,
                   has_address_fields, true, true);
}

void CheckThatOnlyFieldByIndexHasThisPossibleType(
    const FormStructure& form_structure,
    size_t field_index,
    ServerFieldType type,
    FieldPropertiesMask mask) {
  EXPECT_TRUE(field_index < form_structure.field_count());

  for (size_t i = 0; i < form_structure.field_count(); i++) {
    if (i == field_index) {
      EXPECT_THAT(form_structure.field(i)->possible_types(), ElementsAre(type));
      EXPECT_EQ(mask, form_structure.field(i)->properties_mask);
    } else {
      EXPECT_THAT(form_structure.field(i)->possible_types(),
                  Not(Contains(type)));
    }
  }
}

void CheckThatNoFieldHasThisPossibleType(const FormStructure& form_structure,
                                         ServerFieldType type) {
  for (size_t i = 0; i < form_structure.field_count(); i++) {
    EXPECT_THAT(form_structure.field(i)->possible_types(), Not(Contains(type)));
  }
}

class MockAutofillDriver : public TestAutofillDriver {
 public:
  MockAutofillDriver() = default;
  MockAutofillDriver(const MockAutofillDriver&) = delete;
  MockAutofillDriver& operator=(const MockAutofillDriver&) = delete;

  // Mock methods to enable testability.
  MOCK_METHOD((std::vector<FieldGlobalId>),
              FillOrPreviewForm,
              (mojom::RendererFormDataAction action,
               const FormData& data,
               const url::Origin& triggered_origin,
               (const base::flat_map<FieldGlobalId, ServerFieldType>&)),
              (override));
  MOCK_METHOD(void,
              SendAutofillTypePredictionsToRenderer,
              (const std::vector<FormStructure*>& forms),
              (override));
  MOCK_METHOD(void,
              SendFieldsEligibleForManualFillingToRenderer,
              (const std::vector<FieldGlobalId>& fields),
              (override));
};

}  // namespace

class BrowserAutofillManagerTest : public testing::Test {
 public:
  BrowserAutofillManagerTest() {
    scoped_feature_list_async_parse_form_.InitWithFeatureState(
        features::kAutofillParseAsync, true);
  }

  void SetUp() override {
    autofill_client_.SetPrefs(test::PrefServiceForTesting());
    personal_data().set_auto_accept_address_imports_for_testing(true);
    personal_data().Init(/*profile_database=*/database_,
                         /*account_database=*/nullptr,
                         /*pref_service=*/autofill_client_.GetPrefs(),
                         /*local_state=*/autofill_client_.GetPrefs(),
                         /*identity_manager=*/nullptr,
                         /*history_service=*/nullptr,
                         /*sync_service=*/nullptr,
                         /*strike_database=*/nullptr,
                         /*image_fetcher=*/nullptr,
                         /*is_off_the_record=*/false);
    personal_data().SetPrefService(autofill_client_.GetPrefs());

    autocomplete_history_manager_ =
        std::make_unique<NiceMock<MockAutocompleteHistoryManager>>();
    autocomplete_history_manager_->Init(
        /*profile_database=*/database_,
        /*pref_service=*/autofill_client_.GetPrefs(),
        /*is_off_the_record=*/false);
    iban_manager_ =
        std::make_unique<NiceMock<MockIBANManager>>(&personal_data());
    merchant_promo_code_manager_ =
        std::make_unique<NiceMock<MockMerchantPromoCodeManager>>();
    merchant_promo_code_manager_->Init(&personal_data(),
                                       /*is_off_the_record=*/false);

    autofill_driver_ = std::make_unique<NiceMock<MockAutofillDriver>>();
    auto payments_client = std::make_unique<payments::TestPaymentsClient>(
        autofill_client_.GetURLLoaderFactory(),
        autofill_client_.GetIdentityManager(), &personal_data());
    payments_client_ = payments_client.get();
    autofill_client_.set_test_payments_client(std::move(payments_client));
    TestCreditCardSaveManager* credit_card_save_manager =
        new TestCreditCardSaveManager(autofill_driver_.get(), &autofill_client_,
                                      payments_client_, &personal_data());
    credit_card_save_manager->SetCreditCardUploadEnabled(true);
    auto test_form_data_importer =
        std::make_unique<autofill::TestFormDataImporter>(
            &autofill_client_, payments_client_,
            std::unique_ptr<CreditCardSaveManager>(credit_card_save_manager),
            /*iban_save_manager=*/nullptr, &personal_data(), "en-US");
    test_form_data_importer_ = test_form_data_importer.get();
    autofill_client_.set_test_form_data_importer(
        std::move(test_form_data_importer));
    browser_autofill_manager_ = std::make_unique<TestBrowserAutofillManager>(
        autofill_driver_.get(), &autofill_client_);

    auto single_field_form_fill_router =
        std::make_unique<NiceMock<MockSingleFieldFormFillRouter>>(
            autocomplete_history_manager_.get(), iban_manager_.get(),
            merchant_promo_code_manager_.get());
    // By default, if we offer single field form fill, suggestions should be
    // returned because it is assumed |field.should_autocomplete| is set to
    // true. This should be overridden in tests where
    // |field.should_autocomplete| is set to false.
    ON_CALL(*single_field_form_fill_router, OnGetSingleFieldSuggestions)
        .WillByDefault(testing::Return(true));
    single_field_form_fill_router_ = single_field_form_fill_router.get();
    browser_autofill_manager_->set_single_field_form_fill_router_for_test(
        std::move(single_field_form_fill_router));

    auto download_manager =
        std::make_unique<MockAutofillDownloadManager>(&autofill_client_);
    download_manager_ = download_manager.get();
    autofill_client_.set_download_manager(std::move(download_manager));

    auto external_delegate = std::make_unique<TestAutofillExternalDelegate>(
        browser_autofill_manager_.get(), autofill_driver_.get(),
        /*call_parent_methods=*/false);
    external_delegate_ = external_delegate.get();
    browser_autofill_manager_->SetExternalDelegateForTest(
        std::move(external_delegate));

    browser_autofill_manager_->set_touch_to_fill_delegate(
        std::make_unique<MockTouchToFillDelegate>());
    ON_CALL(touch_to_fill_delegate(), GetManager())
        .WillByDefault(Return(browser_autofill_manager_.get()));
    ON_CALL(touch_to_fill_delegate(), IsShowingTouchToFill())
        .WillByDefault(Return(false));

    browser_autofill_manager_->set_fast_checkout_delegate(
        std::make_unique<MockFastCheckoutDelegate>());
    ON_CALL(fast_checkout_delegate(), IsShowingFastCheckoutUI())
        .WillByDefault(Return(false));

    auto test_strike_database = std::make_unique<TestStrikeDatabase>();
    strike_database_ = test_strike_database.get();
    autofill_client_.set_test_strike_database(std::move(test_strike_database));

    // Initialize the TestPersonalDataManager with some default data.
    CreateTestAutofillProfiles();
    CreateTestCreditCards();
  }

  void CreateTestServerCreditCards() {
    personal_data().ClearCreditCards();

    CreditCard masked_server_card;
    test::SetCreditCardInfo(&masked_server_card, "Elvis Presley",
                            "4234567890123456",  // Visa
                            "04", "2999", "1");
    masked_server_card.set_guid("00000000-0000-0000-0000-000000000007");
    masked_server_card.set_record_type(CreditCard::MASKED_SERVER_CARD);
    personal_data().AddServerCreditCard(masked_server_card);

    CreditCard full_server_card;
    test::SetCreditCardInfo(&full_server_card, "Buddy Holly",
                            "5187654321098765",  // Mastercard
                            "10", "2998", "1");
    full_server_card.set_guid("00000000-0000-0000-0000-000000000008");
    full_server_card.set_record_type(CreditCard::FULL_SERVER_CARD);
    personal_data().AddServerCreditCard(full_server_card);
  }

  void CreateTestServerAndLocalCreditCards() {
    personal_data().ClearCreditCards();

    CreditCard masked_server_card;
    test::SetCreditCardInfo(&masked_server_card, "Elvis Presley",
                            "4234567890123456",  // Visa
                            "04", "2999", "1");
    masked_server_card.set_guid("00000000-0000-0000-0000-000000000007");
    masked_server_card.set_record_type(CreditCard::MASKED_SERVER_CARD);
    personal_data().AddServerCreditCard(masked_server_card);

    CreditCard full_server_card;
    test::SetCreditCardInfo(&full_server_card, "Buddy Holly",
                            "5187654321098765",  // Mastercard
                            "10", "2998", "1");
    full_server_card.set_guid("00000000-0000-0000-0000-000000000008");
    full_server_card.set_record_type(CreditCard::FULL_SERVER_CARD);
    personal_data().AddServerCreditCard(full_server_card);

    CreditCard local_card;
    test::SetCreditCardInfo(&local_card, "Elvis Presley",
                            "4234567890123456",  // Visa
                            "04", "2999", "1");
    local_card.set_guid("00000000-0000-0000-0000-000000000009");
    local_card.set_record_type(CreditCard::LOCAL_CARD);
    personal_data().AddCreditCard(local_card);
  }

  void TearDown() override {
    // Order of destruction is important as BrowserAutofillManager relies on
    // PersonalDataManager to be around when it gets destroyed.
    browser_autofill_manager_.reset();

    personal_data().SetPrefService(nullptr);
    personal_data().ClearCreditCards();
  }

  MockTouchToFillDelegate& touch_to_fill_delegate() {
    return *static_cast<MockTouchToFillDelegate*>(
        browser_autofill_manager_->touch_to_fill_delegate());
  }

  MockFastCheckoutDelegate& fast_checkout_delegate() {
    return *static_cast<MockFastCheckoutDelegate*>(
        browser_autofill_manager_->fast_checkout_delegate());
  }

  void GetAutofillSuggestions(const FormData& form,
                              const FormFieldData& field) {
    browser_autofill_manager_->OnAskForValuesToFill(
        form, field, gfx::RectF(), AutoselectFirstSuggestion(false),
        FormElementWasClicked(false));
  }

  void TryToShowTouchToFill(const FormData& form,
                            const FormFieldData& field,
                            FormElementWasClicked form_element_was_clicked) {
    browser_autofill_manager_->OnAskForValuesToFill(
        form, field, gfx::RectF(), AutoselectFirstSuggestion(false),
        form_element_was_clicked);
  }

  void AutocompleteSuggestionsReturned(
      FieldGlobalId field_id,
      const std::vector<std::u16string>& results) {
    std::vector<Suggestion> suggestions;
    base::ranges::transform(
        results, std::back_inserter(suggestions),
        [](const auto& result) { return Suggestion(result); });

    browser_autofill_manager_->OnSuggestionsReturned(
        field_id, AutoselectFirstSuggestion(false), suggestions);
  }

  void FormsSeen(const std::vector<FormData>& forms) {
    browser_autofill_manager_->OnFormsSeen(/*updated_forms=*/forms,
                                           /*removed_forms=*/{});
  }

  void FormSubmitted(const FormData& form) {
    browser_autofill_manager_->OnFormSubmitted(
        form, false, SubmissionSource::FORM_SUBMISSION);
  }

  void FillAutofillFormData(const FormData& form,
                            const FormFieldData& field,
                            int unique_id) {
    browser_autofill_manager_->OnAskForValuesToFill(
        form, field, {}, AutoselectFirstSuggestion(true),
        FormElementWasClicked(false));
    browser_autofill_manager_->FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, form, field, unique_id,
        AutofillTriggerSource::kPopup);
  }

  // Calls |browser_autofill_manager_->OnFillAutofillFormData()| with the
  // specified input parameters after setting up the expectation that the mock
  // driver's |FillOrPreviewForm()| method will be called and saving the
  // parameter of that call into the |response_data| output parameter.
  void FillAutofillFormDataAndSaveResults(const FormData& input_form,
                                          const FormFieldData& input_field,
                                          int unique_id,
                                          FormData* response_data) {
    EXPECT_CALL(*autofill_driver_, FillOrPreviewForm(_, _, _, _))
        .WillOnce(DoAll(testing::SaveArg<1>(response_data),
                        testing::Return(std::vector<FieldGlobalId>{})));
    FillAutofillFormData(input_form, input_field, unique_id);
  }

  void PreviewVirtualCardDataAndSaveResults(
      mojom::RendererFormDataAction action,
      const std::string& guid,
      const FormData& input_form,
      const FormFieldData& input_field,
      FormData* response_data) {
    EXPECT_CALL(*autofill_driver_, FillOrPreviewForm(_, _, _, _))
        .WillOnce((DoAll(testing::SaveArg<1>(response_data),
                         testing::Return(std::vector<FieldGlobalId>{}))));
    browser_autofill_manager_->FillOrPreviewVirtualCardInformation(
        action, guid, input_form, input_field, AutofillTriggerSource::kPopup);
  }

  int MakeFrontendId(
      const TestBrowserAutofillManager::MakeFrontendIdParams& params) {
    return browser_autofill_manager_->MakeFrontendId(params);
  }

  bool WillFillCreditCardNumber(const FormData& form,
                                const FormFieldData& field) {
    return browser_autofill_manager_->WillFillCreditCardNumberForTest(form,
                                                                      field);
  }

  FormData CreateTestCreditCardFormData(bool is_https, bool use_month_type) {
    FormData form;
    CreateTestCreditCardFormData(&form, is_https, use_month_type);
    return form;
  }

  // Populates |form| with data corresponding to a simple credit card form.
  // Note that this actually appends fields to the form data, which can be
  // useful for building up more complex test forms.
  void CreateTestCreditCardFormData(FormData* form,
                                    bool is_https,
                                    bool use_month_type) {
    form->name = u"MyForm";
    if (is_https) {
      form->url = GURL("https://myform.com/form.html");
      form->action = GURL("https://myform.com/submit.html");
    } else {
      // If we are testing a form that submits over HTTP, we also need to set
      // the main frame to HTTP, otherwise mixed form warnings will trigger and
      // autofill will be disabled.
      GURL::Replacements replacements;
      replacements.SetSchemeStr(url::kHttpScheme);
      autofill_client_.set_form_origin(
          autofill_client_.form_origin().ReplaceComponents(replacements));
      form->url = GURL("http://myform.com/form.html");
      form->action = GURL("http://myform.com/submit.html");
    }

    FormFieldData field;
    test::CreateTestFormField("Name on Card", "nameoncard", "", "text", &field);
    form->fields.push_back(field);
    test::CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
    form->fields.push_back(field);
    if (use_month_type) {
      test::CreateTestFormField("Expiration Date", "ccmonth", "", "month",
                                &field);
      form->fields.push_back(field);
    } else {
      test::CreateTestFormField("Expiration Date", "ccmonth", "", "text",
                                &field);
      form->fields.push_back(field);
      test::CreateTestFormField("", "ccyear", "", "text", &field);
      form->fields.push_back(field);
    }
    test::CreateTestFormField("CVC", "cvc", "", "text", &field);
    form->fields.push_back(field);
  }

  void PrepareForRealPanResponse(FormData* form, CreditCard* card) {
    // This line silences the warning from PaymentsClient about matching sync
    // and Payments server types.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        "sync-url", "https://google.com");

    CreateTestCreditCardFormData(form, true, false);
    FormsSeen({*form});
    *card = CreditCard(CreditCard::MASKED_SERVER_CARD, "a123");
    test::SetCreditCardInfo(card, "John Dillinger", "1881" /* Visa */, "01",
                            "2017", "1");
    card->SetNetworkForMaskedCard(kVisaCard);

    EXPECT_CALL(*autofill_driver_, FillOrPreviewForm(_, _, _, _))
        .Times(AtLeast(1));
    browser_autofill_manager_->FillOrPreviewCreditCardForm(
        mojom::RendererFormDataAction::kFill, *form, form->fields[0], card,
        AutofillTriggerSource::kPopup);
  }

  void OnDidGetRealPan(AutofillClient::PaymentsRpcResult result,
                       const std::string& real_pan,
                       bool is_virtual_card = false) {
    payments::FullCardRequest* full_card_request =
        browser_autofill_manager_->client()
            ->GetCvcAuthenticator()
            ->full_card_request_.get();
    DCHECK(full_card_request);

    // Mock user response.
    payments::FullCardRequest::UserProvidedUnmaskDetails details;
    details.cvc = u"123";
    full_card_request->OnUnmaskPromptAccepted(details);

    // Mock payments response.
    payments::PaymentsClient::UnmaskResponseDetails response;
    response.card_type = is_virtual_card
                             ? AutofillClient::PaymentsRpcCardType::kVirtualCard
                             : AutofillClient::PaymentsRpcCardType::kServerCard;
    full_card_request->OnDidGetRealPan(result,
                                       response.with_real_pan(real_pan));
  }

  // Convenience method to cast the FullCardRequest into a CardUnmaskDelegate.
  CardUnmaskDelegate* full_card_unmask_delegate() {
    payments::FullCardRequest* full_card_request =
        browser_autofill_manager_->client()
            ->GetCvcAuthenticator()
            ->full_card_request_.get();
    DCHECK(full_card_request);
    return static_cast<CardUnmaskDelegate*>(full_card_request);
  }

  void DisableAutofillViaAblation(
      base::test::ScopedFeatureList& scoped_feature_list,
      bool for_addresses,
      bool for_credit_cards) {
    base::FieldTrialParams feature_parameters{
        {features::kAutofillAblationStudyEnabledForAddressesParam.name,
         for_addresses ? "true" : "false"},
        {features::kAutofillAblationStudyEnabledForPaymentsParam.name,
         for_credit_cards ? "true" : "false"},
        {features::kAutofillAblationStudyAblationWeightPerMilleParam.name,
         "1000"},
    };
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        features::kAutofillEnableAblationStudy, feature_parameters);
  }

  // Wrappers around the TestAutofillExternalDelegate::GetSuggestions call that
  // take a hardcoded number of expected results so callsites are cleaner.
  void CheckSuggestions(FieldGlobalId field_id, const Suggestion& suggestion0) {
    std::vector<Suggestion> suggestion_vector;
    suggestion_vector.push_back(suggestion0);
    external_delegate_->CheckSuggestions(field_id, 1, &suggestion_vector[0]);
  }
  void CheckSuggestions(FieldGlobalId field_id,
                        const Suggestion& suggestion0,
                        const Suggestion& suggestion1) {
    std::vector<Suggestion> suggestion_vector;
    suggestion_vector.push_back(suggestion0);
    suggestion_vector.push_back(suggestion1);
    external_delegate_->CheckSuggestions(field_id, 2, &suggestion_vector[0]);
  }
  void CheckSuggestions(FieldGlobalId field_id,
                        const Suggestion& suggestion0,
                        const Suggestion& suggestion1,
                        const Suggestion& suggestion2) {
    std::vector<Suggestion> suggestion_vector;
    suggestion_vector.push_back(suggestion0);
    suggestion_vector.push_back(suggestion1);
    suggestion_vector.push_back(suggestion2);
    external_delegate_->CheckSuggestions(field_id, 3, &suggestion_vector[0]);
  }

  void ResetBrowserAutofillManager(TestAutofillClient* client) {
    // |browser_autofill_manager_| owns the |single_field_form_fill_router_| and
    // clears it upon being recreated. Clear it first and then give it a new
    // SingleFieldFormFillRouter to avoid referencing deleted memory.
    browser_autofill_manager_ = std::make_unique<TestBrowserAutofillManager>(
        autofill_driver_.get(), &autofill_client_);

    auto single_field_form_fill_router =
        std::make_unique<NiceMock<MockSingleFieldFormFillRouter>>(
            autocomplete_history_manager_.get(), iban_manager_.get(),
            merchant_promo_code_manager_.get());
    single_field_form_fill_router_ = single_field_form_fill_router.get();
    browser_autofill_manager_->set_single_field_form_fill_router_for_test(
        std::move(single_field_form_fill_router));
  }

  // Matches a AskForValuesToFillFieldLogEvent by equality of fields.
  auto Equal(const AskForValuesToFillFieldLogEvent& expected) {
    return VariantWith<AskForValuesToFillFieldLogEvent>(
        AllOf(Field("has_suggestion",
                    &AskForValuesToFillFieldLogEvent::has_suggestion,
                    expected.has_suggestion),
              Field("suggestion_is_shown",
                    &AskForValuesToFillFieldLogEvent::suggestion_is_shown,
                    expected.suggestion_is_shown)));
  }

  // Matches a TriggerFillFieldLogEvent by equality of fields.
  auto Equal(const TriggerFillFieldLogEvent& expected) {
    return VariantWith<TriggerFillFieldLogEvent>(
        AllOf(Field("fill_event_id", &TriggerFillFieldLogEvent::fill_event_id,
                    expected.fill_event_id),
              Field("data_type", &TriggerFillFieldLogEvent::data_type,
                    expected.data_type),
              Field("associated_country_code",
                    &TriggerFillFieldLogEvent::associated_country_code,
                    expected.associated_country_code),
              Field("timestamp", &TriggerFillFieldLogEvent::timestamp,
                    expected.timestamp)));
  }

  // Matches a FillFieldLogEvent by equality of fields. Use FillEventId(-1) if
  // you want to ignore the fill_event_id.
  auto Equal(const FillFieldLogEvent& expected) {
    return VariantWith<FillFieldLogEvent>(
        AllOf(testing::Conditional(
                  expected.fill_event_id == FillEventId(-1), _,
                  Field("fill_event_id", &FillFieldLogEvent::fill_event_id,
                        expected.fill_event_id)),
              Field("had_value_before_filling",
                    &FillFieldLogEvent::had_value_before_filling,
                    expected.had_value_before_filling),
              Field("autofill_skipped_status",
                    &FillFieldLogEvent::autofill_skipped_status,
                    expected.autofill_skipped_status),
              Field("was_autofilled", &FillFieldLogEvent::was_autofilled,
                    expected.was_autofilled),
              Field("had_value_after_filling",
                    &FillFieldLogEvent::had_value_after_filling,
                    expected.had_value_after_filling)));
  }

  // Matches a TypingFieldLogEvent by equality of fields.
  auto Equal(const TypingFieldLogEvent& expected) {
    return VariantWith<TypingFieldLogEvent>(Field(
        "has_value_after_typing", &TypingFieldLogEvent::has_value_after_typing,
        expected.has_value_after_typing));
  }

  // Matches a HeuristicPredictionFieldLogEvent by equality of fields.
  auto Equal(const HeuristicPredictionFieldLogEvent& expected) {
    return VariantWith<HeuristicPredictionFieldLogEvent>(AllOf(
        Field("field_type", &HeuristicPredictionFieldLogEvent::field_type,
              expected.field_type),
        Field("pattern_source",
              &HeuristicPredictionFieldLogEvent::pattern_source,
              expected.pattern_source),
        Field("is_active_pattern_source",
              &HeuristicPredictionFieldLogEvent::is_active_pattern_source,
              expected.is_active_pattern_source),
        Field("rank_in_field_signature_group",
              &HeuristicPredictionFieldLogEvent::rank_in_field_signature_group,
              expected.rank_in_field_signature_group)));
  }

  // Matches a AutocompleteAttributeFieldLogEvent by equality of fields.
  auto Equal(const AutocompleteAttributeFieldLogEvent& expected) {
    return VariantWith<AutocompleteAttributeFieldLogEvent>(AllOf(
        Field("html_type", &AutocompleteAttributeFieldLogEvent::html_type,
              expected.html_type),
        Field("html_mode", &AutocompleteAttributeFieldLogEvent::html_mode,
              expected.html_mode),
        Field(
            "rank_in_field_signature_group",
            &AutocompleteAttributeFieldLogEvent::rank_in_field_signature_group,
            expected.rank_in_field_signature_group)));
  }

  // Matches a ServerPredictionFieldLogEvent by equality of fields.
  auto Equal(const ServerPredictionFieldLogEvent& expected) {
    return VariantWith<ServerPredictionFieldLogEvent>(AllOf(
        Field("server_type1", &ServerPredictionFieldLogEvent::server_type1,
              expected.server_type1),
        Field("prediction_source1",
              &ServerPredictionFieldLogEvent::prediction_source1,
              expected.prediction_source1),
        Field("server_type2", &ServerPredictionFieldLogEvent::server_type2,
              expected.server_type2),
        Field("prediction_source2",
              &ServerPredictionFieldLogEvent::prediction_source2,
              expected.prediction_source2),
        Field(
            "server_type_prediction_is_override",
            &ServerPredictionFieldLogEvent::server_type_prediction_is_override,
            expected.server_type_prediction_is_override),
        Field("rank_in_field_signature_group",
              &ServerPredictionFieldLogEvent::rank_in_field_signature_group,
              expected.rank_in_field_signature_group)));
  }

  // Matches a RationalizationFieldLogEvent by equality of fields.
  auto Equal(const RationalizationFieldLogEvent& expected) {
    return VariantWith<RationalizationFieldLogEvent>(
        AllOf(Field("field_type", &RationalizationFieldLogEvent::field_type,
                    expected.field_type),
              Field("section_id", &RationalizationFieldLogEvent::section_id,
                    expected.section_id),
              Field("type_changed", &RationalizationFieldLogEvent::type_changed,
                    expected.type_changed)));
  }

  // Matches a vector of FieldLogEventType objects by equality of fields of each
  // log event type.
  auto ArrayEquals(
      const std::vector<AutofillField::FieldLogEventType>& expected) {
    static_assert(
        absl::variant_size<AutofillField::FieldLogEventType>() == 9,
        "If you add a new field event type, you need to update this function");
    std::vector<Matcher<AutofillField::FieldLogEventType>> matchers;
    for (const auto& event : expected) {
      if (absl::holds_alternative<AskForValuesToFillFieldLogEvent>(event)) {
        matchers.push_back(
            Equal(absl::get<AskForValuesToFillFieldLogEvent>(event)));
      } else if (absl::holds_alternative<TriggerFillFieldLogEvent>(event)) {
        matchers.push_back(Equal(absl::get<TriggerFillFieldLogEvent>(event)));
      } else if (absl::holds_alternative<FillFieldLogEvent>(event)) {
        matchers.push_back(Equal(absl::get<FillFieldLogEvent>(event)));
      } else if (absl::holds_alternative<TypingFieldLogEvent>(event)) {
        matchers.push_back(Equal(absl::get<TypingFieldLogEvent>(event)));
      } else if (absl::holds_alternative<HeuristicPredictionFieldLogEvent>(
                     event)) {
        matchers.push_back(
            Equal(absl::get<HeuristicPredictionFieldLogEvent>(event)));
      } else if (absl::holds_alternative<AutocompleteAttributeFieldLogEvent>(
                     event)) {
        matchers.push_back(
            Equal(absl::get<AutocompleteAttributeFieldLogEvent>(event)));
      } else if (absl::holds_alternative<ServerPredictionFieldLogEvent>(
                     event)) {
        matchers.push_back(
            Equal(absl::get<ServerPredictionFieldLogEvent>(event)));
      } else if (absl::holds_alternative<RationalizationFieldLogEvent>(event)) {
        matchers.push_back(
            Equal(absl::get<RationalizationFieldLogEvent>(event)));
      } else {
        NOTREACHED();
      }
    }
    return ElementsAreArray(matchers);
  }

 protected:
  TestPersonalDataManager& personal_data() {
    return *autofill_client_.GetPersonalDataManager();
  }

  base::test::TaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  NiceMock<MockAutofillClient> autofill_client_;
  std::unique_ptr<MockAutofillDriver> autofill_driver_;
  std::unique_ptr<TestBrowserAutofillManager> browser_autofill_manager_;
  raw_ptr<TestAutofillExternalDelegate> external_delegate_;
  scoped_refptr<AutofillWebDataService> database_;
  raw_ptr<MockAutofillDownloadManager> download_manager_;
  std::unique_ptr<MockAutocompleteHistoryManager> autocomplete_history_manager_;
  std::unique_ptr<MockIBANManager> iban_manager_;
  std::unique_ptr<MockMerchantPromoCodeManager> merchant_promo_code_manager_;
  raw_ptr<MockSingleFieldFormFillRouter> single_field_form_fill_router_;
  raw_ptr<TestStrikeDatabase> strike_database_;
  raw_ptr<payments::TestPaymentsClient> payments_client_;
  raw_ptr<TestFormDataImporter> test_form_data_importer_;

 private:
  int ToHistogramSample(autofill_metrics::CardUploadDecision metric) {
    for (int sample = 0; sample < metric + 1; ++sample)
      if (metric & (1 << sample))
        return sample;

    NOTREACHED();
    return 0;
  }

  void CreateTestAutofillProfiles() {
    AutofillProfile profile1;
    test::SetProfileInfo(&profile1, "Elvis", "Aaron", "Presley",
                         "theking@gmail.com", "RCA", "3734 Elvis Presley Blvd.",
                         "Apt. 10", "Memphis", "Tennessee", "38116", "US",
                         "12345678901");
    profile1.set_guid("00000000-0000-0000-0000-000000000001");
    personal_data().AddProfile(profile1);

    AutofillProfile profile2;
    test::SetProfileInfo(&profile2, "Charles", "Hardin", "Holley",
                         "buddy@gmail.com", "Decca", "123 Apple St.", "unit 6",
                         "Lubbock", "Texas", "79401", "US", "23456789012");
    profile2.set_guid("00000000-0000-0000-0000-000000000002");
    personal_data().AddProfile(profile2);

    AutofillProfile profile3;
    test::SetProfileInfo(&profile3, "", "", "", "", "", "", "", "", "", "", "",
                         "");
    profile3.set_guid("00000000-0000-0000-0000-000000000003");
    personal_data().AddProfile(profile3);
  }

  void CreateTestCreditCards() {
    CreditCard credit_card1;
    test::SetCreditCardInfo(&credit_card1, "Elvis Presley",
                            "4234567890123456",  // Visa
                            "04", "2999", "1");
    credit_card1.set_guid("00000000-0000-0000-0000-000000000004");
    credit_card1.set_use_count(10);
    credit_card1.set_use_date(AutofillClock::Now() - base::Days(5));
    personal_data().AddCreditCard(credit_card1);

    CreditCard credit_card2;
    test::SetCreditCardInfo(&credit_card2, "Buddy Holly",
                            "5187654321098765",  // Mastercard
                            "10", "2998", "1");
    credit_card2.set_guid("00000000-0000-0000-0000-000000000005");
    credit_card2.set_use_count(5);
    credit_card2.set_use_date(AutofillClock::Now() - base::Days(4));
    personal_data().AddCreditCard(credit_card2);

    CreditCard credit_card3;
    test::SetCreditCardInfo(&credit_card3, "", "", "", "", "");
    credit_card3.set_guid("00000000-0000-0000-0000-000000000006");
    personal_data().AddCreditCard(credit_card3);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_async_parse_form_;
};

class SuggestionMatchingTest
    : public BrowserAutofillManagerTest,
      public testing::WithParamInterface<std::tuple<bool, std::string>> {
 protected:
  void SetUp() override {
    BrowserAutofillManagerTest::SetUp();
    InitializeFeatures();
  }

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  void InitializeFeatures();
#else
  void InitializeFeatures();
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

  std::string MakeLabel(const std::vector<std::string>& parts);
  std::string MakeMobileLabel(const std::vector<std::string>& parts);

  enum class EnabledFeature { kNone, kDesktop, kMobileShowAll, kMobileShowOne };
  EnabledFeature enabled_feature_;
  base::test::ScopedFeatureList features_;
};

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
void SuggestionMatchingTest::InitializeFeatures() {
  if (std::get<0>(GetParam())) {
    std::string variant = std::get<1>(GetParam());

    if (variant ==
        features::kAutofillUseMobileLabelDisambiguationParameterShowAll) {
      enabled_feature_ = EnabledFeature::kMobileShowAll;
    } else if (variant ==
               features::
                   kAutofillUseMobileLabelDisambiguationParameterShowOne) {
      enabled_feature_ = EnabledFeature::kMobileShowOne;
    } else {
      NOTREACHED();
    }

    std::map<std::string, std::string> parameters;
    parameters[features::kAutofillUseMobileLabelDisambiguationParameterName] =
        variant;
    features_.InitAndEnableFeatureWithParameters(
        features::kAutofillUseMobileLabelDisambiguation, parameters);
  } else {
    enabled_feature_ = EnabledFeature::kNone;
  }
}
#else
void SuggestionMatchingTest::InitializeFeatures() {
  enabled_feature_ = std::get<0>(GetParam()) ? EnabledFeature::kDesktop
                                             : EnabledFeature::kNone;
  features_.InitWithFeatureState(
      features::kAutofillUseImprovedLabelDisambiguation,
      std::get<0>(GetParam()));
}
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

std::string SuggestionMatchingTest::MakeLabel(
    const std::vector<std::string>& parts) {
  return base::JoinString(
      parts, l10n_util::GetStringUTF8(IDS_AUTOFILL_SUGGESTION_LABEL_SEPARATOR));
}

std::string SuggestionMatchingTest::MakeMobileLabel(
    const std::vector<std::string>& parts) {
  return base::JoinString(
      parts, l10n_util::GetStringUTF8(IDS_AUTOFILL_ADDRESS_SUMMARY_SEPARATOR));
}

// Credit card suggestion tests related with keyboard accessory.
class CreditCardSuggestionTest : public BrowserAutofillManagerTest,
                                 public testing::WithParamInterface<bool> {
 protected:
  CreditCardSuggestionTest() {
#if BUILDFLAG(IS_ANDROID)
    is_keyboard_accessory_enabled_ = GetParam();
#endif
  }

  void SetUp() override {
    BrowserAutofillManagerTest::SetUp();
#if BUILDFLAG(IS_ANDROID)
    feature_list_keyboard_accessory_.InitWithFeatureState(
        features::kAutofillKeyboardAccessory, is_keyboard_accessory_enabled_);
#endif
    feature_list_card_metadata_and_product_name_.InitWithFeatures(
        /* enabled_features */ {},
        /* disabled_features */ {features::kAutofillEnableVirtualCardMetadata,
                                 features::kAutofillEnableCardProductName});
  }

  int ObfuscationLength() {
#if BUILDFLAG(IS_ANDROID)
    return is_keyboard_accessory_enabled_ ? 2 : 4;
#else
    return 4;
#endif
  }

 private:
#if BUILDFLAG(IS_ANDROID)
  bool is_keyboard_accessory_enabled_;
#endif
  base::test::ScopedFeatureList feature_list_keyboard_accessory_;
  base::test::ScopedFeatureList feature_list_card_metadata_and_product_name_;
};

// Test that calling OnFormsSeen with an empty set of forms (such as when
// reloading a page or when the renderer processes a set of forms but detects
// no changes) does not load the forms again.
TEST_F(BrowserAutofillManagerTest, OnFormsSeen_Empty) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);

  base::HistogramTester histogram_tester;
  FormsSeen({form});
  histogram_tester.ExpectUniqueSample("Autofill.UserHappiness",
                                      0 /* FORMS_LOADED */, 1);

  // No more forms, metric is not logged.
  FormsSeen({});
  histogram_tester.ExpectUniqueSample("Autofill.UserHappiness",
                                      0 /* FORMS_LOADED */, 1);
}

// Test that calling OnFormsSeen consecutively with a different set of forms
// will query for each separately.
TEST_F(BrowserAutofillManagerTest, OnFormsSeen_DifferentFormStructures) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  base::HistogramTester histogram_tester;
  FormsSeen({form});
  histogram_tester.ExpectUniqueSample("Autofill.UserHappiness",
                                      0 /* FORMS_LOADED */, 1);
  download_manager_->VerifyLastQueriedForms({form});

  // Different form structure.
  FormData form2;
  form2.host_frame = test::MakeLocalFrameToken();
  form2.unique_renderer_id = test::MakeFormRendererId();
  form2.name = u"MyForm";
  form2.url = GURL("https://myform.com/form.html");
  form2.action = GURL("https://myform.com/submit.html");
  FormFieldData field;
  test::CreateTestFormField("First Name", "firstname", "", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Email", "email", "", "text", &field);
  form2.fields.push_back(field);

  FormsSeen({form2});
  histogram_tester.ExpectUniqueSample("Autofill.UserHappiness",
                                      0 /* FORMS_LOADED */, 2);
  download_manager_->VerifyLastQueriedForms({form2});
}

// Test that when forms are seen, the renderer is updated with the predicted
// field types
TEST_F(BrowserAutofillManagerTest,
       OnFormsSeen_SendAutofillTypePredictionsToRenderer) {
  // Set up a queryable form.
  FormData form1;
  test::CreateTestAddressFormData(&form1);

  // Set up a non-queryable form.
  FormData form2;
  FormFieldData field;
  test::CreateTestFormField("Querty", "qwerty", "", "text", &field);
  form2.host_frame = test::MakeLocalFrameToken();
  form2.unique_renderer_id = test::MakeFormRendererId();
  form2.name = u"NonQueryable";
  form2.url = form1.url;
  form2.action = GURL("https://myform.com/submit.html");
  form2.fields.push_back(field);

  // Package the forms for observation.

  // Setup expectations.
  EXPECT_CALL(*autofill_driver_, SendAutofillTypePredictionsToRenderer(_))
      .Times(2);
  FormsSeen({form1, form2});
}

// Test that when forms are seen, the renderer is sent the fields that are
// eligible for manual filling.
TEST_F(BrowserAutofillManagerTest,
       OnFormsSeen_SendFieldsEligibleForManualFillingToRenderer) {
  // Set up a queryable form.
  FormData form1 = CreateTestCreditCardFormData(true, false);

  // Set up a non-queryable form.
  FormData form2;
  FormFieldData field;
  test::CreateTestFormField("Querty", "qwerty", "", "text", &field);
  form2.host_frame = test::MakeLocalFrameToken();
  form2.unique_renderer_id = test::MakeFormRendererId();
  form2.name = u"NonQueryable";
  form2.url = form1.url;
  form2.action = GURL("https://myform.com/submit.html");
  form2.fields.push_back(field);

  // Set up expectations.
  EXPECT_CALL(*autofill_driver_,
              SendFieldsEligibleForManualFillingToRenderer(_))
      .Times(2);
  FormsSeen({form1, form2});
}

// Test that no autofill suggestions are returned for a field with an
// unrecognized autocomplete attribute.
TEST_F(BrowserAutofillManagerTest,
       GetProfileSuggestions_UnrecognizedAttribute) {
  // Set up our form data.
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  FormFieldData field;
  // Set a valid autocomplete attribute for the first name.
  test::CreateTestFormField("First name", "firstname", "", "text", "given-name",
                            &field);
  form.fields.push_back(field);
  // Set no autocomplete attribute for the middle name.
  test::CreateTestFormField("Middle name", "middle", "", "text", "", &field);
  form.fields.push_back(field);
  // Set an unrecognized autocomplete attribute for the last name.
  test::CreateTestFormField("Last Name", "lastname", "", "text", "unrecognized",
                            &field);
  form.fields.push_back(field);
  FormsSeen({form});

  // Ensure that the SingleFieldFormFillRouter is not called for
  // suggestions either.
  EXPECT_CALL(*single_field_form_fill_router_, OnGetSingleFieldSuggestions)
      .Times(0);

  // Suggestions should be returned for the first two fields.
  GetAutofillSuggestions(form, form.fields[0]);
  external_delegate_->CheckSuggestionCount(form.fields[0].global_id(), 2);
  GetAutofillSuggestions(form, form.fields[1]);
  external_delegate_->CheckSuggestionCount(form.fields[1].global_id(), 2);

  // No suggestions should not be provided for the third field because of its
  // unrecognized autocomplete attribute.
  GetAutofillSuggestions(form, form.fields[2]);
  external_delegate_->CheckNoSuggestions(form.fields[2].global_id());
}

// Test that when small forms are disabled (min required fields enforced) no
// suggestions are returned when there are less than three fields and none of
// them have an autocomplete attribute.
TEST_F(BrowserAutofillManagerTest,
       GetProfileSuggestions_MinFieldsEnforced_NoAutocomplete) {
  // Set up our form data.
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  FormFieldData field;
  test::CreateTestFormField("First Name", "firstname", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  form.fields.push_back(field);

  FormsSeen({form});

  // Ensure that the SingleFieldFormFillRouter is called for both fields.
  EXPECT_CALL(*single_field_form_fill_router_, OnGetSingleFieldSuggestions)
      .Times(2);

  GetAutofillSuggestions(form, form.fields[0]);
  EXPECT_FALSE(external_delegate_->on_suggestions_returned_seen());

  GetAutofillSuggestions(form, form.fields[1]);
  EXPECT_FALSE(external_delegate_->on_suggestions_returned_seen());
}

// Test that when small forms are disabled (min required fields enforced)
// for a form with two fields with one that has an autocomplete attribute,
// suggestions are only made for the one that has the attribute.
TEST_F(BrowserAutofillManagerTest,
       GetProfileSuggestions_MinFieldsEnforced_WithOneAutocomplete) {
  // Set up our form data.
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  FormFieldData field;
  test::CreateTestFormField("First Name", "firstname", "", "text", "given-name",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last Name", "lastname", "", "text", "", &field);
  form.fields.push_back(field);

  FormsSeen({form});

  // Check that suggestions are made for the field that has the autocomplete
  // attribute.
  GetAutofillSuggestions(form, form.fields[0]);
  CheckSuggestions(form.fields[0].global_id(), Suggestion("Charles", "", "", 1),
                   Suggestion("Elvis", "", "", 2));

  // Check that there are no suggestions for the field without the autocomplete
  // attribute.
  GetAutofillSuggestions(form, form.fields[1]);
  EXPECT_FALSE(external_delegate_->on_suggestions_returned_seen());
}

// Test that for a form with two fields with autocomplete attributes,
// suggestions are made for both fields. This is true even if a minimum number
// of fields is enforced.
TEST_F(BrowserAutofillManagerTest,
       GetProfileSuggestions_SmallFormWithTwoAutocomplete) {
  // Set up our form data.
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  FormFieldData field;
  test::CreateTestFormField("First Name", "firstname", "", "text", "given-name",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last Name", "lastname", "", "text", "family-name",
                            &field);
  form.fields.push_back(field);

  FormsSeen({form});

  GetAutofillSuggestions(form, form.fields[0]);
  CheckSuggestions(
      form.fields[0].global_id(),
      Suggestion("Charles", "Charles Hardin Holley", kAddressEntryIcon, 1),
      Suggestion("Elvis", "Elvis Aaron Presley", kAddressEntryIcon, 2));

  GetAutofillSuggestions(form, form.fields[1]);
  CheckSuggestions(
      form.fields[1].global_id(),
      Suggestion("Holley", "Charles Hardin Holley", kAddressEntryIcon, 1),
      Suggestion("Presley", "Elvis Aaron Presley", kAddressEntryIcon, 2));
}

// Test that the call is properly forwarded to its SingleFieldFormFillRouter.
TEST_F(BrowserAutofillManagerTest, OnSingleFieldSuggestionSelected) {
  std::u16string test_value = u"TestValue";
  FormData form;
  test::CreateTestAddressFormData(&form);
  FormFieldData field = form.fields[0];

  EXPECT_CALL(*single_field_form_fill_router_,
              OnSingleFieldSuggestionSelected(test_value,
                                              POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY))
      .Times(1);

  browser_autofill_manager_->OnSingleFieldSuggestionSelected(
      test_value, POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY, form, field);

  EXPECT_CALL(*single_field_form_fill_router_,
              OnSingleFieldSuggestionSelected(test_value,
                                              POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY))
      .Times(1);

  browser_autofill_manager_->OnSingleFieldSuggestionSelected(
      test_value, POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY, form, field);

  EXPECT_CALL(
      *single_field_form_fill_router_,
      OnSingleFieldSuggestionSelected(test_value, POPUP_ITEM_ID_IBAN_ENTRY))
      .Times(1);

  browser_autofill_manager_->OnSingleFieldSuggestionSelected(
      test_value, POPUP_ITEM_ID_IBAN_ENTRY, form, field);

  EXPECT_CALL(*single_field_form_fill_router_,
              OnSingleFieldSuggestionSelected(
                  test_value, POPUP_ITEM_ID_MERCHANT_PROMO_CODE_ENTRY))
      .Times(1);

  browser_autofill_manager_->OnSingleFieldSuggestionSelected(
      test_value, POPUP_ITEM_ID_MERCHANT_PROMO_CODE_ENTRY, form, field);
}

// Test that we return all address profile suggestions when all form fields
// are empty.
TEST_P(SuggestionMatchingTest, GetProfileSuggestions_EmptyValue) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  FormsSeen({form});

  GetAutofillSuggestions(form, form.fields[0]);

  std::string label1;
  std::string label2;

  switch (enabled_feature_) {
      // 23456789012 is not formatted because it is invalid for the app locale.
      // It has an extra digit.
    case EnabledFeature::kDesktop:
      label1 = MakeLabel(
          {"123 Apple St., unit 6", "23456789012", "buddy@gmail.com"});
      label2 = MakeLabel({"3734 Elvis Presley Blvd., Apt. 10", "(234) 567-8901",
                          "theking@gmail.com"});
      break;
    case EnabledFeature::kMobileShowAll:
      label1 = MakeMobileLabel(
          {"123 Apple St., unit 6", "23456789012", "buddy@gmail.com"});
      label2 = MakeMobileLabel({"3734 Elvis Presley Blvd., Apt. 10",
                                "(234) 567-8901", "theking@gmail.com"});
      break;
    case EnabledFeature::kMobileShowOne:
      label1 = "123 Apple St., unit 6";
      label2 = "3734 Elvis Presley Blvd., Apt. 10";
      break;
    case EnabledFeature::kNone:
      label1 = "123 Apple St.";
      label2 = "3734 Elvis Presley Blvd.";
  }
  // Test that we sent the right values to the external delegate.
  CheckSuggestions(form.fields[0].global_id(),
                   Suggestion("Charles", label1, kAddressEntryIcon, 1),
                   Suggestion("Elvis", label2, kAddressEntryIcon, 2));
}

// Test that we return only matching address profile suggestions when the
// selected form field has been partially filled out.
TEST_P(SuggestionMatchingTest, GetProfileSuggestions_MatchCharacter) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  FormsSeen({form});

  FormFieldData field;
  test::CreateTestFormField("First Name", "firstname", "E", "text", &field);
  GetAutofillSuggestions(form, field);

  std::string label;

  switch (enabled_feature_) {
    case EnabledFeature::kDesktop:
    case EnabledFeature::kMobileShowAll:
    case EnabledFeature::kMobileShowOne:
      label = "3734 Elvis Presley Blvd., Apt. 10";
      break;
    case EnabledFeature::kNone:
      label = "3734 Elvis Presley Blvd.";
  }
  // Test that we sent the right values to the external delegate.
  CheckSuggestions(field.global_id(),
                   Suggestion("Elvis", label, kAddressEntryIcon, 1));
}

// Tests that we return address profile suggestions values when the section
// is already autofilled, and that we merge identical values.
TEST_P(SuggestionMatchingTest,
       GetProfileSuggestions_AlreadyAutofilledMergeValues) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  FormsSeen({form});

  // First name is already autofilled which will make the section appear as
  // "already autofilled".
  form.fields[0].is_autofilled = true;

  // Two profiles have the same last name, and the third shares the same first
  // letter for last name.
  AutofillProfile profile1;
  profile1.set_guid("00000000-0000-0000-0000-000000000103");
  profile1.SetInfo(NAME_FIRST, u"Robin", "en-US");
  profile1.SetInfo(NAME_LAST, u"Grimes", "en-US");
  profile1.SetInfo(ADDRESS_HOME_LINE1, u"1234 Smith Blvd.", "en-US");
  personal_data().AddProfile(profile1);

  AutofillProfile profile2;
  profile2.set_guid("00000000-0000-0000-0000-000000000124");
  profile2.SetInfo(NAME_FIRST, u"Carl", "en-US");
  profile2.SetInfo(NAME_LAST, u"Grimes", "en-US");
  profile2.SetInfo(ADDRESS_HOME_LINE1, u"1234 Smith Blvd.", "en-US");
  personal_data().AddProfile(profile2);

  AutofillProfile profile3;
  profile3.set_guid("00000000-0000-0000-0000-000000000126");
  profile3.SetInfo(NAME_FIRST, u"Aaron", "en-US");
  profile3.SetInfo(NAME_LAST, u"Googler", "en-US");
  profile3.SetInfo(ADDRESS_HOME_LINE1, u"1600 Amphitheater pkwy", "en-US");
  personal_data().AddProfile(profile3);

  FormFieldData field;
  test::CreateTestFormField("Last Name", "lastname", "G", "text", &field);
  GetAutofillSuggestions(form, field);

  switch (enabled_feature_) {
    case EnabledFeature::kDesktop:
    case EnabledFeature::kMobileShowAll:
    case EnabledFeature::kMobileShowOne:
      CheckSuggestions(
          field.global_id(),
          Suggestion("Googler", "1600 Amphitheater pkwy", kAddressEntryIcon, 1),
          Suggestion("Grimes", "1234 Smith Blvd.", kAddressEntryIcon, 2));
      break;
    case EnabledFeature::kNone:
      // Test that we sent the right values to the external delegate. No labels
      // with duplicate values "Grimes" merged.
      CheckSuggestions(
          field.global_id(),
          Suggestion("Googler", "1600 Amphitheater pkwy", kAddressEntryIcon, 1),
          Suggestion("Grimes", "1234 Smith Blvd., Carl Grimes",
                     kAddressEntryIcon, 2),
          Suggestion("Grimes", "1234 Smith Blvd., Robin Grimes",
                     kAddressEntryIcon, 3));
  }
}

// Tests that we return address profile suggestions values when the section
// is already autofilled, and that they have no label.
TEST_P(SuggestionMatchingTest,
       GetProfileSuggestions_AlreadyAutofilledNoLabels) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  FormsSeen({form});

  // First name is already autofilled which will make the section appear as
  // "already autofilled".
  form.fields[0].is_autofilled = true;

  FormFieldData field;
  test::CreateTestFormField("First Name", "firstname", "E", "text", &field);
  GetAutofillSuggestions(form, field);

  std::string label;

  switch (enabled_feature_) {
    case EnabledFeature::kDesktop:
    case EnabledFeature::kMobileShowAll:
    case EnabledFeature::kMobileShowOne:
      label = "3734 Elvis Presley Blvd., Apt. 10";
      break;
    case EnabledFeature::kNone:
      label = "3734 Elvis Presley Blvd.";
  }
  // Test that we sent the right values to the external delegate.
  CheckSuggestions(field.global_id(),
                   Suggestion("Elvis", label, kAddressEntryIcon, 1));
}

// Test that we return no suggestions when the form has no relevant fields.
TEST_F(BrowserAutofillManagerTest, GetProfileSuggestions_UnknownFields) {
  // Set up our form data.
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");

  FormFieldData field;
  test::CreateTestFormField("Username", "username", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Password", "password", "", "password", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Quest", "quest", "", "quest", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Color", "color", "", "text", &field);
  form.fields.push_back(field);

  FormsSeen({form});

  GetAutofillSuggestions(form, field);
  EXPECT_FALSE(external_delegate_->on_suggestions_returned_seen());
}

// Test that we cull duplicate profile suggestions.
TEST_P(SuggestionMatchingTest, GetProfileSuggestions_WithDuplicates) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  FormsSeen({form});

  // Add a duplicate profile.
  AutofillProfile duplicate_profile =
      *personal_data().GetProfileByGUID("00000000-0000-0000-0000-000000000001");
  personal_data().AddProfile(duplicate_profile);

  GetAutofillSuggestions(form, form.fields[0]);

  std::string label1;
  std::string label2;

  switch (enabled_feature_) {
      // 23456789012 is not formatted because it is invalid for the app locale.
      // It has an extra digit.
    case EnabledFeature::kDesktop:
      label1 = MakeLabel(
          {"123 Apple St., unit 6", "23456789012", "buddy@gmail.com"});
      label2 = MakeLabel({"3734 Elvis Presley Blvd., Apt. 10", "(234) 567-8901",
                          "theking@gmail.com"});
      break;
    case EnabledFeature::kMobileShowAll:
      label1 = MakeMobileLabel(
          {"123 Apple St., unit 6", "23456789012", "buddy@gmail.com"});
      label2 = MakeMobileLabel({"3734 Elvis Presley Blvd., Apt. 10",
                                "(234) 567-8901", "theking@gmail.com"});
      break;
    case EnabledFeature::kMobileShowOne:
      label1 = "123 Apple St., unit 6";
      label2 = "3734 Elvis Presley Blvd., Apt. 10";
      break;
    case EnabledFeature::kNone:
      label1 = "123 Apple St.";
      label2 = "3734 Elvis Presley Blvd.";
  }
  // Test that we sent the right values to the external delegate.
  CheckSuggestions(form.fields[0].global_id(),
                   Suggestion("Charles", label1, kAddressEntryIcon, 1),
                   Suggestion("Elvis", label2, kAddressEntryIcon, 2));
}

// Test that we return no suggestions when autofill is disabled.
TEST_F(BrowserAutofillManagerTest,
       GetProfileSuggestions_AutofillDisabledByUser) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  FormsSeen({form});

  // Disable Autofill.
  browser_autofill_manager_->SetAutofillProfileEnabled(autofill_client_, false);
  browser_autofill_manager_->SetAutofillCreditCardEnabled(autofill_client_,
                                                          false);

  GetAutofillSuggestions(form, form.fields[0]);
  EXPECT_FALSE(external_delegate_->on_suggestions_returned_seen());
}

TEST_F(BrowserAutofillManagerTest,
       OnSuggestionsReturned_CallsExternalDelegate) {
  FieldGlobalId field_id = test::MakeFieldGlobalId();
  std::vector<Suggestion> suggestions = {
      Suggestion("Charles", "123 Apple St.", "", 1),
      Suggestion("Elvis", "3734 Elvis Presley Blvd.", "", 2)};

  {
    browser_autofill_manager_->OnSuggestionsReturned(
        field_id, AutoselectFirstSuggestion(false), suggestions);

    EXPECT_FALSE(external_delegate_->autoselect_first_suggestion());
    CheckSuggestions(field_id, suggestions[0], suggestions[1]);
  }
  {
    browser_autofill_manager_->OnSuggestionsReturned(
        field_id, AutoselectFirstSuggestion(true), suggestions);

    EXPECT_TRUE(external_delegate_->autoselect_first_suggestion());
    CheckSuggestions(field_id, suggestions[0], suggestions[1]);
  }
}

// Test that we return all credit card profile suggestions when all form fields
// are empty.
TEST_F(BrowserAutofillManagerTest, GetCreditCardSuggestions_EmptyValue) {
  // Set up our form data.
  FormData form = CreateTestCreditCardFormData(true, false);
  FormsSeen({form});

  GetAutofillSuggestions(form, form.fields[1]);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  const std::string visa_label = std::string("04/99");
  const std::string master_card_label = std::string("10/98");
#else
  const std::string visa_label = std::string("Expires on 04/99");
  const std::string master_card_label = std::string("Expires on 10/98");
#endif

  // Test that we sent the credit card suggestions to the external delegate.
  CheckSuggestions(
      form.fields[1].global_id(),
      Suggestion(
          std::string("Visa  ") + test::ObfuscatedCardDigitsAsUTF8("3456"),
          visa_label, kVisaCard,
          browser_autofill_manager_->GetPackedCreditCardID(4)),
      Suggestion(std::string("Mastercard  ") +
                     test::ObfuscatedCardDigitsAsUTF8("8765"),
                 master_card_label, kMasterCard,
                 browser_autofill_manager_->GetPackedCreditCardID(5)));
}

// Test that we return all credit card profile suggestions when the triggering
// field has whitespace in it.
TEST_F(BrowserAutofillManagerTest, GetCreditCardSuggestions_Whitespace) {
  // Set up our form data.
  FormData form = CreateTestCreditCardFormData(true, false);
  FormsSeen({form});

  FormFieldData field = form.fields[1];
  field.value = u"       ";
  GetAutofillSuggestions(form, field);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  const std::string visa_label = std::string("04/99");
  const std::string master_card_label = std::string("10/98");
#else
  const std::string visa_label = std::string("Expires on 04/99");
  const std::string master_card_label = std::string("Expires on 10/98");
#endif

  // Test that we sent the right values to the external delegate.
  CheckSuggestions(
      field.global_id(),
      Suggestion(
          std::string("Visa  ") + test::ObfuscatedCardDigitsAsUTF8("3456"),
          visa_label, kVisaCard,
          browser_autofill_manager_->GetPackedCreditCardID(4)),
      Suggestion(std::string("Mastercard  ") +
                     test::ObfuscatedCardDigitsAsUTF8("8765"),
                 master_card_label, kMasterCard,
                 browser_autofill_manager_->GetPackedCreditCardID(5)));
}

// Test that we return all credit card profile suggestions when the triggering
// field has stop characters in it, which should be removed.
TEST_F(BrowserAutofillManagerTest, GetCreditCardSuggestions_StopCharsOnly) {
  // Set up our form data.
  FormData form = CreateTestCreditCardFormData(true, false);
  FormsSeen({form});

  FormFieldData field = form.fields[1];
  field.value = u"____-____-____-____";
  GetAutofillSuggestions(form, field);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  const std::string visa_label = std::string("04/99");
  const std::string master_card_label = std::string("10/98");
#else
  const std::string visa_label = std::string("Expires on 04/99");
  const std::string master_card_label = std::string("Expires on 10/98");
#endif

  // Test that we sent the right values to the external delegate.
  CheckSuggestions(
      field.global_id(),
      Suggestion(
          std::string("Visa  ") + test::ObfuscatedCardDigitsAsUTF8("3456"),
          visa_label, kVisaCard,
          browser_autofill_manager_->GetPackedCreditCardID(4)),
      Suggestion(std::string("Mastercard  ") +
                     test::ObfuscatedCardDigitsAsUTF8("8765"),
                 master_card_label, kMasterCard,
                 browser_autofill_manager_->GetPackedCreditCardID(5)));
}

// Test that we return all credit card profile suggestions when the triggering
// field has some invisible unicode characters in it.
TEST_F(BrowserAutofillManagerTest,
       GetCreditCardSuggestions_InvisibleUnicodeOnly) {
  // Set up our form data.
  FormData form = CreateTestCreditCardFormData(true, false);
  FormsSeen({form});

  FormFieldData field = form.fields[1];
  field.value = std::u16string({0x200E, 0x200F});
  GetAutofillSuggestions(form, field);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  const std::string visa_label = std::string("04/99");
  const std::string master_card_label = std::string("10/98");
#else
  const std::string visa_label = std::string("Expires on 04/99");
  const std::string master_card_label = std::string("Expires on 10/98");
#endif

  // Test that we sent the right values to the external delegate.
  CheckSuggestions(
      field.global_id(),
      Suggestion(
          std::string("Visa  ") + test::ObfuscatedCardDigitsAsUTF8("3456"),
          visa_label, kVisaCard,
          browser_autofill_manager_->GetPackedCreditCardID(4)),
      Suggestion(std::string("Mastercard  ") +
                     test::ObfuscatedCardDigitsAsUTF8("8765"),
                 master_card_label, kMasterCard,
                 browser_autofill_manager_->GetPackedCreditCardID(5)));
}

// Test that we return all credit card profile suggestions when the triggering
// field has stop characters in it and some input.
TEST_F(BrowserAutofillManagerTest,
       GetCreditCardSuggestions_StopCharsWithInput) {
  // Add a credit card with particular numbers that we will attempt to recall.
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "John Smith",
                          "5255667890123123",  // Mastercard
                          "08", "2017", "1");
  credit_card.set_guid("00000000-0000-0000-0000-000000000007");
  personal_data().AddCreditCard(credit_card);

  // Set up our form data.
  FormData form = CreateTestCreditCardFormData(true, false);
  FormsSeen({form});

  FormFieldData field = form.fields[1];
  field.value = u"5255-66__-____-____";
  GetAutofillSuggestions(form, field);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  const std::string master_card_label = std::string("08/17");
#else
  const std::string master_card_label = std::string("Expires on 08/17");
#endif

  // Test that we sent the right value to the external delegate.
  CheckSuggestions(
      field.global_id(),
      Suggestion(std::string("Mastercard  ") +
                     test::ObfuscatedCardDigitsAsUTF8("3123"),
                 master_card_label, kMasterCard,
                 browser_autofill_manager_->GetPackedCreditCardID(7)));
}

// Test that we return only matching credit card profile suggestions when the
// selected form field has been partially filled out.
TEST_F(BrowserAutofillManagerTest, GetCreditCardSuggestions_MatchCharacter) {
  // Set up our form data.
  FormData form = CreateTestCreditCardFormData(true, false);
  FormsSeen({form});

  FormFieldData field;
  test::CreateTestFormField("Card Number", "cardnumber", "78", "text", &field);
  GetAutofillSuggestions(form, field);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  const std::string visa_label = std::string("04/99");
#else
  const std::string visa_label = std::string("Expires on 04/99");
#endif

  // Test that we sent the right values to the external delegate.
  CheckSuggestions(
      field.global_id(),
      Suggestion(
          std::string("Visa  ") + test::ObfuscatedCardDigitsAsUTF8("3456"),
          visa_label, kVisaCard,
          browser_autofill_manager_->GetPackedCreditCardID(4)));
}

// Test that we return credit card profile suggestions when the selected form
// field is the credit card number field.
TEST_P(CreditCardSuggestionTest, GetCreditCardSuggestions_CCNumber) {
  // Set nickname with the corresponding guid of the Mastercard 8765.
  personal_data().SetNicknameForCardWithGUID(
      "00000000-0000-0000-0000-000000000005", kArbitraryNickname);
  // Set up our form data.
  FormData form = CreateTestCreditCardFormData(true, false);
  FormsSeen({form});

  const FormFieldData& credit_card_number_field = form.fields[1];
  GetAutofillSuggestions(form, credit_card_number_field);
  const std::string visa_value =
      std::string("Visa  ") +
      test::ObfuscatedCardDigitsAsUTF8("3456", ObfuscationLength());
  // Mastercard has a valid nickname. Display nickname + last four in the
  // suggestion title.
  const std::string master_card_value =
      kArbitraryNickname + "  " +
      test::ObfuscatedCardDigitsAsUTF8("8765", ObfuscationLength());

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  const std::string visa_label = std::string("04/99");
  const std::string master_card_label = std::string("10/98");
#else
  const std::string visa_label = std::string("Expires on 04/99");
  const std::string master_card_label = std::string("Expires on 10/98");
#endif

  // Test that we sent the right values to the external delegate.
  CheckSuggestions(
      credit_card_number_field.global_id(),
      Suggestion(visa_value, visa_label, kVisaCard,
                 browser_autofill_manager_->GetPackedCreditCardID(4)),
      Suggestion(master_card_value, master_card_label, kMasterCard,
                 browser_autofill_manager_->GetPackedCreditCardID(5)));
}

// Test that we return credit card profile suggestions when the selected form
// field is not the credit card number field.
TEST_P(CreditCardSuggestionTest, GetCreditCardSuggestions_NonCCNumber) {
  // Set nickname with the corresponding guid of the Mastercard 8765.
  personal_data().SetNicknameForCardWithGUID(
      "00000000-0000-0000-0000-000000000005", kArbitraryNickname);
  // Set up our form data.
  FormData form = CreateTestCreditCardFormData(true, false);
  FormsSeen({form});

  const FormFieldData& cardholder_name_field = form.fields[0];
  GetAutofillSuggestions(form, cardholder_name_field);

  const std::string obfuscated_last_four_digits1 =
      test::ObfuscatedCardDigitsAsUTF8("3456", ObfuscationLength());
  const std::string obfuscated_last_four_digits2 =
      test::ObfuscatedCardDigitsAsUTF8("8765", ObfuscationLength());

#if BUILDFLAG(IS_ANDROID)
  // For Android, when keyboard accessary is enabled, always show obfuscated
  // last four. When keyboard accessary is not enabled (drop-down suggestion):
  // 1) if nickname feature is enabled and nickname is available, show nickname
  // + last four. 2) Otherwise, show network + last four.
  // Visa card does not have a nickname.
  const std::string visa_label =
      IsKeyboardAccessoryEnabled()
          ? obfuscated_last_four_digits1
          : std::string("Visa  ") + obfuscated_last_four_digits1;
  // Mastercard has a valid nickname.
  const std::string master_card_label =
      IsKeyboardAccessoryEnabled()
          ? obfuscated_last_four_digits2
          : kArbitraryNickname + "  " + obfuscated_last_four_digits2;

#elif BUILDFLAG(IS_IOS)
  const std::string visa_label = obfuscated_last_four_digits1;
  const std::string master_card_label = obfuscated_last_four_digits2;

#else
  // If no nickname available, we will show network.
  const std::string visa_label = base::JoinString(
      {"Visa  ", obfuscated_last_four_digits1, ", expires on 04/99"}, "");
  // When nickname is available, show nickname. Otherwise, show network.
  const std::string master_card_label =
      base::JoinString({kArbitraryNickname + "  ", obfuscated_last_four_digits2,
                        ", expires on 10/98"},
                       "");
#endif

  // Test that we sent the right values to the external delegate.
  CheckSuggestions(
      cardholder_name_field.global_id(),
      Suggestion("Elvis Presley", visa_label, kVisaCard,
                 browser_autofill_manager_->GetPackedCreditCardID(4)),
      Suggestion("Buddy Holly", master_card_label, kMasterCard,
                 browser_autofill_manager_->GetPackedCreditCardID(5)));
}

// Test that we return a warning explaining that credit card profile suggestions
// are unavailable when the page is secure, but the form action URL is valid but
// not secure.
TEST_F(BrowserAutofillManagerTest,
       GetCreditCardSuggestions_SecureContext_FormActionNotHTTPS) {
  // Set up our form data.
  FormData form = CreateTestCreditCardFormData(/*is_https=*/true, false);
  // However we set the action (target URL) to be HTTP after all.
  form.action = GURL("http://myform.com/submit.html");
  FormsSeen({form});

  GetAutofillSuggestions(form, form.fields[0]);

  // Test that we sent the right values to the external delegate.
  CheckSuggestions(
      form.fields[0].global_id(),
      Suggestion(l10n_util::GetStringUTF8(IDS_AUTOFILL_WARNING_MIXED_FORM), "",
                 "", -26));

  // Clear the test credit cards and try again -- we should still show the
  // mixed form warning.
  personal_data().ClearCreditCards();
  GetAutofillSuggestions(form, form.fields[0]);
  CheckSuggestions(
      form.fields[0].global_id(),
      Suggestion(l10n_util::GetStringUTF8(IDS_AUTOFILL_WARNING_MIXED_FORM), "",
                 "", -26));
}

// Test that we return credit card suggestions for secure pages that have an
// empty form action target URL.
TEST_F(BrowserAutofillManagerTest,
       GetCreditCardSuggestions_SecureContext_EmptyFormAction) {
  // Set up our form data.
  FormData form = CreateTestCreditCardFormData(true, false);
  // Clear the form action.
  form.action = GURL();
  FormsSeen({form});

  GetAutofillSuggestions(form, form.fields[1]);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  const std::string visa_label = std::string("04/99");
  const std::string master_card_label = std::string("10/98");
#else
  const std::string visa_label = std::string("Expires on 04/99");
  const std::string master_card_label = std::string("Expires on 10/98");
#endif

  // Test that we sent the right values to the external delegate.
  CheckSuggestions(
      form.fields[1].global_id(),
      Suggestion(
          std::string("Visa  ") + test::ObfuscatedCardDigitsAsUTF8("3456"),
          visa_label, kVisaCard,
          browser_autofill_manager_->GetPackedCreditCardID(4)),
      Suggestion(std::string("Mastercard  ") +
                     test::ObfuscatedCardDigitsAsUTF8("8765"),
                 master_card_label, kMasterCard,
                 browser_autofill_manager_->GetPackedCreditCardID(5)));
}

// Test that we return credit card suggestions for secure pages that have a
// form action set to "javascript:something".
TEST_F(BrowserAutofillManagerTest,
       GetCreditCardSuggestions_SecureContext_JavascriptFormAction) {
  // Set up our form data.
  FormData form = CreateTestCreditCardFormData(true, false);
  // Have the form action be a javascript function (which is a valid URL).
  form.action = GURL("javascript:alert('Hello');");
  FormsSeen({form});

  GetAutofillSuggestions(form, form.fields[1]);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  const std::string visa_label = std::string("04/99");
  const std::string master_card_label = std::string("10/98");
#else
  const std::string visa_label = std::string("Expires on 04/99");
  const std::string master_card_label = std::string("Expires on 10/98");
#endif

  // Test that we sent the right values to the external delegate.
  CheckSuggestions(
      form.fields[1].global_id(),
      Suggestion(
          std::string("Visa  ") + test::ObfuscatedCardDigitsAsUTF8("3456"),
          visa_label, kVisaCard,
          browser_autofill_manager_->GetPackedCreditCardID(4)),
      Suggestion(std::string("Mastercard  ") +
                     test::ObfuscatedCardDigitsAsUTF8("8765"),
                 master_card_label, kMasterCard,
                 browser_autofill_manager_->GetPackedCreditCardID(5)));
}

// Test that we return all credit card suggestions in the case that two cards
// have the same obfuscated number.
TEST_F(BrowserAutofillManagerTest,
       GetCreditCardSuggestions_RepeatedObfuscatedNumber) {
  // Add a credit card with the same obfuscated number as Elvis's.
  // |credit_card| will be owned by the mock PersonalDataManager.
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "Elvis Presley",
                          "5231567890123456",  // Mastercard
                          "05", "2999", "1");
  credit_card.set_guid("00000000-0000-0000-0000-000000000007");
  credit_card.set_use_date(AutofillClock::Now() - base::Days(15));
  personal_data().AddCreditCard(credit_card);

  // Set up our form data.
  FormData form = CreateTestCreditCardFormData(true, false);
  FormsSeen({form});

  GetAutofillSuggestions(form, form.fields[1]);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  const std::string visa_label = std::string("04/99");
  const std::string master_card_label1 = std::string("10/98");
  const std::string master_card_label2 = std::string("05/99");
#else
  const std::string visa_label = std::string("Expires on 04/99");
  const std::string master_card_label1 = std::string("Expires on 10/98");
  const std::string master_card_label2 = std::string("Expires on 05/99");
#endif

  // Test that we sent the right values to the external delegate.
  CheckSuggestions(
      form.fields[1].global_id(),
      Suggestion(
          std::string("Visa  ") + test::ObfuscatedCardDigitsAsUTF8("3456"),
          visa_label, kVisaCard,
          browser_autofill_manager_->GetPackedCreditCardID(4)),
      Suggestion(std::string("Mastercard  ") +
                     test::ObfuscatedCardDigitsAsUTF8("8765"),
                 master_card_label1, kMasterCard,
                 browser_autofill_manager_->GetPackedCreditCardID(5)),
      Suggestion(std::string("Mastercard  ") +
                     test::ObfuscatedCardDigitsAsUTF8("3456"),
                 master_card_label2, kMasterCard,
                 browser_autofill_manager_->GetPackedCreditCardID(7)));
}

// Test that a masked server card is not suggested if more that six digits
// have been typed in the field.
TEST_F(BrowserAutofillManagerTest,
       GetCreditCardSuggestions_MaskedCardWithMoreThan6Digits) {
  // Add a masked server card.
  personal_data().ClearCreditCards();

  CreditCard masked_server_card;
  test::SetCreditCardInfo(&masked_server_card, "Elvis Presley",
                          "4234567890123456",  // Visa
                          "04", "2999", "1");
  masked_server_card.set_guid("00000000-0000-0000-0000-000000000007");
  masked_server_card.set_record_type(CreditCard::MASKED_SERVER_CARD);
  personal_data().AddServerCreditCard(masked_server_card);
  EXPECT_EQ(1U, personal_data().GetCreditCards().size());

  // Set up our form data.
  FormData form = CreateTestCreditCardFormData(true, false);
  FormsSeen({form});

  FormFieldData field = form.fields[1];
  field.value = u"12345678";
  GetAutofillSuggestions(form, field);

  external_delegate_->CheckNoSuggestions(field.global_id());
}

// Test that expired cards are ordered by their ranking score and are always
// suggested after non expired cards even if they have a higher ranking score.
TEST_F(BrowserAutofillManagerTest, GetCreditCardSuggestions_ExpiredCards) {
  personal_data().ClearCreditCards();

  // Add a never used non expired credit card.
  CreditCard credit_card0("002149C1-EE28-4213-A3B9-DA243FFF021B",
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card0, "Bonnie Parker",
                          "5105105105105100" /* Mastercard */, "04", "2055",
                          "1");
  credit_card0.set_guid("00000000-0000-0000-0000-000000000001");
  personal_data().AddCreditCard(credit_card0);

  // Add an expired card with a higher ranking score.
  CreditCard credit_card1("287151C8-6AB1-487C-9095-28E80BE5DA15",
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card1, "Clyde Barrow",
                          "378282246310005" /* American Express */, "04",
                          "2010", "1");
  credit_card1.set_guid("00000000-0000-0000-0000-000000000002");
  credit_card1.set_use_count(300);
  credit_card1.set_use_date(AutofillClock::Now() - base::Days(10));
  personal_data().AddCreditCard(credit_card1);

  // Add an expired card with a lower ranking score.
  CreditCard credit_card2("1141084B-72D7-4B73-90CF-3D6AC154673B",
                          test::kEmptyOrigin);
  credit_card2.set_use_count(3);
  credit_card2.set_use_date(AutofillClock::Now() - base::Days(1));
  test::SetCreditCardInfo(&credit_card2, "John Dillinger",
                          "4234567890123456" /* Visa */, "01", "2011", "1");
  credit_card2.set_guid("00000000-0000-0000-0000-000000000003");
  personal_data().AddCreditCard(credit_card2);

  ASSERT_EQ(3U, personal_data().GetCreditCards().size());

  // Set up our form data.
  FormData form = CreateTestCreditCardFormData(true, false);
  FormsSeen({form});
  GetAutofillSuggestions(form, form.fields[1]);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  const std::string visa_label = std::string("01/11");
  const std::string master_card_label = std::string("04/55");
  const std::string amex_card_label = std::string("04/10");
#else
  const std::string visa_label = std::string("Expires on 01/11");
  const std::string master_card_label = std::string("Expires on 04/55");
  const std::string amex_card_label = std::string("Expires on 04/10");
#endif

  CheckSuggestions(
      form.fields[1].global_id(),
      Suggestion(std::string("Mastercard  ") +
                     test::ObfuscatedCardDigitsAsUTF8("5100"),
                 master_card_label, kMasterCard,
                 browser_autofill_manager_->GetPackedCreditCardID(1)),
      Suggestion(
          std::string("Amex  ") + test::ObfuscatedCardDigitsAsUTF8("0005"),
          amex_card_label, kAmericanExpressCard,
          browser_autofill_manager_->GetPackedCreditCardID(2)),
      Suggestion(
          std::string("Visa  ") + test::ObfuscatedCardDigitsAsUTF8("3456"),
          visa_label, kVisaCard,
          browser_autofill_manager_->GetPackedCreditCardID(3)));
}

// Test cards that are expired AND disused are suppressed when suppression is
// enabled and the input field is empty.
TEST_F(BrowserAutofillManagerTest,
       GetCreditCardSuggestions_SuppressDisusedCreditCardsOnEmptyField) {
  personal_data().ClearCreditCards();
  ASSERT_EQ(0U, personal_data().GetCreditCards().size());

  // Add a never used non expired local credit card.
  CreditCard credit_card0("00000000-0000-0000-0000-000000000000",
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card0, "Bonnie Parker",
                          "5105105105105100" /* Mastercard */, "04", "2999",
                          "1");
  personal_data().AddCreditCard(credit_card0);

  auto now = AutofillClock::Now();

  // Add an expired local card last used 10 days ago
  CreditCard credit_card1("00000000-0000-0000-0000-000000000001",
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card1, "Clyde Barrow",
                          "4234567890123456" /* Visa */, "04", "2010", "1");
  credit_card1.set_use_date(now - base::Days(10));
  personal_data().AddCreditCard(credit_card1);

  // Add an expired local card last used 180 days ago.
  CreditCard credit_card2("00000000-0000-0000-0000-000000000002",
                          test::kEmptyOrigin);
  credit_card2.set_use_date(now - base::Days(182));
  test::SetCreditCardInfo(&credit_card2, "John Dillinger",
                          "378282246310005" /* American Express */, "01",
                          "2010", "1");
  personal_data().AddCreditCard(credit_card2);

  ASSERT_EQ(3U, personal_data().GetCreditCards().size());

  // Set up our form data.
  FormData form = CreateTestCreditCardFormData(true, false);
  FormsSeen({form});

  // Query with empty string only returns card0 and card1. Note expired
  // masked card2 is not suggested on empty fields.
  {
    GetAutofillSuggestions(form, form.fields[0]);

#if BUILDFLAG(IS_ANDROID)
    const std::string mastercard_label =
        std::string("Mastercard  ") + test::ObfuscatedCardDigitsAsUTF8("5100");
    const std::string visa_label =
        std::string("Visa  ") + test::ObfuscatedCardDigitsAsUTF8("3456");
#elif BUILDFLAG(IS_IOS)
    const std::string mastercard_label =
        test::ObfuscatedCardDigitsAsUTF8("5100");
    const std::string visa_label = test::ObfuscatedCardDigitsAsUTF8("3456");
#else
    const std::string mastercard_label =
        std::string("Mastercard  ") + test::ObfuscatedCardDigitsAsUTF8("5100") +
        std::string(", expires on 04/99");
    const std::string visa_label = std::string("Visa  ") +
                                   test::ObfuscatedCardDigitsAsUTF8("3456") +
                                   std::string(", expires on 04/10");
#endif

    CheckSuggestions(
        form.fields[0].global_id(),
        Suggestion("Bonnie Parker", mastercard_label, kMasterCard,
                   browser_autofill_manager_->GetPackedCreditCardID(0)),
        Suggestion("Clyde Barrow", visa_label, kVisaCard,
                   browser_autofill_manager_->GetPackedCreditCardID(1)));
  }

  // Query with name prefix for card0 returns card0.
  {
    FormFieldData field = form.fields[0];
    field.value = u"B";
    GetAutofillSuggestions(form, field);

#if BUILDFLAG(IS_ANDROID)
    const std::string mastercard_label =
        std::string("Mastercard  ") + test::ObfuscatedCardDigitsAsUTF8("5100");
#elif BUILDFLAG(IS_IOS)
    const std::string mastercard_label =
        test::ObfuscatedCardDigitsAsUTF8("5100");
#else
    const std::string mastercard_label =
        std::string("Mastercard  ") + test::ObfuscatedCardDigitsAsUTF8("5100") +
        std::string(", expires on 04/99");
#endif

    CheckSuggestions(
        form.fields[0].global_id(),
        Suggestion("Bonnie Parker", mastercard_label, kMasterCard,
                   browser_autofill_manager_->GetPackedCreditCardID(0)));
  }

  // Query with name prefix for card1 returns card1.
  {
    FormFieldData field = form.fields[0];
    field.value = u"Cl";
    GetAutofillSuggestions(form, field);

#if BUILDFLAG(IS_ANDROID)
    const std::string visa_label =
        std::string("Visa  ") + test::ObfuscatedCardDigitsAsUTF8("3456");
#elif BUILDFLAG(IS_IOS)
    const std::string visa_label = test::ObfuscatedCardDigitsAsUTF8("3456");
#else
    const std::string visa_label = std::string("Visa  ") +
                                   test::ObfuscatedCardDigitsAsUTF8("3456") +
                                   std::string(", expires on 04/10");
#endif

    CheckSuggestions(
        form.fields[0].global_id(),
        Suggestion("Clyde Barrow", visa_label, kVisaCard,
                   browser_autofill_manager_->GetPackedCreditCardID(1)));
  }

  // Query with name prefix for card2 returns card2.
  {
    FormFieldData field = form.fields[0];
    field.value = u"Jo";
    GetAutofillSuggestions(form, field);

#if BUILDFLAG(IS_ANDROID)
    const std::string amex_label =
        std::string("Amex  ") + test::ObfuscatedCardDigitsAsUTF8("0005");
#elif BUILDFLAG(IS_IOS)
    const std::string amex_label = test::ObfuscatedCardDigitsAsUTF8("0005");
#else
    const std::string amex_label = std::string("Amex  ") +
                                   test::ObfuscatedCardDigitsAsUTF8("0005") +
                                   std::string(", expires on 01/10");
#endif

    CheckSuggestions(
        form.fields[0].global_id(),
        Suggestion("John Dillinger", amex_label, kAmericanExpressCard,
                   browser_autofill_manager_->GetPackedCreditCardID(2)));
  }
}

// Test that a card that doesn't have a number is not shown in the
// suggestions when querying credit cards by their number, and is shown when
// querying other fields.
TEST_F(BrowserAutofillManagerTest, GetCreditCardSuggestions_NumberMissing) {
  // Create one normal credit card and one credit card with the number
  // missing.
  personal_data().ClearCreditCards();
  ASSERT_EQ(0U, personal_data().GetCreditCards().size());

  CreditCard credit_card0("287151C8-6AB1-487C-9095-28E80BE5DA15",
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card0, "Clyde Barrow",
                          "378282246310005" /* American Express */, "04",
                          "2999", "1");
  credit_card0.set_guid("00000000-0000-0000-0000-000000000001");
  personal_data().AddCreditCard(credit_card0);

  CreditCard credit_card1("1141084B-72D7-4B73-90CF-3D6AC154673B",
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card1, "John Dillinger", "", "01", "2999",
                          "1");
  credit_card1.set_guid("00000000-0000-0000-0000-000000000002");
  personal_data().AddCreditCard(credit_card1);

  ASSERT_EQ(2U, personal_data().GetCreditCards().size());

  // Set up our form data.
  FormData form = CreateTestCreditCardFormData(true, false);
  FormsSeen({form});

  // Query by card number field.
  GetAutofillSuggestions(form, form.fields[1]);

// Sublabel is expiration date when filling card number. The second card
// doesn't have a number so it should not be included in the suggestions.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  const std::string amex_card_exp_label = std::string("04/99");
#else
  const std::string amex_card_exp_label = std::string("Expires on 04/99");
#endif

  CheckSuggestions(
      form.fields[1].global_id(),
      Suggestion(
          std::string("Amex  ") + test::ObfuscatedCardDigitsAsUTF8("0005"),
          amex_card_exp_label, kAmericanExpressCard,
          browser_autofill_manager_->GetPackedCreditCardID(1)));

  // Query by cardholder name field.
  GetAutofillSuggestions(form, form.fields[0]);

#if BUILDFLAG(IS_ANDROID)
  const std::string amex_card_label =
      std::string("Amex  ") + test::ObfuscatedCardDigitsAsUTF8("0005");
#elif BUILDFLAG(IS_IOS)
  const std::string amex_card_label = test::ObfuscatedCardDigitsAsUTF8("0005");
#else
  const std::string amex_card_label = std::string("Amex  ") +
                                      test::ObfuscatedCardDigitsAsUTF8("0005") +
                                      std::string(", expires on 04/99");
#endif

  CheckSuggestions(
      form.fields[0].global_id(),
      Suggestion("John Dillinger", "", kGenericCard,
                 browser_autofill_manager_->GetPackedCreditCardID(2)),
      Suggestion("Clyde Barrow", amex_card_label, kAmericanExpressCard,
                 browser_autofill_manager_->GetPackedCreditCardID(1)));
}

TEST_F(BrowserAutofillManagerTest, OnCreditCardFetched_StoreInstrumentId) {
  FormData form = CreateTestCreditCardFormData(true, false);
  FormsSeen({form});
  CreditCard credit_card = test::GetMaskedServerCard();
  browser_autofill_manager_->FillOrPreviewCreditCardForm(
      mojom::RendererFormDataAction::kFill, form, form.fields[0], &credit_card,
      AutofillTriggerSource::kPopup);

  browser_autofill_manager_->OnCreditCardFetchedForTest(
      CreditCardFetchResult::kSuccess, &credit_card,
      /*cvc=*/u"123");

  ASSERT_TRUE(
      test_form_data_importer_->fetched_card_instrument_id().has_value());
  EXPECT_EQ(test_form_data_importer_->fetched_card_instrument_id().value(),
            credit_card.instrument_id());
}

// Test that we return profile and credit card suggestions for combined forms.
TEST_P(SuggestionMatchingTest, GetAddressAndCreditCardSuggestions) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  CreateTestCreditCardFormData(&form, true, false);
  FormsSeen({form});

  GetAutofillSuggestions(form, form.fields[0]);

  std::string label1;
  std::string label2;

  switch (enabled_feature_) {
    case EnabledFeature::kDesktop:
      // 23456789012 is not formatted because it is invalid for the app locale.
      // It has an extra digit.
      label1 = MakeLabel(
          {"123 Apple St., unit 6", "23456789012", "buddy@gmail.com"});
      label2 = MakeLabel({"3734 Elvis Presley Blvd., Apt. 10", "(234) 567-8901",
                          "theking@gmail.com"});
      break;
    case EnabledFeature::kMobileShowAll:
      label1 = MakeMobileLabel(
          {"123 Apple St., unit 6", "23456789012", "buddy@gmail.com"});
      label2 = MakeMobileLabel({"3734 Elvis Presley Blvd., Apt. 10",
                                "(234) 567-8901", "theking@gmail.com"});
      break;
    case EnabledFeature::kMobileShowOne:
      label1 = "123 Apple St., unit 6";
      label2 = "3734 Elvis Presley Blvd., Apt. 10";
      break;
    case EnabledFeature::kNone:
      label1 = "123 Apple St.";
      label2 = "3734 Elvis Presley Blvd.";
  }
  // Test that we sent the right values to the external delegate.
  CheckSuggestions(form.fields[0].global_id(),
                   Suggestion("Charles", label1, kAddressEntryIcon, 1),
                   Suggestion("Elvis", label2, kAddressEntryIcon, 2));

  FormFieldData field;
  test::CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
  GetAutofillSuggestions(form, field);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  const std::string visa_label = std::string("04/99");
  const std::string master_card_label = std::string("10/98");
#else
  const std::string visa_label = std::string("Expires on 04/99");
  const std::string master_card_label = std::string("Expires on 10/98");
#endif

  // Test that we sent the credit card suggestions to the external delegate.
  CheckSuggestions(
      field.global_id(),
      Suggestion(
          std::string("Visa  ") + test::ObfuscatedCardDigitsAsUTF8("3456"),
          visa_label, kVisaCard,
          browser_autofill_manager_->GetPackedCreditCardID(4)),
      Suggestion(std::string("Mastercard  ") +
                     test::ObfuscatedCardDigitsAsUTF8("8765"),
                 master_card_label, kMasterCard,
                 browser_autofill_manager_->GetPackedCreditCardID(5)));
}

// Test that for non-https forms with both address and credit card fields, we
// only return address suggestions. Instead of credit card suggestions, we
// should return a warning explaining that credit card profile suggestions are
// unavailable when the form is not https.
TEST_F(BrowserAutofillManagerTest, GetAddressAndCreditCardSuggestionsNonHttps) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  CreateTestCreditCardFormData(&form, false, false);
  FormsSeen({form});

  GetAutofillSuggestions(form, form.fields[0]);

  // Verify that suggestions are returned.
  EXPECT_TRUE(external_delegate_->on_suggestions_returned_seen());

  FormFieldData field;
  test::CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
  GetAutofillSuggestions(form, field);

  // Test that we sent the right values to the external delegate.
  CheckSuggestions(field.global_id(),
                   Suggestion(l10n_util::GetStringUTF8(
                                  IDS_AUTOFILL_WARNING_INSECURE_CONNECTION),
                              "", "", -1));

  // Clear the test credit cards and try again -- we shouldn't return a warning.
  personal_data().ClearCreditCards();
  GetAutofillSuggestions(form, field);
  external_delegate_->CheckNoSuggestions(field.global_id());
}

TEST_F(BrowserAutofillManagerTest,
       ShouldShowAddressSuggestionsIfCreditCardAutofillDisabled) {
  base::test::ScopedFeatureList features;
  DisableAutofillViaAblation(features, /*for_addresses=*/false,
                             /*for_credit_cards=*/true);

  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  FormsSeen({form});

  GetAutofillSuggestions(form, form.fields[0]);
  // Verify that suggestions are returned.
  EXPECT_TRUE(external_delegate_->on_suggestions_returned_seen());
}

TEST_F(BrowserAutofillManagerTest,
       ShouldShowCreditCardSuggestionsIfAddressAutofillDisabled) {
  base::test::ScopedFeatureList features;
  DisableAutofillViaAblation(features, /*for_addresses=*/true,
                             /*for_credit_cards=*/false);

  // Set up our form data.
  FormData form = CreateTestCreditCardFormData(true, false);
  FormsSeen({form});

  GetAutofillSuggestions(form, form.fields[0]);
  // Verify that suggestions are returned.
  EXPECT_TRUE(external_delegate_->on_suggestions_returned_seen());
}

// Test that the correct section is filled.
TEST_F(BrowserAutofillManagerTest, FillTriggeredSection) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  size_t index_of_trigger_field = form.fields.size();
  test::CreateTestAddressFormData(&form);
  FormsSeen({form});

  // Check that the form has been parsed into two sections.
  ASSERT_NE(form.fields.size(), 0u);
  ASSERT_EQ(index_of_trigger_field, form.fields.size() / 2);
  {
    FormStructure* form_structure;
    AutofillField* autofill_field;
    bool found = browser_autofill_manager_->GetCachedFormAndField(
        form, form.fields[index_of_trigger_field], &form_structure,
        &autofill_field);
    ASSERT_TRUE(found);
    for (size_t i = 0; i < form.fields.size() / 2; ++i) {
      size_t j = form.fields.size() / 2 + i;
      ASSERT_EQ(form_structure->field(i)->name, form_structure->field(j)->name);
      ASSERT_NE(form_structure->field(i)->section,
                form_structure->field(j)->section);
      ASSERT_TRUE(form_structure->field(i)->SameFieldAs(form.fields[j]));
      ASSERT_TRUE(form_structure->field(j)->SameFieldAs(form.fields[i]));
    }
  }

  const char guid[] = "00000000-0000-0000-0000-000000000001";
  AutofillProfile* profile = personal_data().GetProfileByGUID(guid);
  ASSERT_TRUE(profile);
  EXPECT_EQ(1U, profile->use_count());
  EXPECT_NE(base::Time(), profile->use_date());

  FormData response_data;
  FillAutofillFormDataAndSaveResults(form, form.fields[index_of_trigger_field],
                                     MakeFrontendId({.profile_id = guid}),
                                     &response_data);
  // Extract the sections into individual forms to reduce boiler plate code.
  size_t mid = response_data.fields.size() / 2;
  FormData section1 = response_data;
  FormData section2 = response_data;
  section1.fields.erase(section1.fields.begin() + mid, section1.fields.end());
  section2.fields.erase(section2.fields.begin(), section2.fields.end() - mid);
  // First section should be empty, second should be filled.
  ExpectFilledForm(section1, "", "", "", "", "", "", "", "", "", "", "", "", "",
                   "", "", true, false, false);
  ExpectFilledAddressFormElvis(section2, false);
}

MATCHER_P(HasValue, value, "") {
  return arg.value == value;
}

// Test that if the form cache is outdated because a field has changed, filling
// is aborted after that field.
TEST_F(BrowserAutofillManagerTest, DoNotFillIfFormFieldChanged) {
  FormData form;
  test::CreateTestAddressFormData(&form);
  FormsSeen({form});

  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  ASSERT_TRUE(browser_autofill_manager_->GetCachedFormAndField(
      form, form.fields.front(), &form_structure, &autofill_field));

  // Modify |form| so that it doesn't match |form_structure| anymore.
  ASSERT_GE(form.fields.size(), 3u);
  for (auto it = form.fields.begin() + 2; it != form.fields.end(); ++it)
    *it = FormFieldData();

  const char guid[] = "00000000-0000-0000-0000-000000000001";
  AutofillProfile* profile = personal_data().GetProfileByGUID(guid);
  ASSERT_TRUE(profile);

  FormData response_data;
  EXPECT_CALL(*autofill_driver_, FillOrPreviewForm(_, _, _, _))
      .WillOnce((DoAll(testing::SaveArg<1>(&response_data),
                       testing::Return(std::vector<FieldGlobalId>{}))));
  browser_autofill_manager_->FillOrPreviewDataModelFormForTest(
      mojom::RendererFormDataAction::kFill, form, form.fields.front(), profile,
      nullptr, form_structure, autofill_field);
  std::vector<FormFieldData> filled_fields(response_data.fields.begin(),
                                           response_data.fields.begin() + 2);
  std::vector<FormFieldData> skipped_fields(response_data.fields.begin() + 2,
                                            response_data.fields.end());

  EXPECT_THAT(filled_fields, Each(Not(HasValue(u""))));
  EXPECT_THAT(skipped_fields, Each(HasValue(u"")));
}

TEST_F(BrowserAutofillManagerTest,
       FillOrPreviewDataModelFormCallsDidFillOrPreviewForm) {
  FormData form =
      CreateTestCreditCardFormData(/*is_https=*/true, /*use_month_type=*/false);
  FormsSeen({form});
  FormStructure* form_structure;
  AutofillField* autofill_field;
  ASSERT_TRUE(browser_autofill_manager_->GetCachedFormAndField(
      form, form.fields.front(), &form_structure, &autofill_field));
  EXPECT_CALL(autofill_client_, DidFillOrPreviewForm);
  browser_autofill_manager_->FillOrPreviewDataModelFormForTest(
      mojom::RendererFormDataAction::kFill, form, form.fields.front(),
      personal_data().GetCreditCards()[0], /*optional_cvc=*/nullptr,
      form_structure, autofill_field);
}

// Test that if the form cache is outdated because a field was removed, filling
// is aborted.
TEST_F(BrowserAutofillManagerTest, DoNotFillIfFormFieldRemoved) {
  FormData form;
  test::CreateTestAddressFormData(&form);
  FormsSeen({form});

  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  ASSERT_TRUE(browser_autofill_manager_->GetCachedFormAndField(
      form, form.fields.front(), &form_structure, &autofill_field));

  // Modify |form| so that it doesn't match |form_structure| anymore.
  ASSERT_GE(form.fields.size(), 2u);
  form.fields.pop_back();

  const char guid[] = "00000000-0000-0000-0000-000000000001";
  AutofillProfile* profile = personal_data().GetProfileByGUID(guid);
  ASSERT_TRUE(profile);

  EXPECT_CALL(*autofill_driver_, FillOrPreviewForm(_, _, _, _)).Times(0);
}

// Tests that BrowserAutofillManager ignores loss of focus events sent from the
// renderer if the renderer did not have a previously-interacted form.
// TODO(crbug.com/1140473): Remove this test when workaround is no longer
// needed.
TEST_F(BrowserAutofillManagerTest,
       ShouldIgnoreLossOfFocusWithNoPreviouslyInteractedForm) {
  FormData form;
  test::CreateTestAddressFormData(&form);

  browser_autofill_manager_->UpdatePendingForm(form);
  ASSERT_TRUE(
      browser_autofill_manager_->pending_form_data_for_test()->SameFormAs(
          form));

  // Receiving a notification that focus is no longer on the form *without* the
  // renderer having a previously-interacted form should not result in
  // any changes to the pending form.
  browser_autofill_manager_->OnFocusNoLongerOnForm(
      /*had_interacted_form=*/false);
  EXPECT_TRUE(
      browser_autofill_manager_->pending_form_data_for_test()->SameFormAs(
          form));
}

TEST_F(BrowserAutofillManagerTest,
       ShouldNotShowCreditCardsSuggestionsIfCreditCardAutofillDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  DisableAutofillViaAblation(scoped_feature_list, /*for_addresses=*/false,
                             /*for_credit_cards=*/true);

  // Set up our form data.
  FormData form = CreateTestCreditCardFormData(true, false);
  FormsSeen({form});

  GetAutofillSuggestions(form, form.fields[0]);

  // Check that credit card suggestions will not be available.
  external_delegate_->CheckNoSuggestions(form.fields[0].global_id());
}

TEST_F(BrowserAutofillManagerTest,
       ShouldNotShowAddressSuggestionsIfAddressAutofillDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  DisableAutofillViaAblation(scoped_feature_list, /*for_addresses=*/true,
                             /*for_credit_cards=*/false);

  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  FormsSeen({form});

  GetAutofillSuggestions(form, form.fields[0]);

  // Check that credit card suggestions will not be available.
  external_delegate_->CheckNoSuggestions(form.fields[0].global_id());
}

struct LogAblationTestParams {
  const char* description;
  // Whether any autofillable data is stored.
  bool run_with_data_on_file = true;
  // If true, the credit card owner name field is filled with value that is not
  // a prefix of any stored credit card and then autofill suggestions are
  // queried a second time.
  bool second_query_for_suggestions_with_typed_prefix = false;
  // Whether the form should be submitted before validating the metrics.
  bool submit_form = true;
};

enum class LogAblationFormType {
  kAddress,
  kPayment,
  kMixed,  // address fields followed by payment fields
};

class BrowserAutofillManagerLogAblationTest
    : public BrowserAutofillManagerTest,
      public testing::WithParamInterface<
          std::tuple<LogAblationTestParams, LogAblationFormType>> {
 public:
  BrowserAutofillManagerLogAblationTest() = default;
  ~BrowserAutofillManagerLogAblationTest() override = default;
};

// Validate that UMA logging works correctly for ablation studies.
INSTANTIATE_TEST_SUITE_P(
    All,
    BrowserAutofillManagerLogAblationTest,
    testing::Combine(
        testing::Values(
            // Test that if autofillable data is stored and the ablation is
            // enabled, we record metrics as expected.
            LogAblationTestParams{.description = "Having data to fill"},
            // Test that if NO autofillable data is stored and the ablation is
            // enabled, we record "UnconditionalAblation" metrics but no
            // "ConditionalAblation" metrics. The latter only recoded on the
            // condition that we have data to fill.
            LogAblationTestParams{.description = "Having NO data to fill",
                                  .run_with_data_on_file = false},
            // In this test we trigger the GetAutofillSuggestions call twice. By
            // the second time the user has typed a value that is not the prefix
            // of any existing autofill data. This means that autofill would not
            // create any suggestions. We still want to consider this a
            // conditional ablation (the condition to have fillable data on file
            // is met).
            LogAblationTestParams{
                .description = "Typed unknown prefix",
                .second_query_for_suggestions_with_typed_prefix = false},
            // Test that the right events are recorded in case the user
            // interacts with a form but does not submit it.
            LogAblationTestParams{.description = "No form submission",
                                  .submit_form = false}),
        testing::Values(LogAblationFormType::kAddress,
                        LogAblationFormType::kPayment,
                        LogAblationFormType::kMixed)));

TEST_P(BrowserAutofillManagerLogAblationTest, TestLogging) {
  const LogAblationTestParams& params = std::get<0>(GetParam());
  LogAblationFormType form_type = std::get<1>(GetParam());

  SCOPED_TRACE(testing::Message() << params.description << " Form type: "
                                  << static_cast<int>(form_type));

  if (!params.run_with_data_on_file) {
    personal_data().ClearAllServerData();
    personal_data().ClearAllLocalData();
  }

  base::test::ScopedFeatureList scoped_feature_list;
  DisableAutofillViaAblation(scoped_feature_list, /*for_addresses=*/true,
                             /*for_credit_cards=*/true);
  TestAutofillTickClock clock(AutofillTickClock::NowTicks());
  base::HistogramTester histogram_tester;

  // Set up our form data. In the kMixed case the form will contain the fields
  // of an address form followed by the fields of fields of a payment form. The
  // triggering for autofill suggestions will happen on an address field in this
  // case.
  FormData form;
  if (form_type == LogAblationFormType::kAddress ||
      form_type == LogAblationFormType::kMixed) {
    test::CreateTestAddressFormData(&form);
  }
  if (form_type == LogAblationFormType::kPayment ||
      form_type == LogAblationFormType::kMixed) {
    CreateTestCreditCardFormData(&form, true, false);
  }
  FormsSeen({form});

  // Simulate retrieving autofill suggestions with the first field as a trigger
  // script. This should emit signals that lead to recorded metrics later on.
  FormFieldData field = form.fields[0];
  GetAutofillSuggestions(form, field);

  // Simulate user typing into field (due to the ablation we would not fill).
  field.value = u"Unknown User";
  browser_autofill_manager_->OnTextFieldDidChange(
      form, field, gfx::RectF(), AutofillTickClock::NowTicks());

  if (params.second_query_for_suggestions_with_typed_prefix) {
    // Do another lookup. We won't have any suggestions because they would not
    // be compatible with the "Unknown User" username.
    GetAutofillSuggestions(form, field);
  }

  // Advance time and possibly submit the form.
  base::TimeDelta time_delta = base::Seconds(42);
  clock.Advance(time_delta);
  if (params.submit_form)
    FormSubmitted(form);

  // Flush FormEventLoggers.
  browser_autofill_manager_->Reset();

  // Validate the recorded metrics.
  std::string form_type_str = (form_type == LogAblationFormType::kAddress ||
                               form_type == LogAblationFormType::kMixed)
                                  ? "Address"
                                  : "CreditCard";

  // If data was on file, we expect conditional ablation metrics.
  if (params.run_with_data_on_file) {
    histogram_tester.ExpectUniqueSample(
        "Autofill.Ablation.FormSubmissionAfterInteraction." + form_type_str +
            ".ConditionalAblation",
        /*sample=*/params.submit_form ? 1 : 0,
        /*expected_bucket_count=*/1);
  } else {
    histogram_tester.ExpectTotalCount(
        "Autofill.Ablation.FormSubmissionAfterInteraction." + form_type_str +
            ".ConditionalAblation",
        /*count=*/0);
  }
  // Only if data was on file an a submission happened, we can record the
  // duration from interaction to submission.
  if (params.run_with_data_on_file && params.submit_form) {
    histogram_tester.ExpectUniqueTimeSample(
        "Autofill.Ablation.FillDurationSinceInteraction." + form_type_str +
            ".ConditionalAblation",
        time_delta,
        /*expected_bucket_count=*/1);
  } else {
    histogram_tester.ExpectTotalCount(
        "Autofill.Ablation.FillDurationSinceInteraction." + form_type_str +
            ".ConditionalAblation",
        /*count=*/0);
  }
  // The unconditional ablation metrics should always be logged as this the
  // ablation study is always enabled.
  histogram_tester.ExpectUniqueSample(
      "Autofill.Ablation.FormSubmissionAfterInteraction." + form_type_str +
          ".UnconditionalAblation",
      /*sample=*/params.submit_form ? 1 : 0,
      /*expected_bucket_count=*/1);
  // Expect a time from interaction to submission the form was submitted.
  if (params.submit_form) {
    histogram_tester.ExpectUniqueTimeSample(
        "Autofill.Ablation.FillDurationSinceInteraction." + form_type_str +
            ".UnconditionalAblation",
        time_delta,
        /*expected_bucket_count=*/1);
  } else {
    histogram_tester.ExpectTotalCount(
        "Autofill.Ablation.FillDurationSinceInteraction." + form_type_str +
            ".UnconditionalAblation",
        /*count=*/0);
  }

  // Ensure that no metrics are recorded for the complementary form type.
  // I.e. if the trigger element is of an address form, the credit card form
  // should have no metrics.
  std::string complementary_form_type_str =
      (form_type == LogAblationFormType::kAddress ||
       form_type == LogAblationFormType::kMixed)
          ? "CreditCard"
          : "Address";
  for (const char* metric :
       {"FormSubmissionAfterInteraction", "FillDurationSinceInteraction"}) {
    for (const char* ablation_type :
         {"UnconditionalAblation", "ConditionalAblation"}) {
      histogram_tester.ExpectTotalCount(
          base::StrCat({"Autofill.Ablation.", metric, ".",
                        complementary_form_type_str.c_str(), ".",
                        ablation_type}),
          /*count=*/0);
    }
  }
}

// Test that we properly match typed values to stored state data.
TEST_F(BrowserAutofillManagerTest, DetermineStateFieldTypeForUpload) {
  base::test::ScopedFeatureList feature;
  feature.InitAndEnableFeature(features::kAutofillUseAlternativeStateNameMap);

  test::ClearAlternativeStateNameMapForTesting();
  test::PopulateAlternativeStateNameMapForTesting();

  AutofillProfile profile;
  test::SetProfileInfo(&profile, "", "", "", "", "", "", "", "", "Bavaria", "",
                       "DE", "");

  const char* const kValidMatches[] = {"by", "Bavaria", "Bayern",
                                       "BY", "B.Y",     "B-Y"};
  for (const char* valid_match : kValidMatches) {
    SCOPED_TRACE(valid_match);
    FormData form;
    FormFieldData field;

    test::CreateTestFormField("Name", "Name", "Test", "text", &field);
    form.fields.push_back(field);

    test::CreateTestFormField("State", "state", valid_match, "text", &field);
    form.fields.push_back(field);

    FormStructure form_structure(form);
    EXPECT_EQ(form_structure.field_count(), 2U);

    browser_autofill_manager_->PreProcessStateMatchingTypesForTest(
        {profile}, &form_structure);
    EXPECT_TRUE(form_structure.field(1)->state_is_a_matching_type());
  }

  const char* const kInvalidMatches[] = {"Garbage", "BYA",   "BYA is a state",
                                         "Bava",    "Empty", ""};
  for (const char* invalid_match : kInvalidMatches) {
    SCOPED_TRACE(invalid_match);
    FormData form;
    FormFieldData field;

    test::CreateTestFormField("Name", "Name", "Test", "text", &field);
    form.fields.push_back(field);

    test::CreateTestFormField("State", "state", invalid_match, "text", &field);
    form.fields.push_back(field);

    FormStructure form_structure(form);
    EXPECT_EQ(form_structure.field_count(), 2U);

    browser_autofill_manager_->PreProcessStateMatchingTypesForTest(
        {profile}, &form_structure);
    EXPECT_FALSE(form_structure.field(1)->state_is_a_matching_type());
  }

  test::PopulateAlternativeStateNameMapForTesting(
      "US", "California",
      {{.canonical_name = "California",
        .abbreviations = {"CA"},
        .alternative_names = {}}});

  test::SetProfileInfo(&profile, "", "", "", "", "", "", "", "", "California",
                       "", "US", "");

  FormData form;
  FormFieldData field;

  test::CreateTestFormField("Name", "Name", "Test", "text", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("State", "state", "CA", "text", &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  EXPECT_EQ(form_structure.field_count(), 2U);

  browser_autofill_manager_->PreProcessStateMatchingTypesForTest(
      {profile}, &form_structure);
  EXPECT_TRUE(form_structure.field(1)->state_is_a_matching_type());
}

// Ensures that if autofill is disabled but the password manager is enabled,
// Autofill still performs a lookup to the server.
TEST_F(BrowserAutofillManagerTest,
       OnFormsSeen_AutofillDisabledPasswordManagerEnabled) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);

  // Disable autofill and the password manager.
  browser_autofill_manager_->SetAutofillCreditCardEnabled(autofill_client_,
                                                          false);
  browser_autofill_manager_->SetAutofillProfileEnabled(autofill_client_, false);
  ON_CALL(autofill_client_, IsPasswordManagerEnabled())
      .WillByDefault(Return(false));

  // As neither autofill nor password manager are enabled, the form should
  // not be parsed.
  {
    base::HistogramTester histogram_tester;
    FormsSeen({form});
    EXPECT_EQ(0, histogram_tester.GetBucketCount("Autofill.UserHappiness",
                                                 0 /* FORMS_LOADED */));
  }

  // Now enable the password manager.
  ON_CALL(autofill_client_, IsPasswordManagerEnabled())
      .WillByDefault(Return(true));
  // If the password manager is enabled, that's enough to parse the form.
  {
    base::HistogramTester histogram_tester;
    FormsSeen({form});
    histogram_tester.ExpectUniqueSample("Autofill.UserHappiness",
                                        0 /* FORMS_LOADED */, 1);
    download_manager_->VerifyLastQueriedForms({form});
  }
}

// Test that we return normal Autofill suggestions when trying to autofill
// already filled forms.
TEST_P(SuggestionMatchingTest, GetFieldSuggestionsWhenFormIsAutofilled) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  FormsSeen({form});

  // Mark one of the fields as filled.
  form.fields[2].is_autofilled = true;
  GetAutofillSuggestions(form, form.fields[0]);

  std::string label1;
  std::string label2;

  switch (enabled_feature_) {
      // 23456789012 is not formatted because it is invalid for the app locale.
      // It has an extra digit.
    case EnabledFeature::kDesktop:
      label1 = MakeLabel(
          {"123 Apple St., unit 6", "23456789012", "buddy@gmail.com"});
      label2 = MakeLabel({"3734 Elvis Presley Blvd., Apt. 10", "(234) 567-8901",
                          "theking@gmail.com"});
      break;
    case EnabledFeature::kMobileShowAll:
      label1 = MakeMobileLabel(
          {"123 Apple St., unit 6", "23456789012", "buddy@gmail.com"});
      label2 = MakeMobileLabel({"3734 Elvis Presley Blvd., Apt. 10",
                                "(234) 567-8901", "theking@gmail.com"});
      break;
    case EnabledFeature::kMobileShowOne:
      label1 = "123 Apple St., unit 6";
      label2 = "3734 Elvis Presley Blvd., Apt. 10";
      break;
    case EnabledFeature::kNone:
      label1 = "123 Apple St.";
      label2 = "3734 Elvis Presley Blvd.";
  }
  // Test that we sent the right values to the external delegate.
  CheckSuggestions(form.fields[0].global_id(),
                   Suggestion("Charles", label1, kAddressEntryIcon, 1),
                   Suggestion("Elvis", label2, kAddressEntryIcon, 2));
}

// Test that nothing breaks when there are single field form fill (Autocomplete)
// suggestions but no autofill suggestions.
TEST_F(BrowserAutofillManagerTest,
       GetFieldSuggestionsForSingleFieldFormFillOnly) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  FormFieldData field;
  test::CreateTestFormField("Some Field", "somefield", "", "text", &field);
  form.fields.push_back(field);
  FormsSeen({form});

  GetAutofillSuggestions(form, field);

  // Add some Autocomplete suggestions.
  // This triggers the combined message send.
  std::vector<std::u16string> suggestions;
  suggestions.push_back(u"one");
  suggestions.push_back(u"two");
  AutocompleteSuggestionsReturned(field.global_id(), suggestions);

  // Test that we sent the right values to the external delegate.
  CheckSuggestions(field.global_id(), Suggestion("one", "", "", 0),
                   Suggestion("two", "", "", 0));
}

// Test that we do not return duplicate values drawn from multiple profiles when
// filling an already filled field.
TEST_P(SuggestionMatchingTest, GetFieldSuggestionsWithDuplicateValues) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  FormsSeen({form});

  // |profile| will be owned by the mock PersonalDataManager.
  AutofillProfile profile;
  test::SetProfileInfo(&profile, "Elvis", "", "", "", "", "", "", "", "", "",
                       "", "");
  profile.set_guid("00000000-0000-0000-0000-000000000101");
  personal_data().AddProfile(profile);

  FormFieldData& field = form.fields[0];
  field.is_autofilled = true;
  field.value = u"Elvis";
  GetAutofillSuggestions(form, field);

  std::string label;

  switch (enabled_feature_) {
    case EnabledFeature::kDesktop:
    case EnabledFeature::kMobileShowAll:
    case EnabledFeature::kMobileShowOne:
      label = "3734 Elvis Presley Blvd., Apt. 10";
      break;
    case EnabledFeature::kNone:
      label = "3734 Elvis Presley Blvd.";
  }
  // Test that we sent the right values to the external delegate.
  CheckSuggestions(field.global_id(),
                   Suggestion("Elvis", label, kAddressEntryIcon, 1));
}

TEST_P(SuggestionMatchingTest, GetProfileSuggestions_FancyPhone) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  FormsSeen({form});

  AutofillProfile profile;
  profile.set_guid("00000000-0000-0000-0000-000000000103");
  profile.SetInfo(NAME_FULL, u"Natty Bumppo", "en-US");
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"1800PRAIRIE");
  personal_data().AddProfile(profile);

  GetAutofillSuggestions(form, form.fields[9]);

  std::string value1;
  std::string value2;
  std::string value3;
  std::string label1;
  std::string label2;
  std::string label3;

  switch (enabled_feature_) {
      // 23456789012 is not formatted because it is invalid for the app locale.
      // It has an extra digit.
    case EnabledFeature::kDesktop:
      value1 = "(800) 772-4743";
      value2 = "23456789012";
      value3 = "(234) 567-8901";
      label1 = "Natty Bumppo";
      label2 = MakeLabel(
          {"Charles Holley", "123 Apple St., unit 6", "buddy@gmail.com"});
      label3 = MakeLabel({"Elvis Presley", "3734 Elvis Presley Blvd., Apt. 10",
                          "theking@gmail.com"});
      break;
    case EnabledFeature::kMobileShowAll:
      value1 = "(800) 772-4743";
      value2 = "23456789012";
      value3 = "(234) 567-8901";
      label1 = "Natty";
      label2 = MakeMobileLabel(
          {"Charles", "123 Apple St., unit 6", "buddy@gmail.com"});
      label3 = MakeMobileLabel(
          {"Elvis", "3734 Elvis Presley Blvd., Apt. 10", "theking@gmail.com"});
      break;
    case EnabledFeature::kMobileShowOne:
      value1 = "(800) 772-4743";
      value2 = "23456789012";
      value3 = "(234) 567-8901";
      label1 = "";
      label2 = "123 Apple St., unit 6";
      label3 = "3734 Elvis Presley Blvd., Apt. 10";
      break;
    case EnabledFeature::kNone:
      value1 = "18007724743";  // 1800PRAIRIE
      value2 = "23456789012";
      value3 = "12345678901";
      label1 = "Natty Bumppo";
      label2 = "123 Apple St.";
      label3 = "3734 Elvis Presley Blvd.";
  }
  // Test that we sent the right values to the external delegate.
  CheckSuggestions(form.fields[9].global_id(),
                   Suggestion(value1, label1, kAddressEntryIcon, 1),
                   Suggestion(value2, label2, kAddressEntryIcon, 2),
                   Suggestion(value3, label3, kAddressEntryIcon, 3));
}

TEST_F(BrowserAutofillManagerTest,
       GetProfileSuggestions_ForPhonePrefixOrSuffix) {
  // Set up our form data.
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");

  struct {
    const char* const label;
    const char* const name;
    size_t max_length;
    const char* const autocomplete_attribute;
  } test_fields[] = {{"country code", "country_code", 1, "tel-country-code"},
                     {"area code", "area_code", 3, "tel-area-code"},
                     {"phone", "phone_prefix", 3, "tel-local-prefix"},
                     {"-", "phone_suffix", 4, "tel-local-suffix"},
                     {"Phone Extension", "ext", 5, "tel-extension"}};

  FormFieldData field;
  for (const auto& test_field : test_fields) {
    test::CreateTestFormField(test_field.label, test_field.name, "", "text", "",
                              test_field.max_length, &field);
    form.fields.push_back(field);
  }

  FormsSeen({form});

  personal_data().ClearProfiles();
  AutofillProfile profile;
  profile.set_guid("00000000-0000-0000-0000-000000000104");
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"1800FLOWERS");
  personal_data().AddProfile(profile);

  const FormFieldData& phone_prefix = form.fields[2];
  GetAutofillSuggestions(form, phone_prefix);

  // Test that we sent the right prefix values to the external delegate.
  CheckSuggestions(form.fields[2].global_id(),
                   Suggestion("356", "1800FLOWERS", kAddressEntryIcon, 1));

  const FormFieldData& phone_suffix = form.fields[3];
  GetAutofillSuggestions(form, phone_suffix);

  // Test that we sent the right suffix values to the external delegate.
  CheckSuggestions(form.fields[3].global_id(),
                   Suggestion("9377", "1800FLOWERS", kAddressEntryIcon, 1));
}

// Tests that the suggestion consists of phone number without the country code
// when a length limit is imposed in the field due to which filling with
// country code is not possible.
TEST_F(BrowserAutofillManagerTest, GetProfileSuggestions_ForPhoneField) {
  FormData form;
  test::CreateTestAddressFormData(&form);
  form.fields[9].max_length = 10;
  FormsSeen({form});

  AutofillProfile profile;
  profile.set_guid("00000000-0000-0000-0000-000000000103");
  profile.SetInfo(NAME_FULL, u"Natty Bumppo", "en-US");
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"+886123456789");
  personal_data().ClearProfiles();
  personal_data().AddProfile(profile);

  GetAutofillSuggestions(form, form.fields[9]);

  CheckSuggestions(
      form.fields[9].global_id(),
      Suggestion("123456789", "Natty Bumppo", kAddressEntryIcon, 1));
}

// Tests that we return email profile suggestions values
// when the email field with username autocomplete attribute exist.
TEST_F(BrowserAutofillManagerTest,
       GetProfileSuggestions_ForEmailFieldWithUserNameAutocomplete) {
  // Set up our form data.
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");

  struct {
    const char* const label;
    const char* const name;
    size_t max_length;
    const char* const autocomplete_attribute;
  } test_fields[] = {{"First Name", "firstname", 30, "given-name"},
                     {"Last Name", "lastname", 30, "family-name"},
                     {"Email", "email", 30, "username"},
                     {"Password", "password", 30, "new-password"}};

  FormFieldData field;
  for (const auto& test_field : test_fields) {
    const char* const field_type =
        strcmp(test_field.name, "password") == 0 ? "password" : "text";
    test::CreateTestFormField(test_field.label, test_field.name, "", field_type,
                              test_field.autocomplete_attribute,
                              test_field.max_length, &field);
    form.fields.push_back(field);
  }

  FormsSeen({form});

  personal_data().ClearProfiles();
  AutofillProfile profile;
  profile.set_guid("00000000-0000-0000-0000-000000000103");
  profile.SetRawInfo(NAME_FULL, u"Natty Bumppo");
  profile.SetRawInfo(EMAIL_ADDRESS, u"test@example.com");
  personal_data().AddProfile(profile);

  GetAutofillSuggestions(form, form.fields[2]);
  CheckSuggestions(
      form.fields[2].global_id(),
      Suggestion("test@example.com", "Natty Bumppo", kAddressEntryIcon, 1));
}

// Test that we correctly fill an address form.
TEST_F(BrowserAutofillManagerTest, FillAddressForm) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  FormsSeen({form});

  const char guid[] = "00000000-0000-0000-0000-000000000001";
  AutofillProfile* profile = personal_data().GetProfileByGUID(guid);
  ASSERT_TRUE(profile);
  EXPECT_EQ(1U, profile->use_count());
  EXPECT_NE(base::Time(), profile->use_date());

  FormData response_data;
  FillAutofillFormDataAndSaveResults(form, form.fields[0],
                                     MakeFrontendId({.profile_id = guid}),
                                     &response_data);
  ExpectFilledAddressFormElvis(response_data, false);

  EXPECT_EQ(2U, profile->use_count());
  EXPECT_NE(base::Time(), profile->use_date());
}

TEST_F(BrowserAutofillManagerTest, WillFillCreditCardNumber) {
  // Set up our form data.
  FormData form = CreateTestCreditCardFormData(true, false);
  FormsSeen({form});

  FormFieldData* number_field = nullptr;
  FormFieldData* name_field = nullptr;
  FormFieldData* month_field = nullptr;
  for (auto& field : form.fields) {
    if (field.name == u"cardnumber") {
      number_field = &field;
    } else if (field.name == u"nameoncard") {
      name_field = &field;
    } else if (field.name == u"ccmonth") {
      month_field = &field;
    }
  }

  // Empty form - whole form is Autofilled.
  EXPECT_TRUE(WillFillCreditCardNumber(form, *number_field));
  EXPECT_TRUE(WillFillCreditCardNumber(form, *name_field));
  EXPECT_TRUE(WillFillCreditCardNumber(form, *month_field));

  // If the user has entered a value, it won't be overridden.
  number_field->value = u"gibberish";
  EXPECT_TRUE(WillFillCreditCardNumber(form, *number_field));
  EXPECT_FALSE(WillFillCreditCardNumber(form, *name_field));
  EXPECT_FALSE(WillFillCreditCardNumber(form, *month_field));

  // But if that value is removed, it will be Autofilled.
  number_field->value.clear();
  EXPECT_TRUE(WillFillCreditCardNumber(form, *name_field));
  EXPECT_TRUE(WillFillCreditCardNumber(form, *month_field));

  // When the number is already autofilled, we won't fill it.
  number_field->is_autofilled = true;
  EXPECT_TRUE(WillFillCreditCardNumber(form, *number_field));
  EXPECT_FALSE(WillFillCreditCardNumber(form, *name_field));
  EXPECT_FALSE(WillFillCreditCardNumber(form, *month_field));

  // If another field is filled, we would still fill other non-filled fields in
  // the section.
  number_field->is_autofilled = false;
  name_field->is_autofilled = true;
  EXPECT_TRUE(WillFillCreditCardNumber(form, *name_field));
  EXPECT_TRUE(WillFillCreditCardNumber(form, *month_field));
}

// Test that we correctly log FIELD_WAS_AUTOFILLED event in UserHappiness.
TEST_F(BrowserAutofillManagerTest, FillCreditCardForm_LogFieldWasAutofill) {
  // Set up our form data.
  FormData form;
  // Construct a form with a 4 fields: cardholder name, card number,
  // expiration date and cvc.
  CreateTestCreditCardFormData(&form, true, true);
  FormsSeen({form});

  const char guid[] = "00000000-0000-0000-0000-000000000004";
  FormData response_data;
  base::HistogramTester histogram_tester;
  FillAutofillFormDataAndSaveResults(form, *form.fields.begin(),
                                     MakeFrontendId({.credit_card_id = guid}),
                                     &response_data);
  // Cardholder name, card number, expiration data were autofilled but cvc was
  // not be autofilled.
  histogram_tester.ExpectBucketCount("Autofill.UserHappiness.CreditCard",
                                     AutofillMetrics::FIELD_WAS_AUTOFILLED, 3);
}

// Test that we correctly fill a credit card form.
TEST_F(BrowserAutofillManagerTest, FillCreditCardForm_Simple) {
  // Set up our form data.
  FormData form = CreateTestCreditCardFormData(true, false);
  FormsSeen({form});

  const char guid[] = "00000000-0000-0000-0000-000000000004";
  FormData response_data;
  FillAutofillFormDataAndSaveResults(form, *form.fields.begin(),
                                     MakeFrontendId({.credit_card_id = guid}),
                                     &response_data);
  ExpectFilledCreditCardFormElvis(response_data, false);
}

// Test that whitespace is stripped from the credit card number.
TEST_F(BrowserAutofillManagerTest,
       FillCreditCardForm_StripCardNumberWhitespace) {
  // Same as the SetUp(), but generate Elvis card with whitespace in credit
  // card number.  |credit_card| will be owned by the TestPersonalDataManager.
  personal_data().ClearCreditCards();
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "Elvis Presley",
                          "4234 5678 9012 3456",  // Visa
                          "04", "2999", "1");
  credit_card.set_guid("00000000-0000-0000-0000-000000000008");
  personal_data().AddCreditCard(credit_card);
  // Set up our form data.
  FormData form = CreateTestCreditCardFormData(true, false);
  FormsSeen({form});

  const char guid[] = "00000000-0000-0000-0000-000000000008";
  FormData response_data;
  FillAutofillFormDataAndSaveResults(form, *form.fields.begin(),
                                     MakeFrontendId({.credit_card_id = guid}),
                                     &response_data);
  ExpectFilledCreditCardFormElvis(response_data, false);
}

// Test that separator characters are stripped from the credit card number.
TEST_F(BrowserAutofillManagerTest,
       FillCreditCardForm_StripCardNumberSeparators) {
  // Same as the SetUp(), but generate Elvis card with separator characters in
  // credit card number.  |credit_card| will be owned by the
  // TestPersonalDataManager.
  personal_data().ClearCreditCards();
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "Elvis Presley",
                          "4234-5678-9012-3456",  // Visa
                          "04", "2999", "1");
  credit_card.set_guid("00000000-0000-0000-0000-000000000009");
  personal_data().AddCreditCard(credit_card);
  // Set up our form data.
  FormData form = CreateTestCreditCardFormData(true, false);
  FormsSeen({form});

  const char guid[] = "00000000-0000-0000-0000-000000000009";
  FormData response_data;
  FillAutofillFormDataAndSaveResults(form, *form.fields.begin(),
                                     MakeFrontendId({.credit_card_id = guid}),
                                     &response_data);
  ExpectFilledCreditCardFormElvis(response_data, false);
}

// Test that we correctly fill a credit card form with month input type.
// Test 1 of 4: Empty month, empty year
TEST_F(BrowserAutofillManagerTest, FillCreditCardForm_NoYearNoMonth) {
  personal_data().ClearCreditCards();
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "Miku Hatsune",
                          "4234567890654321",  // Visa
                          "", "", "1");
  credit_card.set_guid("00000000-0000-0000-0000-000000000007");
  personal_data().AddCreditCard(credit_card);
  // Set up our form data.
  FormData form = CreateTestCreditCardFormData(true, true);
  FormsSeen({form});

  const char guid[] = "00000000-0000-0000-0000-000000000007";
  FormData response_data;
  FillAutofillFormDataAndSaveResults(form, *form.fields.begin(),
                                     MakeFrontendId({.credit_card_id = guid}),
                                     &response_data);
  ExpectFilledCreditCardYearMonthWithYearMonth(response_data, false, "", "");
}

// Test that we correctly fill a credit card form with month input type.
// Test 2 of 4: Non-empty month, empty year
TEST_F(BrowserAutofillManagerTest, FillCreditCardForm_NoYearMonth) {
  personal_data().ClearCreditCards();
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "Miku Hatsune",
                          "4234567890654321",  // Visa
                          "04", "", "1");
  credit_card.set_guid("00000000-0000-0000-0000-000000000007");
  personal_data().AddCreditCard(credit_card);
  // Set up our form data.
  FormData form = CreateTestCreditCardFormData(true, true);
  FormsSeen({form});

  const char guid[] = "00000000-0000-0000-0000-000000000007";
  FormData response_data;
  FillAutofillFormDataAndSaveResults(form, *form.fields.begin(),
                                     MakeFrontendId({.credit_card_id = guid}),
                                     &response_data);
  ExpectFilledCreditCardYearMonthWithYearMonth(response_data, false, "", "04");
}

// Test that we correctly fill a credit card form with month input type.
// Test 3 of 4: Empty month, non-empty year
TEST_F(BrowserAutofillManagerTest, FillCreditCardForm_YearNoMonth) {
  // Same as the SetUp(), but generate 4 credit cards with year month
  // combination.
  personal_data().ClearCreditCards();
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "Miku Hatsune",
                          "4234567890654321",  // Visa
                          "", "2999", "1");
  credit_card.set_guid("00000000-0000-0000-0000-000000000007");
  personal_data().AddCreditCard(credit_card);
  // Set up our form data.
  FormData form = CreateTestCreditCardFormData(true, true);
  FormsSeen({form});

  const char guid[] = "00000000-0000-0000-0000-000000000007";
  FormData response_data;
  FillAutofillFormDataAndSaveResults(form, *form.fields.begin(),
                                     MakeFrontendId({.credit_card_id = guid}),
                                     &response_data);
  ExpectFilledCreditCardYearMonthWithYearMonth(response_data, false, "2999",
                                               "");
}

// Test that we correctly fill a credit card form with month input type.
// Test 4 of 4: Non-empty month, non-empty year
TEST_F(BrowserAutofillManagerTest, FillCreditCardForm_YearMonth) {
  personal_data().ClearCreditCards();
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "Miku Hatsune",
                          "4234567890654321",  // Visa
                          "04", "2999", "1");
  credit_card.set_guid("00000000-0000-0000-0000-000000000007");
  personal_data().AddCreditCard(credit_card);
  // Set up our form data.
  FormData form = CreateTestCreditCardFormData(true, true);
  FormsSeen({form});

  const char guid[] = "00000000-0000-0000-0000-000000000007";
  FormData response_data;
  FillAutofillFormDataAndSaveResults(form, *form.fields.begin(),
                                     MakeFrontendId({.credit_card_id = guid}),
                                     &response_data);
  ExpectFilledCreditCardYearMonthWithYearMonth(response_data, false, "2999",
                                               "04");
}

// Test that only the first 16 credit card number fields are filled.
TEST_F(BrowserAutofillManagerTest,
       FillOnlyFirstNineteenCreditCardNumberFields) {
  // Set up our form data.
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");

  FormFieldData field;
  test::CreateTestFormField("Card Name", "cardname", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last Name", "cardlastname", "", "text", &field);
  form.fields.push_back(field);

  // Add 20 credit card number fields with distinct names.
  for (int i = 0; i < 20; i++) {
    std::u16string field_name = u"Card Number " + base::NumberToString16(i + 1);
    test::CreateTestFormField(base::UTF16ToASCII(field_name).c_str(),
                              "cardnumber", "", "text", &field);
    form.fields.push_back(field);
  }

  test::CreateTestFormField("CVC", "cvc", "", "text", &field);
  form.fields.push_back(field);

  FormsSeen({form});

  const char guid[] = "00000000-0000-0000-0000-000000000004";
  FormData response_data;
  FillAutofillFormDataAndSaveResults(form, *form.fields.begin(),
                                     MakeFrontendId({.credit_card_id = guid}),
                                     &response_data);
  ExpectFilledField("Card Name", "cardname", "Elvis", "text",
                    response_data.fields[0]);
  ExpectFilledField("Last Name", "cardlastname", "Presley", "text",
                    response_data.fields[1]);

  // Verify that the first 19 credit card number fields are filled.
  for (int i = 0; i < 19; i++) {
    std::u16string field_name = u"Card Number " + base::NumberToString16(i + 1);
    ExpectFilledField(base::UTF16ToASCII(field_name).c_str(), "cardnumber",
                      "4234567890123456", "text", response_data.fields[2 + i]);
  }

  // Verify that the 20th. credit card number field is not filled.
  ExpectFilledField("Card Number 20", "cardnumber", "", "text",
                    response_data.fields[21]);

  ExpectFilledField("CVC", "cvc", "", "text", response_data.fields[22]);
}

// Test that only the first 16 of identical fields are filled.
TEST_F(BrowserAutofillManagerTest,
       FillOnlyFirstSixteenIdenticalCreditCardNumberFields) {
  // Set up our form data.
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");

  FormFieldData field;
  test::CreateTestFormField("Card Name", "cardname", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last Name", "cardlastname", "", "text", &field);
  form.fields.push_back(field);

  // Add 20 identical card number fields.
  for (int i = 0; i < 20; i++) {
    test::CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
    form.fields.push_back(field);
  }

  test::CreateTestFormField("CVC", "cvc", "", "text", &field);
  form.fields.push_back(field);

  FormsSeen({form});

  const char guid[] = "00000000-0000-0000-0000-000000000004";
  FormData response_data;
  FillAutofillFormDataAndSaveResults(form, *form.fields.begin(),
                                     MakeFrontendId({.credit_card_id = guid}),
                                     &response_data);
  ExpectFilledField("Card Name", "cardname", "Elvis", "text",
                    response_data.fields[0]);
  ExpectFilledField("Last Name", "cardlastname", "Presley", "text",
                    response_data.fields[1]);

  // Verify that the first 19 card number fields are filled.
  for (int i = 0; i < 19; i++) {
    ExpectFilledField("Card Number", "cardnumber", "4234567890123456", "text",
                      response_data.fields[2 + i]);
  }
  // Verify that the 20th. card number field is not filled.
  ExpectFilledField("Card Number", "cardnumber", "", "text",
                    response_data.fields[21]);

  ExpectFilledField("CVC", "cvc", "", "text", response_data.fields[22]);
}

// Test the credit card number is filled correctly into single-digit fields.
TEST_F(BrowserAutofillManagerTest, FillCreditCardNumberIntoSingleDigitFields) {
  // Set up our form data.
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");

  FormFieldData field;
  test::CreateTestFormField("Card Name", "cardname", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last Name", "cardlastname", "", "text", &field);
  form.fields.push_back(field);

  // Add 20 identical card number fields.
  for (int i = 0; i < 20; i++) {
    test::CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
    // Limit the length the field to 1.
    field.max_length = 1;
    form.fields.push_back(field);
  }

  test::CreateTestFormField("CVC", "cvc", "", "text", &field);
  form.fields.push_back(field);

  FormsSeen({form});

  const char guid[] = "00000000-0000-0000-0000-000000000004";
  FormData response_data;
  FillAutofillFormDataAndSaveResults(form, *form.fields.begin(),
                                     MakeFrontendId({.credit_card_id = guid}),
                                     &response_data);
  ExpectFilledField("Card Name", "cardname", "Elvis", "text",
                    response_data.fields[0]);
  ExpectFilledField("Last Name", "cardlastname", "Presley", "text",
                    response_data.fields[1]);

  // Verify that the first 19 card number fields are filled.
  std::u16string card_number = u"4234567890123456";
  for (unsigned int i = 0; i < 19; i++) {
    ExpectFilledField("Card Number", "cardnumber",
                      i < card_number.length()
                          ? base::UTF16ToASCII(card_number.substr(i, 1)).c_str()
                          : "4234567890123456",
                      "text", response_data.fields[2 + i]);
  }

  // Verify that the 20th. card number field is contains the full value.
  ExpectFilledField("Card Number", "cardnumber", "", "text",
                    response_data.fields[21]);

  ExpectFilledField("CVC", "cvc", "", "text", response_data.fields[22]);
}

// Test that we correctly fill a credit card form with first and last cardholder
// name.
TEST_F(BrowserAutofillManagerTest, FillCreditCardForm_SplitName) {
  // Set up our form data.
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");

  FormFieldData field;
  test::CreateTestFormField("Card Name", "cardname", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last Name", "cardlastname", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("CVC", "cvc", "", "text", &field);
  form.fields.push_back(field);

  FormsSeen({form});

  const char guid[] = "00000000-0000-0000-0000-000000000004";
  FormData response_data;
  FillAutofillFormDataAndSaveResults(form, *form.fields.begin(),
                                     MakeFrontendId({.credit_card_id = guid}),
                                     &response_data);
  ExpectFilledField("Card Name", "cardname", "Elvis", "text",
                    response_data.fields[0]);
  ExpectFilledField("Last Name", "cardlastname", "Presley", "text",
                    response_data.fields[1]);
  ExpectFilledField("Card Number", "cardnumber", "4234567890123456", "text",
                    response_data.fields[2]);
}

// Test that only filled selection boxes are counted for the type filling limit.
TEST_F(BrowserAutofillManagerTest,
       OnlyCountFilledSelectionBoxesForTypeFillingLimit) {
  test::PopulateAlternativeStateNameMapForTesting(
      "US", "Tennessee",
      {{.canonical_name = "Tennessee",
        .abbreviations = {"TN"},
        .alternative_names = {}}});
  // Set up our form data.
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");

  FormFieldData field;
  test::CreateTestFormField("First Name", "firstname", "", "text", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("Middle Name", "middlename", "", "text", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  form.fields.push_back(field);

  // Create a selection box for the state that hat the correct entry to be
  // filled with user data. Note, TN is the official abbreviation for Tennessee.
  test::CreateTestSelectField("State", "state", "", {"AA", "BB", "TN"},
                              {"AA", "BB", "TN"}, &field);
  form.fields.push_back(field);

  // Add 20 selection boxes that can not be filled since the correct entry
  // is missing.
  for (int i = 0; i < 20; i++) {
    test::CreateTestSelectField("State", "state", "", {"AA", "BB", "CC"},
                                {"AA", "BB", "CC"}, &field);
    form.fields.push_back(field);
  }

  // Add 20 other selection boxes that should be fillable since the correct
  // entry is present.
  for (int i = 0; i < 20; i++) {
    test::CreateTestSelectField("State", "state", "", {"AA", "BB", "TN"},
                                {"AA", "BB", "TN"}, &field);
    form.fields.push_back(field);
  }

  // Create a selection box for the state that hat the correct entry to be
  // filled with user data. Note, TN is the official abbreviation for Tennessee.
  for (int i = 0; i < 20; ++i) {
    test::CreateTestSelectField("Country", "country", "", {"DE", "FR", "US"},
                                {"DE", "FR", "US"}, &field);
    form.fields.push_back(field);
  }

  FormsSeen({form});

  AutofillProfile profile;
  const char guid[] = "00000000-0000-0000-0000-000000000123";
  test::SetProfileInfo(&profile, "Elvis", "Aaron", "Presley",
                       "theking@gmail.com", "1987", "3734 Elvis Presley Blvd.",
                       "Apt. 10", "Memphis", "Tennessee", "38116", "US",
                       "12345678901");
  profile.set_guid(guid);
  personal_data().AddProfile(profile);

  FormData response_data;
  FillAutofillFormDataAndSaveResults(form, *form.fields.begin(),
                                     MakeFrontendId({.profile_id = guid}),
                                     &response_data);

  // Verify the correct filling of the name entries.
  ExpectFilledField("First Name", "firstname", "Elvis", "text",
                    response_data.fields[0]);
  ExpectFilledField("Middle Name", "middlename", "Aaron", "text",
                    response_data.fields[1]);
  ExpectFilledField("Last Name", "lastname", "Presley", "text",
                    response_data.fields[2]);

  // Verify that the first selection box is correctly filled.
  ExpectFilledField("State", "state", "TN", "select-one",
                    response_data.fields[3]);

  // Verify that the next 20 selection boxes are not filled.
  for (int i = 0; i < 20; i++) {
    ExpectFilledField("State", "state", "", "select-one",
                      response_data.fields[4 + i]);
  }

  // Verify that the remaining selection boxes are correctly filled again
  // because there's no limit on filling ADDRESS_HOME_STATE fields.
  for (int i = 0; i < 20; i++) {
    ExpectFilledField("State", "state", "TN", "select-one",
                      response_data.fields[24 + i]);
  }

  // Verify that only the first 9 of the remaining selection boxes are
  // correctly filled due to the limit on filling ADDRESS_HOME_COUNTRY fields.
  for (int i = 0; i < 20; i++) {
    ExpectFilledField("Country", "country", i < 9 ? "US" : "", "select-one",
                      response_data.fields[44 + i]);
  }
}

// Test that we correctly fill a combined address and credit card form.
TEST_F(BrowserAutofillManagerTest, FillAddressAndCreditCardForm) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  CreateTestCreditCardFormData(&form, true, false);
  FormsSeen({form});

  // First fill the address data.
  const char guid[] = "00000000-0000-0000-0000-000000000001";
  FormData response_data;
  {
    SCOPED_TRACE("Address");
    FillAutofillFormDataAndSaveResults(form, form.fields[0],
                                       MakeFrontendId({.profile_id = guid}),
                                       &response_data);
    ExpectFilledAddressFormElvis(response_data, true);
  }

  // Now fill the credit card data.
  const char guid2[] = "00000000-0000-0000-0000-000000000004";
  {
    FillAutofillFormDataAndSaveResults(
        form, form.fields.back(), MakeFrontendId({.credit_card_id = guid2}),
        &response_data);
    SCOPED_TRACE("Credit card");
    ExpectFilledCreditCardFormElvis(response_data, true);
  }
}

// Test parameter data for tests with a simple structure: Create a form,
// autofill it, check that values have been correctly filled.
struct AutofillSimpleFormCase {
  struct FormFieldExpectedData {
    const char* label;
    const char* name;
    const char* value = "";
  };

  const std::string test_name;
  const std::string cc_guid = "";
  const std::string profile_guid = "00000000-0000-0000-0000-000000000001";

  const test::FormDescription form_description;
  const std::vector<FormFieldExpectedData> expected_form_fields;
};

class AutofillSimpleFormTest
    : public BrowserAutofillManagerTest,
      public ::testing::WithParamInterface<AutofillSimpleFormCase> {};

const AutofillSimpleFormCase kAutofillSimpleFormCases[] = {
    // Test that a field with an unrecognized autocomplete attribute is not
    // filled.
    {.test_name = "FillAddressForm_UnrecognizedAttribute",
     .form_description =
         {.fields = {{.label = u"First name",
                      .name = u"firstname",
                      .autocomplete_attribute = "given-name"},
                     {.label = u"Middle name", .name = u"middle"},
                     {.label = u"Last name",
                      .name = u"lastname",
                      .autocomplete_attribute = "unrecognized"}}},
     .expected_form_fields =
         {{.label = "First name", .name = "firstname", .value = "Elvis"},
          {.label = "Middle name", .name = "middle", .value = "Aaron"},
          {.label = "Last name", .name = "lastname"}}},

    // Test that non credit card related fields with the autocomplete attribute
    // set to off are filled on all platforms when the feature to autofill all
    // addresses is enabled (default).
    {.test_name = "FillAddressForm_AutocompleteOffNotRespected",
     .form_description =
         {.fields = {{.label = u"First name", .name = u"firstname"},
                     {.label = u"Middle name",
                      .name = u"middle",
                      .should_autocomplete = false},
                     {.label = u"Last name", .name = u"lastname"},
                     {.label = u"Address Line 1",
                      .name = u"addr1",
                      .should_autocomplete = false}}},
     .expected_form_fields =
         {{.label = "First name", .name = "firstname", .value = "Elvis"},
          {.label = "Middle name", .name = "middle", .value = "Aaron"},
          {.label = "Last name", .name = "lastname", .value = "Presley"},
          {.label = "Address Line 1",
           .name = "addr1",
           .value = "3734 Elvis Presley Blvd."}}},

    // Test that a field with a value equal to it's placeholder attribute is
    // filled.
    {.test_name = "FillAddressForm_PlaceholderEqualsValue",
     .form_description = {.fields = {{.label = u"First name",
                                      .name = u"firstname",
                                      .value = u"First Name",
                                      .placeholder = u"First Name"},
                                     {.label = u"Middle name",
                                      .name = u"middle",
                                      .value = u"Middle Name",
                                      .placeholder = u"Middle Name"},
                                     {.label = u"Last name",
                                      .name = u"lastname",
                                      .value = u"Last Name",
                                      .placeholder = u"Last Name"}}},
     .expected_form_fields =
         {{.label = "First name", .name = "firstname", .value = "Elvis"},
          {.label = "Middle name", .name = "middle", .value = "Aaron"},
          {.label = "Last name", .name = "lastname", .value = "Presley"}}},

    // Test that a credit card field with an unrecognized autocomplete attribute
    // gets filled.
    {.test_name = "FillCreditCardForm_UnrecognizedAttribute",
     .cc_guid = "00000000-0000-0000-0000-000000000004",
     .profile_guid = "",
     .form_description =
         {.fields = {{.label = u"Name on Card",
                      .name = u"nameoncard",
                      .autocomplete_attribute = "cc-name"},
                     {.label = u"Card Number", .name = u"cardnumber"},
                     {.label = u"Expiration Date",
                      .name = u"ccmonth",
                      .autocomplete_attribute = "unrecognized"}}},
     .expected_form_fields = {{.label = "Name on Card",
                               .name = "nameoncard",
                               .value = "Elvis Presley"},
                              {.label = "Card Number",
                               .name = "cardnumber",
                               .value = "4234567890123456"},
                              {.label = "Expiration Date",
                               .name = "ccmonth",
                               .value = "04/2999"}}},

};

TEST_P(AutofillSimpleFormTest, FillSimpleForm) {
  const AutofillSimpleFormCase& params = GetParam();
  FormData form = test::GetFormData(params.form_description);
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");

  FormsSeen({form});

  FormData response_data;
  FillAutofillFormDataAndSaveResults(
      form, form.fields[0],
      MakeFrontendId({.credit_card_id = params.cc_guid,
                      .profile_id = params.profile_guid}),
      &response_data);

  ASSERT_EQ(response_data.fields.size(), params.expected_form_fields.size());
  for (size_t i = 0; i < response_data.fields.size(); ++i) {
    SCOPED_TRACE(params.test_name + ", fields expectations");
    const auto& [label, name, value] = params.expected_form_fields[i];
    ExpectFilledField(label, name, value, "text", response_data.fields[i]);
  }
}

INSTANTIATE_TEST_SUITE_P(
    BrowserAutofillManagerTest,
    AutofillSimpleFormTest,
    ::testing::ValuesIn(kAutofillSimpleFormCases),
    [](const ::testing::TestParamInfo<AutofillSimpleFormTest::ParamType>&
           info) { return info.param.test_name; });

// Test that if a company is of a format of a birthyear and the relevant feature
// is enabled, we would not fill it.
TEST_F(BrowserAutofillManagerTest, FillAddressForm_CompanyBirthyear) {
  // Set up our form data.
  FormData address_form;
  address_form.name = u"MyForm";
  address_form.url = GURL("https://myform.com/form.html");
  address_form.action = GURL("https://myform.com/submit.html");

  FormFieldData field;
  test::CreateTestFormField("First name", "firstname", "", "text", &field);
  address_form.fields.push_back(field);
  test::CreateTestFormField("Middle name", "middle", "", "text", &field);
  address_form.fields.push_back(field);
  test::CreateTestFormField("Last name", "lastname", "", "text", &field);
  address_form.fields.push_back(field);
  test::CreateTestFormField("Company", "company", "", "text", &field);
  address_form.fields.push_back(field);

  FormsSeen({address_form});

  AutofillProfile profile;
  const char guid[] = "00000000-0000-0000-0000-000000000123";
  test::SetProfileInfo(&profile, "Elvis", "Aaron", "Presley",
                       "theking@gmail.com", "1987", "3734 Elvis Presley Blvd.",
                       "Apt. 10", "Memphis", "Tennessee", "38116", "US",
                       "12345678901");
  profile.set_guid(guid);
  personal_data().AddProfile(profile);

  FormData response_data;
  FillAutofillFormDataAndSaveResults(address_form, *address_form.fields.begin(),
                                     MakeFrontendId({.profile_id = guid}),
                                     &response_data);

  // All the fields should be filled except the company.
  ExpectFilledField("First name", "firstname", "Elvis", "text",
                    response_data.fields[0]);
  ExpectFilledField("Middle name", "middle", "Aaron", "text",
                    response_data.fields[1]);
  ExpectFilledField("Last name", "lastname", "Presley", "text",
                    response_data.fields[2]);
  ExpectFilledField("Company", "company", "", "text", response_data.fields[3]);
}

// Test that credit card fields are filled even if they have the autocomplete
// attribute set to off.
TEST_F(BrowserAutofillManagerTest, FillCreditCardForm_AutocompleteOff) {
  // Set up our form data.
  FormData form = CreateTestCreditCardFormData(true, false);

  // Set the autocomplete=off on all fields.
  for (FormFieldData field : form.fields) {
    field.should_autocomplete = false;
  }

  FormsSeen({form});

  const char guid[] = "00000000-0000-0000-0000-000000000004";
  FormData response_data;
  FillAutofillFormDataAndSaveResults(form, *form.fields.begin(),
                                     MakeFrontendId({.credit_card_id = guid}),
                                     &response_data);

  // All fields should be filled.
  ExpectFilledCreditCardFormElvis(response_data, false);
}

// Test that selecting an expired credit card fills everything except the
// expiration date.
TEST_F(BrowserAutofillManagerTest, FillCreditCardForm_ExpiredCard) {
  personal_data().ClearCreditCards();
  CreditCard expired_card;
  test::SetCreditCardInfo(&expired_card, "Homer Simpson",
                          "4234567890654321",  // Visa
                          "05", "2000", "1");
  expired_card.set_guid("00000000-0000-0000-0000-000000000009");
  personal_data().AddCreditCard(expired_card);

  // Set up the form data.
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");

  // Create a credit card form.
  FormFieldData field;
  test::CreateTestFormField("Name on Card", "nameoncard", "", "text", "cc-name",
                            &field);
  form.fields.push_back(field);
  std::vector<const char*> kCreditCardTypes = {"Visa", "Mastercard", "AmEx",
                                               "discover"};
  test::CreateTestSelectField("Card Type", "cardtype", "", "cc-type",
                              kCreditCardTypes, kCreditCardTypes, &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Card Number", "cardnumber", "", "text",
                            "cc-number", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Expiration Month", "ccmonth", "", "text",
                            "cc-exp-month", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Expiration Year", "ccyear", "", "text",
                            "cc-exp-year", &field);
  form.fields.push_back(field);
  FormsSeen({form});

  const char guid[] = "00000000-0000-0000-0000-000000000009";
  FormData response_data;
  FillAutofillFormDataAndSaveResults(form, *form.fields.begin(),
                                     MakeFrontendId({.credit_card_id = guid}),
                                     &response_data);

  // The credit card name, type and number should be filled.
  ExpectFilledField("Name on Card", "nameoncard", "Homer Simpson", "text",
                    response_data.fields[0]);
  ExpectFilledField("Card Type", "cardtype", "Visa", "select-one",
                    response_data.fields[1]);
  ExpectFilledField("Card Number", "cardnumber", "4234567890654321", "text",
                    response_data.fields[2]);

  // The expiration month and year should not be filled.
  ExpectFilledField("Expiration Month", "ccmonth", "", "text",
                    response_data.fields[3]);
  ExpectFilledField("Expiration Year", "ccyear", "", "text",
                    response_data.fields[4]);
}

TEST_F(BrowserAutofillManagerTest, PreviewCreditCardForm_VirtualCard) {
  personal_data().ClearCreditCards();
  CreditCard virtual_card = test::GetVirtualCard();
  personal_data().AddServerCreditCard(virtual_card);
  // Set up our form data.
  FormData form = CreateTestCreditCardFormData(true, false);
  FormsSeen({form});

  FormData response_data;
  PreviewVirtualCardDataAndSaveResults(mojom::RendererFormDataAction::kPreview,
                                       virtual_card.guid(), form,
                                       form.fields[1], &response_data);

  std::u16string expected_cardholder_name = u"Lorem Ipsum";
  // Virtual card number using obfuscated dots only: Virtual card Mastercard
  // ••••4444
  std::u16string expected_card_number =
      u"Virtual card Mastercard  " +
      virtual_card.ObfuscatedNumberWithVisibleLastFourDigits();
  // Virtual card expiration month using obfuscated dots: ••
  std::u16string expected_exp_month = CreditCard::GetMidlineEllipsisDots(2);
  // Virtual card expiration year using obfuscated dots: ••••
  std::u16string expected_exp_year = CreditCard::GetMidlineEllipsisDots(4);
  // Virtual card cvc using obfuscated dots: •••
  std::u16string expected_cvc = CreditCard::GetMidlineEllipsisDots(3);

  EXPECT_EQ(response_data.fields[0].value, expected_cardholder_name);
  EXPECT_EQ(response_data.fields[1].value, expected_card_number);
  EXPECT_EQ(response_data.fields[2].value, expected_exp_month);
  EXPECT_EQ(response_data.fields[3].value, expected_exp_year);
  EXPECT_EQ(response_data.fields[4].value, expected_cvc);
}

// Test that non-focusable field is ignored while inferring boundaries between
// sections, but not filled.
TEST_F(BrowserAutofillManagerTest, FillFormWithNonFocusableFields) {
  // Create a form with both focusable and non-focusable fields.
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");

  FormFieldData field;

  test::CreateTestFormField("First Name", "firstname", "", "text", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("", "lastname", "", "text", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("", "email", "", "text", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("Phone Number", "phonenumber", "", "tel", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("", "email_", "", "text", &field);
  field.is_focusable = false;
  form.fields.push_back(field);

  test::CreateTestFormField("Country", "country", "", "text", &field);
  form.fields.push_back(field);

  FormsSeen({form});

  // Fill the form
  const char guid[] = "00000000-0000-0000-0000-000000000001";
  FormData response_data;
  FillAutofillFormDataAndSaveResults(form, form.fields[0],
                                     MakeFrontendId({.profile_id = guid}),
                                     &response_data);

  // All the visible fields should be filled as all the fields belong to the
  // same logical section.
  ASSERT_EQ(6U, response_data.fields.size());
  ExpectFilledField("First Name", "firstname", "Elvis", "text",
                    response_data.fields[0]);
  ExpectFilledField("", "lastname", "Presley", "text", response_data.fields[1]);
  ExpectFilledField("", "email", "theking@gmail.com", "text",
                    response_data.fields[2]);
  ExpectFilledField("Phone Number", "phonenumber", "12345678901", "tel",
                    response_data.fields[3]);
  ExpectFilledField("", "email_", "", "text", response_data.fields[4]);
  ExpectFilledField("Country", "country", "United States", "text",
                    response_data.fields[5]);
}

// Test that we correctly fill a form that has multiple logical sections, e.g.
// both a billing and a shipping address.
TEST_F(BrowserAutofillManagerTest, FillFormWithMultipleSections) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  const size_t kAddressFormSize = form.fields.size();
  test::CreateTestAddressFormData(&form);
  for (size_t i = kAddressFormSize; i < form.fields.size(); ++i) {
    // Make sure the fields have distinct names.
    form.fields[i].name = form.fields[i].name + u"_";
  }
  FormsSeen({form});

  // Fill the first section.
  const char guid[] = "00000000-0000-0000-0000-000000000001";
  FormData response_data;
  FillAutofillFormDataAndSaveResults(form, form.fields[0],
                                     MakeFrontendId({.profile_id = guid}),
                                     &response_data);
  {
    SCOPED_TRACE("Address 1");
    // The second address section should be empty.
    ASSERT_EQ(response_data.fields.size(), 2 * kAddressFormSize);
    for (size_t i = kAddressFormSize; i < form.fields.size(); ++i) {
      EXPECT_EQ(std::u16string(), response_data.fields[i].value);
    }

    // The first address section should be filled with Elvis's data.
    response_data.fields.resize(kAddressFormSize);
    ExpectFilledAddressFormElvis(response_data, false);
  }

  // Fill the second section, with the initiating field somewhere in the middle
  // of the section.
  const char guid2[] = "00000000-0000-0000-0000-000000000001";
  ASSERT_LT(9U, kAddressFormSize);
  FillAutofillFormDataAndSaveResults(form, form.fields[kAddressFormSize + 9],
                                     MakeFrontendId({.profile_id = guid2}),
                                     &response_data);
  {
    SCOPED_TRACE("Address 2");
    ASSERT_EQ(response_data.fields.size(), form.fields.size());

    // The first address section should be empty.
    ASSERT_EQ(response_data.fields.size(), 2 * kAddressFormSize);
    for (size_t i = 0; i < kAddressFormSize; ++i) {
      EXPECT_EQ(std::u16string(), response_data.fields[i].value);
    }

    // The second address section should be filled with Elvis's data.
    FormData secondSection = response_data;
    secondSection.fields.erase(secondSection.fields.begin(),
                               secondSection.fields.begin() + kAddressFormSize);
    for (size_t i = 0; i < kAddressFormSize; ++i) {
      // Restore the expected field names.
      std::u16string name = secondSection.fields[i].name;
      std::u16string original_name = name.substr(0, name.size() - 1);
      secondSection.fields[i].name = original_name;
    }
    ExpectFilledAddressFormElvis(secondSection, false);
  }
}

// Test that we correctly fill a form that has author-specified sections, which
// might not match our expected section breakdown.
TEST_F(BrowserAutofillManagerTest, FillFormWithAuthorSpecifiedSections) {
  // Create a form with a billing section and an unnamed section, interleaved.
  // The billing section includes both address and credit card fields.
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");

  FormFieldData field;

  test::CreateTestFormField("", "country", "", "text",
                            "section-billing country", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("", "firstname", "", "text", "given-name", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("", "lastname", "", "text", "family-name", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("", "address", "", "text",
                            "section-billing address-line1", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("", "city", "", "text", "section-billing locality",
                            &field);
  form.fields.push_back(field);

  test::CreateTestFormField("", "state", "", "text", "section-billing region",
                            &field);
  form.fields.push_back(field);

  test::CreateTestFormField("", "zip", "", "text",
                            "section-billing postal-code", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("", "ccname", "", "text", "section-billing cc-name",
                            &field);
  form.fields.push_back(field);

  test::CreateTestFormField("", "ccnumber", "", "text",
                            "section-billing cc-number", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("", "ccexp", "", "text", "section-billing cc-exp",
                            &field);
  form.fields.push_back(field);

  test::CreateTestFormField("", "email", "", "text", "email", &field);
  form.fields.push_back(field);

  FormsSeen({form});

  // Fill the unnamed section.
  const char guid[] = "00000000-0000-0000-0000-000000000001";
  FormData response_data;
  FillAutofillFormDataAndSaveResults(form, form.fields[1],
                                     MakeFrontendId({.profile_id = guid}),
                                     &response_data);
  {
    SCOPED_TRACE("Unnamed section");
    EXPECT_EQ(u"MyForm", response_data.name);
    EXPECT_EQ(GURL("https://myform.com/form.html"), response_data.url);
    EXPECT_EQ(GURL("https://myform.com/submit.html"), response_data.action);
    ASSERT_EQ(11U, response_data.fields.size());

    ExpectFilledField("", "country", "", "text", response_data.fields[0]);
    ExpectFilledField("", "firstname", "Elvis", "text",
                      response_data.fields[1]);
    ExpectFilledField("", "lastname", "Presley", "text",
                      response_data.fields[2]);
    ExpectFilledField("", "address", "", "text", response_data.fields[3]);
    ExpectFilledField("", "city", "", "text", response_data.fields[4]);
    ExpectFilledField("", "state", "", "text", response_data.fields[5]);
    ExpectFilledField("", "zip", "", "text", response_data.fields[6]);
    ExpectFilledField("", "ccname", "", "text", response_data.fields[7]);
    ExpectFilledField("", "ccnumber", "", "text", response_data.fields[8]);
    ExpectFilledField("", "ccexp", "", "text", response_data.fields[9]);
    ExpectFilledField("", "email", "theking@gmail.com", "text",
                      response_data.fields[10]);
  }

  // Fill the address portion of the billing section.
  const char guid2[] = "00000000-0000-0000-0000-000000000001";
  FillAutofillFormDataAndSaveResults(form, form.fields[0],
                                     MakeFrontendId({.profile_id = guid2}),
                                     &response_data);
  {
    SCOPED_TRACE("Billing address");
    EXPECT_EQ(u"MyForm", response_data.name);
    EXPECT_EQ(GURL("https://myform.com/form.html"), response_data.url);
    EXPECT_EQ(GURL("https://myform.com/submit.html"), response_data.action);
    ASSERT_EQ(11U, response_data.fields.size());

    ExpectFilledField("", "country", "US", "text", response_data.fields[0]);
    ExpectFilledField("", "firstname", "", "text", response_data.fields[1]);
    ExpectFilledField("", "lastname", "", "text", response_data.fields[2]);
    ExpectFilledField("", "address", "3734 Elvis Presley Blvd.", "text",
                      response_data.fields[3]);
    ExpectFilledField("", "city", "Memphis", "text", response_data.fields[4]);
    ExpectFilledField("", "state", "Tennessee", "text",
                      response_data.fields[5]);
    ExpectFilledField("", "zip", "38116", "text", response_data.fields[6]);
    ExpectFilledField("", "ccname", "", "text", response_data.fields[7]);
    ExpectFilledField("", "ccnumber", "", "text", response_data.fields[8]);
    ExpectFilledField("", "ccexp", "", "text", response_data.fields[9]);
    ExpectFilledField("", "email", "", "text", response_data.fields[10]);
  }

  // Fill the credit card portion of the billing section.
  const char guid3[] = "00000000-0000-0000-0000-000000000004";
  FillAutofillFormDataAndSaveResults(form, form.fields[form.fields.size() - 2],
                                     MakeFrontendId({.credit_card_id = guid3}),
                                     &response_data);
  {
    SCOPED_TRACE("Credit card");
    EXPECT_EQ(u"MyForm", response_data.name);
    EXPECT_EQ(GURL("https://myform.com/form.html"), response_data.url);
    EXPECT_EQ(GURL("https://myform.com/submit.html"), response_data.action);
    ASSERT_EQ(11U, response_data.fields.size());

    ExpectFilledField("", "country", "", "text", response_data.fields[0]);
    ExpectFilledField("", "firstname", "", "text", response_data.fields[1]);
    ExpectFilledField("", "lastname", "", "text", response_data.fields[2]);
    ExpectFilledField("", "address", "", "text", response_data.fields[3]);
    ExpectFilledField("", "city", "", "text", response_data.fields[4]);
    ExpectFilledField("", "state", "", "text", response_data.fields[5]);
    ExpectFilledField("", "zip", "", "text", response_data.fields[6]);
    ExpectFilledField("", "ccname", "Elvis Presley", "text",
                      response_data.fields[7]);
    ExpectFilledField("", "ccnumber", "4234567890123456", "text",
                      response_data.fields[8]);
    ExpectFilledField("", "ccexp", "04/2999", "text", response_data.fields[9]);
    ExpectFilledField("", "email", "", "text", response_data.fields[10]);
  }
}

// Test that we correctly fill a form that has a single logical section with
// multiple email address fields.
TEST_F(BrowserAutofillManagerTest, FillFormWithMultipleEmails) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  FormFieldData field;
  test::CreateTestFormField("Confirm email", "email2", "", "text", &field);
  form.fields.push_back(field);

  FormsSeen({form});

  // Fill the form.
  const char guid[] = "00000000-0000-0000-0000-000000000001";
  FormData response_data;
  FillAutofillFormDataAndSaveResults(form, form.fields[0],
                                     MakeFrontendId({.profile_id = guid}),
                                     &response_data);

  // The second email address should be filled.
  EXPECT_EQ(u"theking@gmail.com", response_data.fields.back().value);

  // The remainder of the form should be filled as usual.
  response_data.fields.pop_back();
  ExpectFilledAddressFormElvis(response_data, false);
}

// Test that we correctly fill a previously auto-filled form.
TEST_F(BrowserAutofillManagerTest, FillAutofilledForm) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  // Mark the address fields as autofilled.
  for (auto& field : form.fields) {
    field.is_autofilled = true;
  }

  CreateTestCreditCardFormData(&form, true, false);
  FormsSeen({form});

  // First fill the address data.
  const char guid[] = "00000000-0000-0000-0000-000000000001";
  FormData response_data;
  FillAutofillFormDataAndSaveResults(form, *form.fields.begin(),
                                     MakeFrontendId({.profile_id = guid}),
                                     &response_data);
  {
    SCOPED_TRACE("Address");
    ExpectFilledForm(response_data, "Elvis", "", "", "", "", "", "", "", "", "",
                     "", "", "", "", "", true, true, false);
  }

  // Now fill the credit card data.
  const char guid2[] = "00000000-0000-0000-0000-000000000004";
  FillAutofillFormDataAndSaveResults(form, form.fields.back(),
                                     MakeFrontendId({.credit_card_id = guid2}),
                                     &response_data);
  {
    SCOPED_TRACE("Credit card 1");
    ExpectFilledCreditCardFormElvis(response_data, true);
  }

  // Now set the credit card fields to also be auto-filled, and try again to
  // fill the credit card data
  for (auto& field : form.fields) {
    field.is_autofilled = true;
  }

  FillAutofillFormDataAndSaveResults(form, form.fields[form.fields.size() - 2],
                                     MakeFrontendId({.credit_card_id = guid2}),
                                     &response_data);
  {
    SCOPED_TRACE("Credit card 2");
    ExpectFilledForm(response_data, "", "", "", "", "", "", "", "", "", "", "",
                     "", "", "", "2999", true, true, false);
  }
}

// Test that we correctly fill a previously partly auto-filled form.
TEST_F(BrowserAutofillManagerTest, FillPartlyAutofilledForm) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  // Mark couple of the address fields as autofilled.
  form.fields[3].is_autofilled = true;
  form.fields[4].is_autofilled = true;
  form.fields[5].is_autofilled = true;
  form.fields[6].is_autofilled = true;
  form.fields[10].is_autofilled = true;

  CreateTestCreditCardFormData(&form, true, false);
  FormsSeen({form});

  // First fill the address data.
  const char guid[] = "00000000-0000-0000-0000-000000000001";
  FormData response_data;
  FillAutofillFormDataAndSaveResults(form, *form.fields.begin(),
                                     MakeFrontendId({.profile_id = guid}),
                                     &response_data);
  {
    SCOPED_TRACE("Address");
    ExpectFilledForm(response_data, "Elvis", "Aaron", "Presley", "", "", "", "",
                     "38116", "United States", "12345678901", "", "", "", "",
                     "", true, true, false);
  }

  // Now fill the credit card data.
  const char guid2[] = "00000000-0000-0000-0000-000000000004";
  FillAutofillFormDataAndSaveResults(form, form.fields.back(),
                                     MakeFrontendId({.credit_card_id = guid2}),
                                     &response_data);
  {
    SCOPED_TRACE("Credit card 1");
    ExpectFilledCreditCardFormElvis(response_data, true);
  }
}

// Test that we correctly fill a previously partly auto-filled form.
TEST_F(BrowserAutofillManagerTest, FillPartlyManuallyFilledForm) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);

  CreateTestCreditCardFormData(&form, true, false);
  FormsSeen({form});

  // Michael will be overridden with Elvis because Autofill is triggered from
  // the first field.
  form.fields[0].value = u"Michael";
  form.fields[0].properties_mask |= kUserTyped;

  // Jackson will be preserved.
  form.fields[2].value = u"Jackson";
  form.fields[2].properties_mask |= kUserTyped;

  FormsSeen({form});

  // First fill the address data.
  const char guid[] = "00000000-0000-0000-0000-000000000001";
  FormData response_data;
  FillAutofillFormDataAndSaveResults(form, *form.fields.begin(),
                                     MakeFrontendId({.profile_id = guid}),
                                     &response_data);
  {
    SCOPED_TRACE("Address");
    ExpectFilledForm(response_data, "Elvis", "Aaron", "Jackson",
                     "3734 Elvis Presley Blvd.", "Apt. 10", "Memphis",
                     "Tennessee", "38116", "United States", "12345678901",
                     "theking@gmail.com", "", "", "", "", true, true, false);
  }

  // Now fill the credit card data.
  const char guid2[] = "00000000-0000-0000-0000-000000000004";
  FillAutofillFormDataAndSaveResults(form, form.fields.back(),
                                     MakeFrontendId({.credit_card_id = guid2}),
                                     &response_data);
  {
    SCOPED_TRACE("Credit card 1");
    ExpectFilledForm(response_data, "Michael", "", "Jackson", "", "", "", "",
                     "", "", "", "", "Elvis Presley", "4234567890123456", "04",
                     "2999", true, true, false);
  }
}

// Test that we correctly fill a phone number split across multiple fields.
TEST_F(BrowserAutofillManagerTest, FillPhoneNumber) {
  // In one form, rely on the max length attribute to imply US phone number
  // parts. In the other form, rely on the autocomplete type attribute.
  FormData form_with_us_number_max_length;
  form_with_us_number_max_length.unique_renderer_id =
      test::MakeFormRendererId();
  form_with_us_number_max_length.name = u"MyMaxlengthPhoneForm";
  form_with_us_number_max_length.url =
      GURL("https://myform.com/phone_form.html");
  form_with_us_number_max_length.action =
      GURL("https://myform.com/phone_submit.html");
  FormData form_with_autocompletetype = form_with_us_number_max_length;
  form_with_autocompletetype.unique_renderer_id = test::MakeFormRendererId();
  form_with_autocompletetype.name = u"MyAutocompletetypePhoneForm";

  struct {
    const char* label;
    const char* name;
    size_t max_length;
    const char* autocomplete_attribute;
  } test_fields[] = {{"country code", "country_code", 1, "tel-country-code"},
                     {"area code", "area_code", 3, "tel-area-code"},
                     {"phone", "phone_prefix", 3, "tel-local-prefix"},
                     {"-", "phone_suffix", 4, "tel-local-suffix"},
                     {"Phone Extension", "ext", 3, "tel-extension"}};

  FormFieldData field;
  const size_t default_max_length = field.max_length;
  for (const auto& test_field : test_fields) {
    test::CreateTestFormField(test_field.label, test_field.name, "", "text", "",
                              test_field.max_length, &field);
    form_with_us_number_max_length.fields.push_back(field);

    field.max_length = default_max_length;
    field.autocomplete_attribute = test_field.autocomplete_attribute;
    field.parsed_autocomplete = ParseAutocompleteAttribute(
        test_field.autocomplete_attribute, default_max_length);
    form_with_autocompletetype.fields.push_back(field);
  }

  FormsSeen({form_with_us_number_max_length, form_with_autocompletetype});

  // We should be able to fill prefix and suffix fields for US numbers.
  AutofillProfile* work_profile =
      personal_data().GetProfileByGUID("00000000-0000-0000-0000-000000000002");
  ASSERT_TRUE(work_profile != nullptr);
  work_profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"16505554567");

  std::string guid(work_profile->guid());
  FormData response_data1;
  FillAutofillFormDataAndSaveResults(
      form_with_us_number_max_length,
      *form_with_us_number_max_length.fields.begin(),
      MakeFrontendId({.profile_id = guid}), &response_data1);

  ASSERT_EQ(5U, response_data1.fields.size());
  EXPECT_EQ(u"1", response_data1.fields[0].value);
  EXPECT_EQ(u"650", response_data1.fields[1].value);
  EXPECT_EQ(u"555", response_data1.fields[2].value);
  EXPECT_EQ(u"4567", response_data1.fields[3].value);
  EXPECT_EQ(std::u16string(), response_data1.fields[4].value);

  FormData response_data2;
  FillAutofillFormDataAndSaveResults(
      form_with_autocompletetype, *form_with_autocompletetype.fields.begin(),
      MakeFrontendId({.profile_id = guid}), &response_data2);

  ASSERT_EQ(5U, response_data2.fields.size());
  EXPECT_EQ(u"1", response_data2.fields[0].value);
  EXPECT_EQ(u"650", response_data2.fields[1].value);
  EXPECT_EQ(u"555", response_data2.fields[2].value);
  EXPECT_EQ(u"4567", response_data2.fields[3].value);
  EXPECT_EQ(std::u16string(), response_data2.fields[4].value);

  // For other countries, fill prefix and suffix fields with best effort.
  work_profile->SetRawInfo(ADDRESS_HOME_COUNTRY, u"GB");
  work_profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"447700954321");
  FormData response_data3;
  FillAutofillFormDataAndSaveResults(
      form_with_us_number_max_length,
      *form_with_us_number_max_length.fields.begin(),
      MakeFrontendId({.profile_id = guid}), &response_data3);

  ASSERT_EQ(5U, response_data3.fields.size());
  EXPECT_EQ(u"4", response_data3.fields[0].value);
  EXPECT_EQ(u"700", response_data3.fields[1].value);
  EXPECT_EQ(u"95", response_data3.fields[2].value);
  EXPECT_EQ(u"4321", response_data3.fields[3].value);
  EXPECT_EQ(std::u16string(), response_data3.fields[4].value);

  FormData response_data4;
  FillAutofillFormDataAndSaveResults(
      form_with_autocompletetype, *form_with_autocompletetype.fields.begin(),
      MakeFrontendId({.profile_id = guid}), &response_data4);

  ASSERT_EQ(5U, response_data4.fields.size());
  EXPECT_EQ(u"44", response_data4.fields[0].value);
  EXPECT_EQ(u"7700", response_data4.fields[1].value);
  EXPECT_EQ(u"95", response_data4.fields[2].value);
  EXPECT_EQ(u"4321", response_data4.fields[3].value);
  EXPECT_EQ(std::u16string(), response_data4.fields[4].value);
}

TEST_F(BrowserAutofillManagerTest, FillFirstPhoneNumber_ComponentizedNumbers) {
  AutofillProfile* work_profile =
      personal_data().GetProfileByGUID("00000000-0000-0000-0000-000000000002");
  ASSERT_TRUE(work_profile != nullptr);
  work_profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"16505554567");

  std::string guid(work_profile->guid());

  // Verify only the first complete number is filled when there are multiple
  // componentized number fields.
  FormData form_with_multiple_componentized_phone_fields;
  form_with_multiple_componentized_phone_fields.unique_renderer_id =
      test::MakeFormRendererId();
  form_with_multiple_componentized_phone_fields.url =
      GURL("https://www.foo.com/");

  FormFieldData field;
  // Default is zero, have to set to a number autofill can process.
  field.max_length = 10;
  form_with_multiple_componentized_phone_fields.name =
      u"multiple_componentized_number_fields";
  test::CreateTestFormField("Full Name", "full_name", "", "text", &field);
  form_with_multiple_componentized_phone_fields.fields.push_back(field);
  test::CreateTestFormField("country code", "country_code", "", "text", &field);
  form_with_multiple_componentized_phone_fields.fields.push_back(field);
  test::CreateTestFormField("area code", "area_code", "", "text", &field);
  form_with_multiple_componentized_phone_fields.fields.push_back(field);
  test::CreateTestFormField("number", "phone_number", "", "text", &field);
  form_with_multiple_componentized_phone_fields.fields.push_back(field);
  test::CreateTestFormField("extension", "extension", "", "text", &field);
  form_with_multiple_componentized_phone_fields.fields.push_back(field);
  test::CreateTestFormField("shipping country code", "shipping_country_code",
                            "", "text", &field);
  form_with_multiple_componentized_phone_fields.fields.push_back(field);
  test::CreateTestFormField("shipping area code", "shipping_area_code", "",
                            "text", &field);
  form_with_multiple_componentized_phone_fields.fields.push_back(field);
  test::CreateTestFormField("shipping number", "shipping_phone_number", "",
                            "text", &field);
  form_with_multiple_componentized_phone_fields.fields.push_back(field);

  FormsSeen({form_with_multiple_componentized_phone_fields});
  FormData response_data;
  FillAutofillFormDataAndSaveResults(
      form_with_multiple_componentized_phone_fields,
      *form_with_multiple_componentized_phone_fields.fields.begin(),
      MakeFrontendId({.profile_id = guid}), &response_data);

  // Verify only the first complete set of phone number fields are filled.
  ASSERT_EQ(8U, response_data.fields.size());
  EXPECT_EQ(u"Charles Hardin Holley", response_data.fields[0].value);
  EXPECT_EQ(u"1", response_data.fields[1].value);
  EXPECT_EQ(u"650", response_data.fields[2].value);
  EXPECT_EQ(u"5554567", response_data.fields[3].value);
  EXPECT_EQ(std::u16string(), response_data.fields[4].value);
  EXPECT_EQ(std::u16string(), response_data.fields[5].value);
  EXPECT_EQ(std::u16string(), response_data.fields[6].value);
  EXPECT_EQ(std::u16string(), response_data.fields[7].value);
}

TEST_F(BrowserAutofillManagerTest, FillFirstPhoneNumber_WholeNumbers) {
  AutofillProfile* work_profile =
      personal_data().GetProfileByGUID("00000000-0000-0000-0000-000000000002");
  ASSERT_TRUE(work_profile != nullptr);
  work_profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"16505554567");

  std::string guid(work_profile->guid());

  FormData form_with_multiple_whole_number_fields;
  form_with_multiple_whole_number_fields.unique_renderer_id =
      test::MakeFormRendererId();
  form_with_multiple_whole_number_fields.url = GURL("https://www.foo.com/");

  FormFieldData field;
  // Default is zero, have to set to a number autofill can process.
  field.max_length = 10;
  form_with_multiple_whole_number_fields.name = u"multiple_whole_number_fields";
  test::CreateTestFormField("Full Name", "full_name", "", "text", &field);
  form_with_multiple_whole_number_fields.fields.push_back(field);
  test::CreateTestFormField("number", "phone_number", "", "text", &field);
  form_with_multiple_whole_number_fields.fields.push_back(field);
  test::CreateTestFormField("extension", "extension", "", "text", &field);
  form_with_multiple_whole_number_fields.fields.push_back(field);
  test::CreateTestFormField("shipping number", "shipping_phone_number", "",
                            "text", &field);
  form_with_multiple_whole_number_fields.fields.push_back(field);

  FormsSeen({form_with_multiple_whole_number_fields});
  FormData response_data;
  FillAutofillFormDataAndSaveResults(
      form_with_multiple_whole_number_fields,
      *form_with_multiple_whole_number_fields.fields.begin(),
      MakeFrontendId({.profile_id = guid}), &response_data);

  // Verify only the first complete set of phone number fields are filled.
  ASSERT_EQ(4U, response_data.fields.size());
  EXPECT_EQ(u"Charles Hardin Holley", response_data.fields[0].value);
  EXPECT_EQ(u"6505554567", response_data.fields[1].value);
  EXPECT_EQ(std::u16string(), response_data.fields[2].value);
  EXPECT_EQ(std::u16string(), response_data.fields[3].value);
}

TEST_F(BrowserAutofillManagerTest, FillFirstPhoneNumber_FillPartsOnceOnly) {
  AutofillProfile* work_profile =
      personal_data().GetProfileByGUID("00000000-0000-0000-0000-000000000002");
  ASSERT_TRUE(work_profile != nullptr);
  work_profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"16505554567");

  std::string guid(work_profile->guid());

  // Verify only the first complete number is filled when there are multiple
  // componentized number fields.
  FormData form_with_multiple_componentized_phone_fields;
  form_with_multiple_componentized_phone_fields.unique_renderer_id =
      test::MakeFormRendererId();
  form_with_multiple_componentized_phone_fields.url =
      GURL("https://www.foo.com/");

  FormFieldData field;
  // Default is zero, have to set to a number autofill can process.
  field.max_length = 10;
  form_with_multiple_componentized_phone_fields.name =
      u"multiple_componentized_number_fields";
  test::CreateTestFormField("Full Name", "full_name", "", "text", &field);
  form_with_multiple_componentized_phone_fields.fields.push_back(field);
  test::CreateTestFormField("country code", "country_code", "", "text", &field);
  form_with_multiple_componentized_phone_fields.fields.push_back(field);
  test::CreateTestFormField("area code", "area_code", "", "text", &field);
  form_with_multiple_componentized_phone_fields.fields.push_back(field);
  test::CreateTestFormField("number", "phone_number", "", "text",
                            "tel-national", &field);
  form_with_multiple_componentized_phone_fields.fields.push_back(field);
  test::CreateTestFormField("extension", "extension", "", "text", "", &field);
  form_with_multiple_componentized_phone_fields.fields.push_back(field);
  test::CreateTestFormField("shipping country code", "shipping_country_code",
                            "", "text", &field);
  form_with_multiple_componentized_phone_fields.fields.push_back(field);
  test::CreateTestFormField("shipping area code", "shipping_area_code", "",
                            "text", &field);
  form_with_multiple_componentized_phone_fields.fields.push_back(field);
  test::CreateTestFormField("shipping number", "shipping_phone_number", "",
                            "text", &field);
  form_with_multiple_componentized_phone_fields.fields.push_back(field);

  FormsSeen({form_with_multiple_componentized_phone_fields});
  FormData response_data;
  FillAutofillFormDataAndSaveResults(
      form_with_multiple_componentized_phone_fields,
      *form_with_multiple_componentized_phone_fields.fields.begin(),
      MakeFrontendId({.profile_id = guid}), &response_data);

  // Verify only the first complete set of phone number fields are filled,
  // and phone components are not filled more than once.
  ASSERT_EQ(8U, response_data.fields.size());
  EXPECT_EQ(u"Charles Hardin Holley", response_data.fields[0].value);
  EXPECT_EQ(u"1", response_data.fields[1].value);
  EXPECT_EQ(std::u16string(), response_data.fields[2].value);
  EXPECT_EQ(u"6505554567", response_data.fields[3].value);
  EXPECT_EQ(std::u16string(), response_data.fields[4].value);
  EXPECT_EQ(std::u16string(), response_data.fields[5].value);
  EXPECT_EQ(std::u16string(), response_data.fields[6].value);
  EXPECT_EQ(std::u16string(), response_data.fields[7].value);
}

// Verify when extension is misclassified, and there is a complete
// phone field, we do not fill anything to extension field.
TEST_F(BrowserAutofillManagerTest,
       FillFirstPhoneNumber_NotFillMisclassifiedExtention) {
  AutofillProfile* work_profile =
      personal_data().GetProfileByGUID("00000000-0000-0000-0000-000000000002");
  ASSERT_TRUE(work_profile != nullptr);
  work_profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"16505554567");

  std::string guid(work_profile->guid());

  FormData form_with_misclassified_extension;
  form_with_misclassified_extension.unique_renderer_id =
      test::MakeFormRendererId();
  form_with_misclassified_extension.url = GURL("https://www.foo.com/");

  FormFieldData field;
  // Default is zero, have to set to a number autofill can process.
  field.max_length = 10;
  form_with_misclassified_extension.name =
      u"complete_phone_form_with_extension";
  test::CreateTestFormField("Full Name", "full_name", "", "text", "name",
                            &field);
  form_with_misclassified_extension.fields.push_back(field);
  test::CreateTestFormField("address", "address", "", "text", "addresses",
                            &field);
  form_with_misclassified_extension.fields.push_back(field);
  test::CreateTestFormField("area code", "area_code", "", "text",
                            "tel-area-code", &field);
  form_with_misclassified_extension.fields.push_back(field);
  test::CreateTestFormField("number", "phone_number", "", "text", "tel-local",
                            &field);
  form_with_misclassified_extension.fields.push_back(field);
  test::CreateTestFormField("extension", "extension", "", "text", "tel-local",
                            &field);
  form_with_misclassified_extension.fields.push_back(field);

  FormsSeen({form_with_misclassified_extension});
  FormData response_data;
  FillAutofillFormDataAndSaveResults(
      form_with_misclassified_extension,
      *form_with_misclassified_extension.fields.begin(),
      MakeFrontendId({.profile_id = guid}), &response_data);

  // Verify the misclassified extension field is not filled.
  ASSERT_EQ(5U, response_data.fields.size());
  EXPECT_EQ(u"Charles Hardin Holley", response_data.fields[0].value);
  EXPECT_EQ(std::u16string(), response_data.fields[1].value);
  EXPECT_EQ(u"650", response_data.fields[2].value);
  EXPECT_EQ(u"5554567", response_data.fields[3].value);
  EXPECT_EQ(std::u16string(), response_data.fields[4].value);
}

// Verify that phone number fields annotated with the autocomplete attribute
// are filled best-effort.
// Phone number local heuristics only succeed if a PHONE_HOME_NUMBER field is
// present.
TEST_F(BrowserAutofillManagerTest, FillFirstPhoneNumber_BestEfforFilling) {
  AutofillProfile* work_profile =
      personal_data().GetProfileByGUID("00000000-0000-0000-0000-000000000002");
  ASSERT_TRUE(work_profile != nullptr);
  work_profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"16505554567");

  std::string guid(work_profile->guid());

  FormData form_with_no_complete_number;
  form_with_no_complete_number.unique_renderer_id = test::MakeFormRendererId();
  form_with_no_complete_number.url = GURL("https://www.foo.com/");

  FormFieldData field;
  // Default is zero, have to set to a number autofill can process.
  field.max_length = 10;
  form_with_no_complete_number.name = u"no_complete_phone_form";
  test::CreateTestFormField("Full Name", "full_name", "", "text", "name",
                            &field);
  form_with_no_complete_number.fields.push_back(field);
  test::CreateTestFormField("address", "address", "", "text", "address",
                            &field);
  form_with_no_complete_number.fields.push_back(field);
  test::CreateTestFormField("area code", "area_code", "", "text",
                            "tel-area-code", &field);
  form_with_no_complete_number.fields.push_back(field);
  test::CreateTestFormField("extension", "extension", "", "text", "extension",
                            &field);
  form_with_no_complete_number.fields.push_back(field);

  FormsSeen({form_with_no_complete_number});
  FormData response_data;
  FillAutofillFormDataAndSaveResults(
      form_with_no_complete_number,
      *form_with_no_complete_number.fields.begin(),
      MakeFrontendId({.profile_id = guid}), &response_data);

  // Verify when there is no complete phone number fields, we do best effort
  // filling.
  ASSERT_EQ(4U, response_data.fields.size());
  EXPECT_EQ(u"Charles Hardin Holley", response_data.fields[0].value);
  EXPECT_EQ(u"123 Apple St., unit 6", response_data.fields[1].value);
  EXPECT_EQ(u"650", response_data.fields[2].value);
  EXPECT_EQ(std::u16string(), response_data.fields[3].value);
}

// When the focus is on second phone field explicitly, we will fill the
// entire form, both first phone field and second phone field included.
TEST_F(BrowserAutofillManagerTest,
       FillFirstPhoneNumber_FocusOnSecondPhoneNumber) {
  AutofillProfile* work_profile =
      personal_data().GetProfileByGUID("00000000-0000-0000-0000-000000000002");
  ASSERT_TRUE(work_profile != nullptr);
  work_profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"16505554567");

  std::string guid(work_profile->guid());

  FormData form_with_multiple_whole_number_fields;
  form_with_multiple_whole_number_fields.unique_renderer_id =
      test::MakeFormRendererId();
  form_with_multiple_whole_number_fields.url = GURL("https://www.foo.com/");

  FormFieldData field;
  // Default is zero, have to set to a number autofill can process.
  field.max_length = 10;
  form_with_multiple_whole_number_fields.name = u"multiple_whole_number_fields";
  test::CreateTestFormField("Full Name", "full_name", "", "text", &field);
  form_with_multiple_whole_number_fields.fields.push_back(field);
  test::CreateTestFormField("number", "phone_number", "", "text", &field);
  form_with_multiple_whole_number_fields.fields.push_back(field);
  test::CreateTestFormField("extension", "extension", "", "text", &field);
  form_with_multiple_whole_number_fields.fields.push_back(field);
  test::CreateTestFormField("shipping number", "shipping_phone_number", "",
                            "text", &field);
  form_with_multiple_whole_number_fields.fields.push_back(field);

  FormsSeen({form_with_multiple_whole_number_fields});
  FormData response_data;
  auto it = form_with_multiple_whole_number_fields.fields.begin();
  // Move it to point to "shipping number".
  std::advance(it, 3);
  FillAutofillFormDataAndSaveResults(form_with_multiple_whole_number_fields,
                                     *it, MakeFrontendId({.profile_id = guid}),
                                     &response_data);

  // Verify when the second phone number field is being focused, we fill
  // that field *AND* the first phone number field.
  ASSERT_EQ(4U, response_data.fields.size());
  EXPECT_EQ(u"Charles Hardin Holley", response_data.fields[0].value);
  EXPECT_EQ(u"6505554567", response_data.fields[1].value);
  EXPECT_EQ(std::u16string(), response_data.fields[2].value);
  EXPECT_EQ(u"6505554567", response_data.fields[3].value);
}

TEST_F(BrowserAutofillManagerTest,
       FillFirstPhoneNumber_HiddenFieldShouldNotCount) {
  AutofillProfile* work_profile =
      personal_data().GetProfileByGUID("00000000-0000-0000-0000-000000000002");
  ASSERT_TRUE(work_profile != nullptr);
  work_profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"16505554567");

  std::string guid(work_profile->guid());

  FormData form_with_multiple_whole_number_fields;
  form_with_multiple_whole_number_fields.unique_renderer_id =
      test::MakeFormRendererId();
  form_with_multiple_whole_number_fields.url = GURL("https://www.foo.com/");

  FormFieldData field;
  // Default is zero, have to set to a number autofill can process.
  field.max_length = 10;
  form_with_multiple_whole_number_fields.name = u"multiple_whole_number_fields";
  test::CreateTestFormField("Full Name", "full_name", "", "text", &field);
  form_with_multiple_whole_number_fields.fields.push_back(field);
  test::CreateTestFormField("number", "phone_number", "", "text", &field);
  field.is_focusable = false;
  form_with_multiple_whole_number_fields.fields.push_back(field);
  field.is_focusable = true;
  test::CreateTestFormField("extension", "extension", "", "text", &field);
  form_with_multiple_whole_number_fields.fields.push_back(field);
  test::CreateTestFormField("shipping number", "shipping_phone_number", "",
                            "text", &field);
  form_with_multiple_whole_number_fields.fields.push_back(field);

  FormsSeen({form_with_multiple_whole_number_fields});
  FormData response_data;
  FillAutofillFormDataAndSaveResults(
      form_with_multiple_whole_number_fields,
      *form_with_multiple_whole_number_fields.fields.begin(),
      MakeFrontendId({.profile_id = guid}), &response_data);

  // Verify hidden/non-focusable phone field is set to only_fill_when_focused.
  ASSERT_EQ(4U, response_data.fields.size());
  EXPECT_EQ(u"Charles Hardin Holley", response_data.fields[0].value);
  EXPECT_EQ(std::u16string(), response_data.fields[1].value);
  EXPECT_EQ(std::u16string(), response_data.fields[2].value);
  EXPECT_EQ(u"6505554567", response_data.fields[3].value);
}

// The hidden and the presentational fields should be filled, only if their
// control type is 'select-one'. This exception is made to support synthetic
// fields.
TEST_F(BrowserAutofillManagerTest, FormWithHiddenOrPresentationalSelects) {
  FormData form;
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");

  FormFieldData field;

  test::CreateTestFormField("First name", "firstname", "", "text", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("Last name", "lastname", "", "text", &field);
  form.fields.push_back(field);

  {
    const std::vector<const char*> values{"CA", "US", "BR"};
    const std::vector<const char*> contents{"Canada", "United States",
                                            "Banana Republic"};
    test::CreateTestSelectField("Country", "country", "", values, contents,
                                &field);
    field.is_focusable = false;
    form.fields.push_back(field);
  }
  {
    const std::vector<const char*> values{"NY", "CA", "TN"};
    const std::vector<const char*> contents{"New York", "California",
                                            "Tennessee"};
    test::CreateTestSelectField("State", "state", "", values, contents, &field);
    field.role = FormFieldData::RoleAttribute::kPresentation;
    form.fields.push_back(field);
  }

  test::CreateTestFormField("City", "city", "", "text", &field);
  field.is_focusable = false;
  form.fields.push_back(field);

  test::CreateTestFormField("Street Address", "address", "", "text", &field);
  field.role = FormFieldData::RoleAttribute::kPresentation;
  form.fields.push_back(field);

  FormsSeen({form});

  const char guid[] = "00000000-0000-0000-0000-000000000001";
  FormData response_data;
  base::HistogramTester histogram_tester;

  FillAutofillFormDataAndSaveResults(form, form.fields[0],
                                     MakeFrontendId({.profile_id = guid}),
                                     &response_data);
  histogram_tester.ExpectTotalCount(
      "Autofill.HiddenOrPresentationalSelectFieldsFilled", 2);

  ExpectFilledField("First name", "firstname", "Elvis", "text",
                    response_data.fields[0]);
  ExpectFilledField("Last name", "lastname", "Presley", "text",
                    response_data.fields[1]);
  ExpectFilledField("Country", "country", "US", "select-one",
                    response_data.fields[2]);
  ExpectFilledField("State", "state", "TN", "select-one",
                    response_data.fields[3]);
  ExpectFilledField("City", "city", "", "text", response_data.fields[4]);
  ExpectFilledField("Street Address", "address", "", "text",
                    response_data.fields[5]);
}

TEST_F(BrowserAutofillManagerTest,
       FillFirstPhoneNumber_MultipleSectionFilledCorrectly) {
  AutofillProfile* work_profile =
      personal_data().GetProfileByGUID("00000000-0000-0000-0000-000000000002");
  ASSERT_TRUE(work_profile != nullptr);
  work_profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"16505554567");

  std::string guid(work_profile->guid());

  FormData form_with_multiple_sections;
  form_with_multiple_sections.unique_renderer_id = test::MakeFormRendererId();
  form_with_multiple_sections.url = GURL("https://www.foo.com/");

  FormFieldData field;
  // Default is zero, have to set to a number autofill can process.
  field.max_length = 10;
  form_with_multiple_sections.name = u"multiple_section_fields";
  test::CreateTestFormField("Full Name", "full_name", "", "text", &field);
  form_with_multiple_sections.fields.push_back(field);
  test::CreateTestFormField("Address", "address", "", "text", &field);
  form_with_multiple_sections.fields.push_back(field);
  test::CreateTestFormField("number", "phone_number", "", "text", &field);
  form_with_multiple_sections.fields.push_back(field);
  test::CreateTestFormField("other number", "other_phone_number", "", "text",
                            &field);
  form_with_multiple_sections.fields.push_back(field);
  test::CreateTestFormField("extension", "extension", "", "text", &field);
  form_with_multiple_sections.fields.push_back(field);
  test::CreateTestFormField("Full Name", "full_name", "", "text", &field);
  form_with_multiple_sections.fields.push_back(field);
  test::CreateTestFormField("Shipping Address", "shipping_address", "", "text",
                            &field);
  form_with_multiple_sections.fields.push_back(field);
  test::CreateTestFormField("shipping number", "shipping_phone_number", "",
                            "text", &field);
  form_with_multiple_sections.fields.push_back(field);
  test::CreateTestFormField("other shipping number",
                            "other_shipping_phone_number", "", "text", &field);
  form_with_multiple_sections.fields.push_back(field);

  FormsSeen({form_with_multiple_sections});
  FormData response_data;
  // Fill first sections.
  FillAutofillFormDataAndSaveResults(
      form_with_multiple_sections, *form_with_multiple_sections.fields.begin(),
      MakeFrontendId({.profile_id = guid}), &response_data);

  // Verify first section is filled with rationalization.
  ASSERT_EQ(9U, response_data.fields.size());
  EXPECT_EQ(u"Charles Hardin Holley", response_data.fields[0].value);
  EXPECT_EQ(u"123 Apple St.", response_data.fields[1].value);
  EXPECT_EQ(u"6505554567", response_data.fields[2].value);
  EXPECT_EQ(std::u16string(), response_data.fields[3].value);
  EXPECT_EQ(std::u16string(), response_data.fields[4].value);
  EXPECT_EQ(std::u16string(), response_data.fields[5].value);
  EXPECT_EQ(std::u16string(), response_data.fields[6].value);
  EXPECT_EQ(std::u16string(), response_data.fields[7].value);
  EXPECT_EQ(std::u16string(), response_data.fields[8].value);

  // Fill second section.
  auto it = form_with_multiple_sections.fields.begin();
  std::advance(it, 6);  // Pointing to second section.

  FillAutofillFormDataAndSaveResults(form_with_multiple_sections, *it,
                                     MakeFrontendId({.profile_id = guid}),
                                     &response_data);

  // Verify second section is filled with rationalization.
  ASSERT_EQ(9U, response_data.fields.size());
  EXPECT_EQ(std::u16string(), response_data.fields[0].value);
  EXPECT_EQ(std::u16string(), response_data.fields[1].value);
  EXPECT_EQ(std::u16string(), response_data.fields[2].value);
  EXPECT_EQ(std::u16string(), response_data.fields[3].value);
  EXPECT_EQ(std::u16string(), response_data.fields[4].value);
  EXPECT_EQ(u"Charles Hardin Holley", response_data.fields[5].value);
  EXPECT_EQ(u"123 Apple St.", response_data.fields[6].value);
  EXPECT_EQ(u"6505554567", response_data.fields[7].value);
  EXPECT_EQ(std::u16string(), response_data.fields[8].value);
}

// Test that we can still fill a form when a field has been removed from it.
TEST_F(BrowserAutofillManagerTest, FormChangesRemoveField) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);

  // Add a field -- we'll remove it again later.
  FormFieldData field;
  test::CreateTestFormField("Some", "field", "", "text", &field);
  form.fields.insert(form.fields.begin() + 3, field);

  FormsSeen({form});

  // Now, after the call to |FormsSeen|, we remove the field before filling.
  form.fields.erase(form.fields.begin() + 3);

  FormsSeen({form});

  const char guid[] = "00000000-0000-0000-0000-000000000001";
  FormData response_data;
  FillAutofillFormDataAndSaveResults(form, form.fields[0],
                                     MakeFrontendId({.profile_id = guid}),
                                     &response_data);
  ExpectFilledAddressFormElvis(response_data, false);
}

// Test that we can still fill a form when a field has been added to it.
TEST_F(BrowserAutofillManagerTest, FormChangesAddField) {
  // The offset of the phone field in the address form.
  const int kPhoneFieldOffset = 9;

  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);

  // Remove the phone field -- we'll add it back later.
  auto pos = form.fields.begin() + kPhoneFieldOffset;
  FormFieldData field = *pos;
  pos = form.fields.erase(pos);

  FormsSeen({form});

  // Now, after the call to |FormsSeen|, we restore the field before filling.
  form.fields.insert(pos, field);

  FormsSeen({form});

  const char guid[] = "00000000-0000-0000-0000-000000000001";
  FormData response_data;
  FillAutofillFormDataAndSaveResults(form, form.fields[0],
                                     MakeFrontendId({.profile_id = guid}),
                                     &response_data);
  ExpectFilledAddressFormElvis(response_data, false);
}

// Test that we can still fill a form when the visibility of some fields
// changes.
TEST_F(BrowserAutofillManagerTest, FormChangesVisibilityOfFields) {
  // Set up our form data.
  FormData form;
  form.unique_renderer_id = test::MakeFormRendererId();
  form.url = GURL("https://www.foo.com/");

  FormFieldData field;

  // Default is zero, have to set to a number autofill can process.
  field.max_length = 10;
  form.name = u"multiple_groups_fields";
  test::CreateTestFormField("First Name", "first_name", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last Name", "last_name", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address", "address", "", "text", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("Postal Code", "postal_code", "", "text", &field);
  field.is_focusable = false;
  form.fields.push_back(field);

  test::CreateTestFormField("Country", "country", "", "text", &field);
  field.is_focusable = false;
  form.fields.push_back(field);

  FormsSeen({form});

  // Fill the form with the first profile. The hidden fields will not get
  // filled.
  const char guid[] = "00000000-0000-0000-0000-000000000001";
  FormData response_data;
  FillAutofillFormDataAndSaveResults(form, form.fields[0],
                                     MakeFrontendId({.profile_id = guid}),
                                     &response_data);

  ASSERT_EQ(5U, response_data.fields.size());
  ExpectFilledField("First Name", "first_name", "Elvis", "text",
                    response_data.fields[0]);
  ExpectFilledField("Last Name", "last_name", "Presley", "text",
                    response_data.fields[1]);
  ExpectFilledField("Address", "address", "3734 Elvis Presley Blvd.", "text",
                    response_data.fields[2]);
  ExpectFilledField("Postal Code", "postal_code", "", "text",
                    response_data.fields[3]);
  ExpectFilledField("Country", "country", "", "text", response_data.fields[4]);

  // Two other fields will show up. Select the second profile. The fields that
  // were already filled, would be left unchanged, and the rest would be filled
  // with the second profile. (Two different profiles are selected, to make sure
  // the right fields are getting filled.)
  response_data.fields[3].is_focusable = true;
  response_data.fields[4].is_focusable = true;

  FormData later_response_data;
  const char guid2[] = "00000000-0000-0000-0000-000000000002";
  FillAutofillFormDataAndSaveResults(response_data, response_data.fields[4],
                                     MakeFrontendId({.profile_id = guid2}),
                                     &later_response_data);
  ASSERT_EQ(5U, later_response_data.fields.size());
  ExpectFilledField("First Name", "first_name", "Elvis", "text",
                    later_response_data.fields[0]);
  ExpectFilledField("Last Name", "last_name", "Presley", "text",
                    later_response_data.fields[1]);
  ExpectFilledField("Address", "address", "3734 Elvis Presley Blvd.", "text",
                    later_response_data.fields[2]);
  ExpectFilledField("Postal Code", "postal_code", "79401", "text",
                    later_response_data.fields[3]);
  ExpectFilledField("Country", "country", "United States", "text",
                    later_response_data.fields[4]);
}

// Test that we are able to save form data when forms are submitted.
TEST_F(BrowserAutofillManagerTest, FormSubmitted) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  FormsSeen({form});

  // Fill the form.
  const char guid[] = "00000000-0000-0000-0000-000000000001";
  FormData response_data;
  FillAutofillFormDataAndSaveResults(form, form.fields[0],
                                     MakeFrontendId({.profile_id = guid}),
                                     &response_data);
  ExpectFilledAddressFormElvis(response_data, false);

  // Simulate form submission. We should call into the PDM to try to save the
  // filled data.
  FormSubmitted(response_data);
  EXPECT_EQ(1, personal_data().num_times_save_imported_profile_called());
}

// Test that we are saving form data when the FormSubmitted event is sent.
TEST_F(BrowserAutofillManagerTest, FormSubmittedSaveData) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  FormsSeen({form});

  // Fill the form.
  const char guid[] = "00000000-0000-0000-0000-000000000001";
  FormData response_data;
  FillAutofillFormDataAndSaveResults(form, form.fields[0],
                                     MakeFrontendId({.profile_id = guid}),
                                     &response_data);
  ExpectFilledAddressFormElvis(response_data, false);

  browser_autofill_manager_->OnFormSubmitted(response_data, false,
                                             SubmissionSource::FORM_SUBMISSION);
  EXPECT_EQ(1, personal_data().num_times_save_imported_profile_called());
}

// Test the field log events at the form submission.
class BrowserAutofillManagerWithLogEventsTest
    : public BrowserAutofillManagerTest {
 protected:
  BrowserAutofillManagerWithLogEventsTest() {
    scoped_features_.InitWithFeatures(
        /*enabled_features=*/{features::kAutofillLogUKMEventsWithSampleRate,
                              features::kAutofillParsingPatternProvider,
                              features::kAutofillFeedback},
        /*disabled_features=*/{});
  }

  std::vector<AutofillField::FieldLogEventType> ToFieldTypeEvents(
      ServerFieldType heuristic_type,
      ServerFieldType overall_type,
      size_t field_signature_rank = 1) {
    std::vector<AutofillField::FieldLogEventType> expected_events;
#if BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
    // Default pattern.
    expected_events.push_back(HeuristicPredictionFieldLogEvent{
        .field_type = heuristic_type,
        .pattern_source = PatternSource::kDefault,
        .is_active_pattern_source = true,
        .rank_in_field_signature_group = field_signature_rank,
    });
    // Legacy pattern.
    expected_events.push_back(HeuristicPredictionFieldLogEvent{
        .field_type = heuristic_type,
        .pattern_source = PatternSource::kLegacy,
        .is_active_pattern_source = false,
        .rank_in_field_signature_group = field_signature_rank,
    });
    // Experimental pattern.
    expected_events.push_back(HeuristicPredictionFieldLogEvent{
        .field_type = heuristic_type,
        .pattern_source = PatternSource::kExperimental,
        .is_active_pattern_source = false,
        .rank_in_field_signature_group = field_signature_rank,
    });
    // Nextgen pattern.
    expected_events.push_back(HeuristicPredictionFieldLogEvent{
        .field_type = heuristic_type,
        .pattern_source = PatternSource::kNextGen,
        .is_active_pattern_source = false,
        .rank_in_field_signature_group = field_signature_rank,
    });
#else
    // Legacy pattern.
    expected_events.push_back(HeuristicPredictionFieldLogEvent{
        .field_type = heuristic_type,
        .pattern_source = PatternSource::kLegacy,
        .is_active_pattern_source = true,
        .rank_in_field_signature_group = field_signature_rank,
    });
#endif
    // Rationalization.
    expected_events.push_back(RationalizationFieldLogEvent{
        .field_type = overall_type,
        .section_id = 1,
        .type_changed = false,
    });
    return expected_events;
  }

  // n = 1 means the first instance.
  template <class T>
  const T* FindNthEventOfType(
      const std::vector<AutofillField::FieldLogEventType>& events,
      size_t n) {
    // |count| represents the number of events of type T having been seen so
    // far.
    size_t count = 0;
    for (const auto& event : events) {
      if (const T* log_event = absl::get_if<T>(&event)) {
        ++count;
        if (count == n) {
          return log_event;
        }
      }
    }
    return nullptr;
  }

  template <class T>
  const T* FindFirstEventOfType(
      const std::vector<AutofillField::FieldLogEventType>& events) {
    return FindNthEventOfType<T>(events, 1);
  }

 private:
  base::test::ScopedFeatureList scoped_features_;
};

// Test that we record TriggerFillFieldLogEvent for the field we click to show
// the autofill suggestion and FillFieldLogEvent for every field in the form.
TEST_F(BrowserAutofillManagerWithLogEventsTest, LogEventsAtFormSubmitted) {
  TestAutofillClock clock(AutofillClock::Now());
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  FormsSeen({form});

  // Fill the form.
  const char guid[] = "00000000-0000-0000-0000-000000000001";
  FormData response_data;
  FillAutofillFormDataAndSaveResults(form, form.fields[0],
                                     MakeFrontendId({.profile_id = guid}),
                                     &response_data);
  ExpectFilledAddressFormElvis(response_data, false);

  // Simulate form submission.
  FormSubmitted(response_data);
  EXPECT_EQ(1, personal_data().num_times_save_imported_profile_called());

  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  ASSERT_TRUE(browser_autofill_manager_->GetCachedFormAndField(
      form, form.fields.front(), &form_structure, &autofill_field));
  ASSERT_TRUE(form_structure);

  const std::vector<AutofillField::FieldLogEventType>& focus_field_log_events =
      autofill_field->field_log_events();
  ASSERT_EQ(u"First Name", autofill_field->parseable_label());
  const TriggerFillFieldLogEvent* trigger_fill_field_log_event =
      FindFirstEventOfType<TriggerFillFieldLogEvent>(focus_field_log_events);
  ASSERT_TRUE(trigger_fill_field_log_event);

  for (const auto& autofill_field_ptr : *form_structure) {
    SCOPED_TRACE(autofill_field_ptr->parseable_label());
    std::vector<AutofillField::FieldLogEventType> expected_events =
        ToFieldTypeEvents(autofill_field_ptr->heuristic_type(),
                          autofill_field_ptr->heuristic_type());

    if (autofill_field_ptr->parseable_label() == u"First Name") {
      // The "First Name" field is the trigger field, so it contains the
      // TriggerFillFieldLogEvent followed by a FillFieldLogEvent.
      expected_events.push_back(TriggerFillFieldLogEvent{
          .fill_event_id = trigger_fill_field_log_event->fill_event_id,
          .data_type = FillDataType::kAutofillProfile,
          .associated_country_code = "US",
          .timestamp = AutofillClock::Now()});
    }
    // All filled fields share the same expected FillFieldLogEvent.
    // The first TriggerFillFieldLogEvent determines the fill_event_id for
    // all following FillFieldLogEvents.
    expected_events.push_back(FillFieldLogEvent{
        .fill_event_id = trigger_fill_field_log_event->fill_event_id,
        .had_value_before_filling = OptionalBoolean::kFalse,
        .autofill_skipped_status = SkipStatus::kNotSkipped,
        .was_autofilled = OptionalBoolean::kTrue,
        .had_value_after_filling = OptionalBoolean::kTrue,
    });
    EXPECT_THAT(autofill_field_ptr->field_log_events(),
                ArrayEquals(expected_events));
  }
}

// Test that we record FillFieldLogEvents correctly after autofill when the
// field has nothing to fill or the field contains a user typed value already.
TEST_F(BrowserAutofillManagerWithLogEventsTest,
       LogEventsFillPartlyManuallyFilledForm) {
  TestAutofillClock clock(AutofillClock::Now());
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  FormsSeen({form});

  // Michael will be overridden with Elvis because Autofill is triggered from
  // the first field.
  form.fields[0].value = u"Michael";
  form.fields[0].properties_mask |= kUserTyped;

  // Jackson will be preserved, only override the first field.
  form.fields[2].value = u"Jackson";
  form.fields[2].properties_mask |= kUserTyped;

  // Fill the address data.
  AutofillProfile profile1;
  const char guid[] = "00000000-0000-0000-0000-000000000100";
  profile1.set_guid(guid);
  test::SetProfileInfo(&profile1, "Buddy", "Aaron", "Holly", "", "RCA",
                       "3734 Elvis Presley Blvd.", "Apt. 10", "Memphis",
                       "Tennessee", "38116", "US", "");
  personal_data().AddProfile(profile1);
  FormData response_data;
  FillAutofillFormDataAndSaveResults(form, *form.fields.begin(),
                                     MakeFrontendId({.profile_id = guid}),
                                     &response_data);

  ExpectFilledForm(response_data, "Buddy", "Aaron", "Jackson",
                   "3734 Elvis Presley Blvd.", "Apt. 10", "Memphis",
                   "Tennessee", "38116", "United States", "", "", "", "", "",
                   "", true, false, false);

  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  ASSERT_TRUE(browser_autofill_manager_->GetCachedFormAndField(
      form, form.fields.front(), &form_structure, &autofill_field));
  ASSERT_TRUE(form_structure);

  const std::vector<AutofillField::FieldLogEventType>& focus_field_log_events =
      autofill_field->field_log_events();
  ASSERT_EQ(u"First Name", autofill_field->parseable_label());
  const TriggerFillFieldLogEvent* trigger_fill_field_log_event =
      FindFirstEventOfType<TriggerFillFieldLogEvent>(focus_field_log_events);
  ASSERT_TRUE(trigger_fill_field_log_event);

  // The first TriggerFillFieldLogEvent determines the fill_event_id for
  // all following FillFieldLogEvents.
  FillEventId fill_event_id = trigger_fill_field_log_event->fill_event_id;
  for (const auto& autofill_field_ptr : *form_structure) {
    SCOPED_TRACE(autofill_field_ptr->parseable_label());
    std::vector<AutofillField::FieldLogEventType> expected_events =
        ToFieldTypeEvents(autofill_field_ptr->heuristic_type(),
                          autofill_field_ptr->heuristic_type());

    if (autofill_field_ptr->parseable_label() == u"First Name") {
      // The "First Name" field is the trigger field, so it contains the
      // TriggerFillFieldLogEvent followed by a FillFieldLogEvent.
      expected_events.push_back(
          TriggerFillFieldLogEvent{.fill_event_id = fill_event_id,
                                   .data_type = FillDataType::kAutofillProfile,
                                   .associated_country_code = "US",
                                   .timestamp = AutofillClock::Now()});
      expected_events.push_back(FillFieldLogEvent{
          .fill_event_id = fill_event_id,
          .had_value_before_filling = OptionalBoolean::kTrue,
          .autofill_skipped_status = SkipStatus::kNotSkipped,
          .was_autofilled = OptionalBoolean::kTrue,
          .had_value_after_filling = OptionalBoolean::kTrue,
      });
    } else if (autofill_field_ptr->parseable_label() == u"Phone Number" ||
               autofill_field_ptr->parseable_label() == u"Email") {
      // Not filled because the address profile contained no data to fill.
      expected_events.push_back(FillFieldLogEvent{
          .fill_event_id = fill_event_id,
          .had_value_before_filling = OptionalBoolean::kFalse,
          .autofill_skipped_status = SkipStatus::kNotSkipped,
          .was_autofilled = OptionalBoolean::kFalse,
          .had_value_after_filling = OptionalBoolean::kFalse,
      });
    } else if (autofill_field_ptr->parseable_label() == u"Last Name") {
      // Not filled because the field contained a user typed value already.
      expected_events.push_back(FillFieldLogEvent{
          .fill_event_id = fill_event_id,
          .had_value_before_filling = OptionalBoolean::kTrue,
          .autofill_skipped_status = SkipStatus::kUserFilledFields,
          .was_autofilled = OptionalBoolean::kFalse,
          .had_value_after_filling = OptionalBoolean::kTrue,
      });
    } else {
      expected_events.push_back(FillFieldLogEvent{
          .fill_event_id = fill_event_id,
          .had_value_before_filling = OptionalBoolean::kFalse,
          .autofill_skipped_status = SkipStatus::kNotSkipped,
          .was_autofilled = OptionalBoolean::kTrue,
          .had_value_after_filling = OptionalBoolean::kTrue,
      });
    }
    EXPECT_THAT(autofill_field_ptr->field_log_events(),
                ArrayEquals(expected_events));
  }
}

// Test that we record FillFieldLogEvents after filling a form twice, the first
// time some field values are missing when autofilling.
TEST_F(BrowserAutofillManagerWithLogEventsTest, LogEventsAtRefillForm) {
  TestAutofillClock clock(AutofillClock::Now());
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // First fill the address data which does not have email and phone number.
  AutofillProfile profile1;
  const char guid[] = "00000000-0000-0000-0000-000000000100";
  profile1.set_guid(guid);
  test::SetProfileInfo(&profile1, "Buddy", "Aaron", "Holly", "", "RCA",
                       "3734 Elvis Presley Blvd.", "Apt. 10", "Memphis",
                       "Tennessee", "38116", "US", "");
  personal_data().AddProfile(profile1);
  FormData response_data;
  FillAutofillFormDataAndSaveResults(form, *form.fields.begin(),
                                     MakeFrontendId({.profile_id = guid}),
                                     &response_data);

  ExpectFilledForm(response_data, "Buddy", "Aaron", "Holly",
                   "3734 Elvis Presley Blvd.", "Apt. 10", "Memphis",
                   "Tennessee", "38116", "United States", "", "", "", "", "",
                   "", true, false, false);

  // Refill the address data with all the field values.
  const char guid2[] = "00000000-0000-0000-0000-000000000001";
  FillAutofillFormDataAndSaveResults(
      response_data, *response_data.fields.begin(),
      MakeFrontendId({.profile_id = guid2}), &response_data);

  ExpectFilledForm(response_data, "Elvis", "Aaron", "Holly",
                   "3734 Elvis Presley Blvd.", "Apt. 10", "Memphis",
                   "Tennessee", "38116", "United States", "12345678901",
                   "theking@gmail.com", "", "", "", "", true, false, false);

  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  ASSERT_TRUE(browser_autofill_manager_->GetCachedFormAndField(
      form, form.fields.front(), &form_structure, &autofill_field));
  ASSERT_TRUE(form_structure);

  const std::vector<AutofillField::FieldLogEventType>& focus_field_log_events =
      autofill_field->field_log_events();
  ASSERT_EQ(u"First Name", autofill_field->parseable_label());
  const TriggerFillFieldLogEvent* trigger_fill_field_log_event1 =
      FindNthEventOfType<TriggerFillFieldLogEvent>(focus_field_log_events, 1);
  ASSERT_TRUE(trigger_fill_field_log_event1);
  const TriggerFillFieldLogEvent* trigger_fill_field_log_event2 =
      FindNthEventOfType<TriggerFillFieldLogEvent>(focus_field_log_events, 2);
  ASSERT_TRUE(trigger_fill_field_log_event2);

  // All filled fields share the same expected FillFieldLogEvent.
  // The first TriggerFillFieldLogEvent determines the fill_event_id for
  // all following FillFieldLogEvents.
  FillFieldLogEvent expected_fill_field_log_event1{
      .fill_event_id = trigger_fill_field_log_event1->fill_event_id,
      .had_value_before_filling = OptionalBoolean::kFalse,
      .autofill_skipped_status = SkipStatus::kNotSkipped,
      .was_autofilled = OptionalBoolean::kTrue,
      .had_value_after_filling = OptionalBoolean::kTrue,
  };
  FillFieldLogEvent expected_fill_field_log_event2{
      .fill_event_id = trigger_fill_field_log_event2->fill_event_id,
      .had_value_before_filling = OptionalBoolean::kTrue,
      .autofill_skipped_status = SkipStatus::kNotSkipped,
      .was_autofilled = OptionalBoolean::kTrue,
      .had_value_after_filling = OptionalBoolean::kTrue,
  };

  for (const auto& autofill_field_ptr : *form_structure) {
    SCOPED_TRACE(autofill_field_ptr->parseable_label());
    std::vector<AutofillField::FieldLogEventType> expected_events =
        ToFieldTypeEvents(autofill_field_ptr->heuristic_type(),
                          autofill_field_ptr->heuristic_type());

    if (autofill_field_ptr->parseable_label() == u"First Name") {
      // The "First Name" field is the trigger field, so it contains the
      // TriggerFillFieldLogEvent followed by a FillFieldLogEvent.
      expected_events.push_back(TriggerFillFieldLogEvent{
          .fill_event_id = trigger_fill_field_log_event1->fill_event_id,
          .data_type = FillDataType::kAutofillProfile,
          .associated_country_code = "US",
          .timestamp = AutofillClock::Now()});
      expected_events.push_back(expected_fill_field_log_event1);
      expected_events.push_back(TriggerFillFieldLogEvent{
          .fill_event_id = trigger_fill_field_log_event2->fill_event_id,
          .data_type = FillDataType::kAutofillProfile,
          .associated_country_code = "US",
          .timestamp = AutofillClock::Now()});
      expected_events.push_back(expected_fill_field_log_event2);
    } else if (autofill_field_ptr->parseable_label() == u"Phone Number" ||
               autofill_field_ptr->parseable_label() == u"Email") {
      FillFieldLogEvent expected_event = expected_fill_field_log_event1;
      expected_event.was_autofilled = OptionalBoolean::kFalse;
      expected_event.had_value_after_filling = OptionalBoolean::kFalse;
      expected_events.push_back(expected_event);

      FillFieldLogEvent expected_event2 = expected_fill_field_log_event2;
      expected_event2.had_value_before_filling = OptionalBoolean::kFalse;
      expected_events.push_back(expected_event2);
    } else {
      expected_events.push_back(expected_fill_field_log_event1);

      FillFieldLogEvent expected_event2 = expected_fill_field_log_event2;
      expected_event2.autofill_skipped_status =
          SkipStatus::kAutofilledFieldsNotRefill;
      expected_event2.was_autofilled = OptionalBoolean::kFalse;
      expected_events.push_back(expected_event2);
    }
    EXPECT_THAT(autofill_field_ptr->field_log_events(),
                ArrayEquals(expected_events));
  }
}

// Test that we record user typing log event correctly after autofill.
TEST_F(BrowserAutofillManagerWithLogEventsTest, LogEventsAtUserTypingInField) {
  TestAutofillClock clock(AutofillClock::Now());
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Fill the form.
  const char guid[] = "00000000-0000-0000-0000-000000000001";
  FormData response_data;
  FillAutofillFormDataAndSaveResults(form, form.fields[0],
                                     MakeFrontendId({.profile_id = guid}),
                                     &response_data);
  ExpectFilledAddressFormElvis(response_data, false);

  ExpectFilledForm(response_data, "Elvis", "Aaron", "Presley",
                   "3734 Elvis Presley Blvd.", "Apt. 10", "Memphis",
                   "Tennessee", "38116", "United States", "12345678901",
                   "theking@gmail.com", "", "", "", "", true, false, false);

  FormFieldData field = form.fields[0];
  // Simulate editing the first field.
  field.value = u"Michael";
  browser_autofill_manager_->OnTextFieldDidChange(
      form, field, gfx::RectF(), AutofillTickClock::NowTicks());

  // Simulate form submission.
  FormSubmitted(response_data);
  EXPECT_EQ(1, personal_data().num_times_save_imported_profile_called());

  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  ASSERT_TRUE(browser_autofill_manager_->GetCachedFormAndField(
      form, form.fields.front(), &form_structure, &autofill_field));
  ASSERT_TRUE(form_structure);

  const std::vector<AutofillField::FieldLogEventType>& focus_field_log_events =
      autofill_field->field_log_events();
  ASSERT_EQ(u"First Name", autofill_field->parseable_label());
  const TriggerFillFieldLogEvent* trigger_fill_field_log_event =
      FindFirstEventOfType<TriggerFillFieldLogEvent>(focus_field_log_events);
  ASSERT_TRUE(trigger_fill_field_log_event);

  // All filled fields share the same expected FillFieldLogEvent.
  // The first TriggerFillFieldLogEvent determines the fill_event_id for
  // all following FillFieldLogEvents.
  FillFieldLogEvent expected_fill_field_log_event{
      .fill_event_id = trigger_fill_field_log_event->fill_event_id,
      .had_value_before_filling = OptionalBoolean::kFalse,
      .autofill_skipped_status = SkipStatus::kNotSkipped,
      .was_autofilled = OptionalBoolean::kTrue,
      .had_value_after_filling = OptionalBoolean::kTrue,
  };

  for (const auto& autofill_field_ptr : *form_structure) {
    SCOPED_TRACE(autofill_field_ptr->parseable_label());
    std::vector<AutofillField::FieldLogEventType> expected_events =
        ToFieldTypeEvents(autofill_field_ptr->heuristic_type(),
                          autofill_field_ptr->heuristic_type());

    if (autofill_field_ptr->parseable_label() == u"First Name") {
      // The "First Name" field is the trigger field, so it contains the
      // TriggerFillFieldLogEvent followed by a FillFieldLogEvent.
      expected_events.push_back(TriggerFillFieldLogEvent{
          .fill_event_id = trigger_fill_field_log_event->fill_event_id,
          .data_type = FillDataType::kAutofillProfile,
          .associated_country_code = "US",
          .timestamp = AutofillClock::Now()});
      expected_events.push_back(expected_fill_field_log_event);
      expected_events.push_back(TypingFieldLogEvent{
          .has_value_after_typing = OptionalBoolean::kTrue,
      });
    } else {
      expected_events.push_back(expected_fill_field_log_event);
    }
    EXPECT_THAT(autofill_field_ptr->field_log_events(),
                ArrayEquals(expected_events));
  }
}

// Test that we record field log events correctly when the user touches to fill
// and fills the credit card form with a suggestion.
TEST_F(BrowserAutofillManagerWithLogEventsTest,
       LogEventsAutofillSuggestionsOrTouchToFill) {
  TestAutofillClock clock(AutofillClock::Now());
  FormData form;
  CreateTestCreditCardFormData(&form, /*is_https=*/true,
                               /*use_month_type=*/false);
  FormsSeen({form});
  const FormFieldData& field = form.fields[0];

  // Touch the field of "Name on Card" and autofill suggestion is shown.
  EXPECT_CALL(touch_to_fill_delegate(), TryToShowTouchToFill(_, _))
      .WillOnce(Return(false));
  TryToShowTouchToFill(form, field, FormElementWasClicked(true));
  EXPECT_TRUE(external_delegate_->on_suggestions_returned_seen());

  // Fill the form by triggering the suggestion from "Name on Card" field.
  const char guid[] = "00000000-0000-0000-0000-000000000004";
  FormData response_data;
  FillAutofillFormDataAndSaveResults(form, *form.fields.begin(),
                                     MakeFrontendId({.credit_card_id = guid}),
                                     &response_data);
  ExpectFilledCreditCardFormElvis(response_data, false);

  // Simulate form submission.
  FormSubmitted(response_data);

  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  ASSERT_TRUE(browser_autofill_manager_->GetCachedFormAndField(
      form, form.fields.front(), &form_structure, &autofill_field));
  ASSERT_TRUE(form_structure);

  const std::vector<AutofillField::FieldLogEventType>& focus_field_log_events =
      autofill_field->field_log_events();
  ASSERT_EQ(u"Name on Card", autofill_field->parseable_label());
  const TriggerFillFieldLogEvent* trigger_fill_field_log_event =
      FindFirstEventOfType<TriggerFillFieldLogEvent>(focus_field_log_events);
  ASSERT_TRUE(trigger_fill_field_log_event);

  // All filled fields share the same expected FillFieldLogEvent.
  // The first TriggerFillFieldLogEvent determines the fill_event_id for
  // all following FillFieldLogEvents.
  FillFieldLogEvent expected_fill_field_log_event{
      .fill_event_id = trigger_fill_field_log_event->fill_event_id,
      .had_value_before_filling = OptionalBoolean::kFalse,
      .autofill_skipped_status = SkipStatus::kNotSkipped,
      .was_autofilled = OptionalBoolean::kTrue,
      .had_value_after_filling = OptionalBoolean::kTrue,
  };

  for (const auto& autofill_field_ptr : *form_structure) {
    SCOPED_TRACE(autofill_field_ptr->parseable_label());
    std::vector<AutofillField::FieldLogEventType> expected_events =
        ToFieldTypeEvents(autofill_field_ptr->heuristic_type(),
                          autofill_field_ptr->heuristic_type());

    if (autofill_field_ptr->parseable_label() == u"Name on Card") {
      // The "Name on Card" field gets focus and shows a suggestion so it
      // contains the AskForValuesToFillFieldLogEvent.
      expected_events.push_back(AskForValuesToFillFieldLogEvent{
          .has_suggestion = OptionalBoolean::kTrue,
          .suggestion_is_shown = OptionalBoolean::kTrue,
      });
      expected_events.push_back(TriggerFillFieldLogEvent{
          .fill_event_id = trigger_fill_field_log_event->fill_event_id,
          .data_type = FillDataType::kCreditCard,
          .associated_country_code = "",
          .timestamp = AutofillClock::Now()});
      // The "Name on Card" field is the trigger field, so it contains the
      // TriggerFillFieldLogEvent followed by a FillFieldLogEvent.
      expected_events.push_back(expected_fill_field_log_event);
    } else if (autofill_field_ptr->parseable_label() == u"CVC") {
      // CVC field is not autofilled.
      expected_events.push_back(FillFieldLogEvent{
          .fill_event_id = trigger_fill_field_log_event->fill_event_id,
          .had_value_before_filling = OptionalBoolean::kFalse,
          .autofill_skipped_status = SkipStatus::kNotSkipped,
          .was_autofilled = OptionalBoolean::kFalse,
          .had_value_after_filling = OptionalBoolean::kFalse,
      });
    } else {
      expected_events.push_back(expected_fill_field_log_event);
    }
    EXPECT_THAT(autofill_field_ptr->field_log_events(),
                ArrayEquals(expected_events));
  }
}

// Test that we record AutocompleteAttributeFieldLogEvent for the fields with
// autocomplete attributes in the form.
TEST_F(BrowserAutofillManagerWithLogEventsTest,
       LogEventsOnAutocompleteAttributeField) {
  // Set up our form data.
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  FormFieldData field;
  test::CreateTestFormField("First Name", "firstname", "", "text", "given-name",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last Name", "lastname", "", "text", "family-name",
                            &field);
  form.fields.push_back(field);
  // Set no autocomplete attribute for the middle name.
  test::CreateTestFormField("Middle name", "middle", "", "text", "", &field);
  form.fields.push_back(field);
  // Set an unrecognized autocomplete attribute for the last name.
  test::CreateTestFormField("Email", "email", "", "text", "unrecognized",
                            &field);
  form.fields.push_back(field);

  // Simulate having seen this form on page load.
  auto form_structure_instance = std::make_unique<FormStructure>(form);
  FormStructure* form_structure = form_structure_instance.get();
  form_structure->DetermineHeuristicTypes(nullptr, nullptr);
  browser_autofill_manager_->AddSeenFormStructure(
      std::move(form_structure_instance));

  // Simulate form submission.
  FormSubmitted(form);

  for (const auto& autofill_field_ptr : *form_structure) {
    SCOPED_TRACE(autofill_field_ptr->parseable_label());
    // The overall_type of the field "Email" is UNKNOWN_TYPE, because its
    // html_type is unrecognized.
    ServerFieldType overall_type =
        autofill_field_ptr->parseable_label() == u"Email"
            ? UNKNOWN_TYPE
            : autofill_field_ptr->heuristic_type();
    std::vector<AutofillField::FieldLogEventType> expected_events =
        ToFieldTypeEvents(autofill_field_ptr->heuristic_type(), overall_type);
    if (autofill_field_ptr->parseable_label() != u"Middle name") {
      expected_events.insert(expected_events.begin(),
                             AutocompleteAttributeFieldLogEvent{
                                 .html_type = autofill_field_ptr->html_type(),
                                 .html_mode = HtmlFieldMode::kNone,
                                 .rank_in_field_signature_group = 1,
                             });
    }
    EXPECT_THAT(autofill_field_ptr->field_log_events(),
                ArrayEquals(expected_events));
  }
}

// Test that we record field log events correctly for autofill crowdsourced
// server prediction.
TEST_F(BrowserAutofillManagerWithLogEventsTest,
       LogEventsParseQueryResponseServerPrediction) {
  // Set up our form data.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  FormFieldData field;
  test::CreateTestFormField(/*label=*/"Name", /*name=*/"name",
                            /*value=*/"", /*type=*/"text", /*field=*/&field);
  form.fields.push_back(field);
  test::CreateTestFormField(/*label=*/"Street", /*name=*/"Street",
                            /*value=*/"", /*type=*/"text", /*field=*/&field);
  form.fields.push_back(field);
  test::CreateTestFormField(/*label=*/"City", /*name=*/"city",
                            /*value=*/"", /*type=*/"text", /*field=*/&field);
  form.fields.push_back(field);
  test::CreateTestFormField(/*label=*/"State", /*name=*/"state",
                            /*value=*/"", /*type=*/"text", /*field=*/&field);
  form.fields.push_back(field);
  test::CreateTestFormField(/*label=*/"Postal Code", /*name=*/"zipcode",
                            /*value=*/"", /*type=*/"text", /*field=*/&field);
  form.fields.push_back(field);

  // Simulate having seen this form on page load.
  // |form_structure_instance| will be owned by |browser_autofill_manager_|.
  auto form_structure_instance = std::make_unique<FormStructure>(form);
  // This pointer is valid as long as autofill manager lives.
  FormStructure* form_structure = form_structure_instance.get();
  form_structure->DetermineHeuristicTypes(nullptr, nullptr);
  browser_autofill_manager_->AddSeenFormStructure(
      std::move(form_structure_instance));

  // Make API response with suggestions.
  AutofillQueryResponse response;
  AutofillQueryResponse::FormSuggestion* form_suggestion;
  // Set suggestions for form.
  form_suggestion = response.add_form_suggestions();
  autofill::test::AddFieldPredictionsToForm(
      form.fields[0],
      {test::CreateFieldPrediction(NAME_FIRST,
                                   FieldPrediction::SOURCE_AUTOFILL_DEFAULT),
       test::CreateFieldPrediction(USERNAME,
                                   FieldPrediction::SOURCE_PASSWORDS_DEFAULT)},
      form_suggestion);
  autofill::test::AddFieldPredictionsToForm(
      form.fields[1],
      {test::CreateFieldPrediction(ADDRESS_HOME_LINE1,
                                   FieldPrediction::SOURCE_OVERRIDE)},
      form_suggestion);
  autofill::test::AddFieldPredictionToForm(form.fields[2], ADDRESS_HOME_CITY,
                                           form_suggestion);
  autofill::test::AddFieldPredictionToForm(form.fields[3], ADDRESS_HOME_STATE,
                                           form_suggestion);
  autofill::test::AddFieldPredictionToForm(form.fields[4], ADDRESS_HOME_ZIP,
                                           form_suggestion);

  std::string response_string;
  ASSERT_TRUE(response.SerializeToString(&response_string));
  std::string encoded_response_string;
  base::Base64Encode(response_string, &encoded_response_string);

  // Query autofill server for the field type prediction.
  browser_autofill_manager_->OnLoadedServerPredictionsForTest(
      encoded_response_string, test::GetEncodedSignatures(*form_structure));
  std::vector<ServerFieldType> types{NAME_FIRST, ADDRESS_HOME_LINE1,
                                     ADDRESS_HOME_CITY, ADDRESS_HOME_STATE,
                                     ADDRESS_HOME_ZIP};
  for (size_t i = 0; i < types.size(); ++i) {
    EXPECT_EQ(types[i], form_structure->field(i)->Type().GetStorableType());
  }

  // Simulate form submission.
  FormSubmitted(form);

  for (const auto& autofill_field_ptr : *form_structure) {
    SCOPED_TRACE(autofill_field_ptr->parseable_label());
    std::vector<AutofillField::FieldLogEventType> expected_events =
        ToFieldTypeEvents(autofill_field_ptr->heuristic_type(),
                          autofill_field_ptr->heuristic_type());
    // The autofill server applies two predictions on the "Name" field.
    ServerFieldType server_type2 =
        autofill_field_ptr->parseable_label() == u"Name" ? USERNAME
                                                         : NO_SERVER_DATA;
    FieldPrediction::Source prediction_source2 =
        autofill_field_ptr->parseable_label() == u"Name"
            ? FieldPrediction::SOURCE_PASSWORDS_DEFAULT
            : FieldPrediction::SOURCE_UNSPECIFIED;
    // The server prediction overrides the type predicted by local heuristic on
    // the field of label "Street".
    bool server_type_prediction_is_override =
        autofill_field_ptr->parseable_label() == u"Street" ? true : false;
    expected_events.push_back(ServerPredictionFieldLogEvent{
        .server_type1 = autofill_field_ptr->server_type(),
        .prediction_source1 =
            autofill_field_ptr->server_predictions()[0].source(),
        .server_type2 = server_type2,
        .prediction_source2 = prediction_source2,
        .server_type_prediction_is_override =
            server_type_prediction_is_override,
        .rank_in_field_signature_group = 1,
    });
    // Rationalization.
    expected_events.push_back(RationalizationFieldLogEvent{
        .field_type = autofill_field_ptr->server_type(),
        .section_id = 1,
        .type_changed = false,
    });
    EXPECT_THAT(autofill_field_ptr->field_log_events(),
                ArrayEquals(expected_events));
  }
}

// Test that we record field log events correctly for rationalization when
// there are two address fields.
TEST_F(BrowserAutofillManagerWithLogEventsTest,
       LogEventsRationalizationTwoAddresses) {
  // Set up our form data.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  FormFieldData field;
  test::CreateTestFormField(/*label=*/"Full Name", /*name=*/"fullName",
                            /*value=*/"", /*type=*/"text", /*field=*/&field);
  form.fields.push_back(field);
  test::CreateTestFormField(/*label=*/"Address", /*name=*/"address",
                            /*value=*/"", /*type=*/"text", /*field=*/&field);
  form.fields.push_back(field);
  test::CreateTestFormField(/*label=*/"Address", /*name=*/"address",
                            /*value=*/"", /*type=*/"text", /*field=*/&field);
  form.fields.push_back(field);
  test::CreateTestFormField(/*label=*/"City", /*name=*/"city",
                            /*value=*/"", /*type=*/"text", /*field=*/&field);
  form.fields.push_back(field);

  // Simulate having seen this form on page load.
  // |form_structure_instance| will be owned by |browser_autofill_manager_|.
  auto form_structure_instance = std::make_unique<FormStructure>(form);
  // This pointer is valid as long as autofill manager lives.
  FormStructure* form_structure = form_structure_instance.get();
  form_structure->DetermineHeuristicTypes(nullptr, nullptr);
  browser_autofill_manager_->AddSeenFormStructure(
      std::move(form_structure_instance));

  // Make API response with suggestions.
  AutofillQueryResponse response;
  AutofillQueryResponse::FormSuggestion* form_suggestion;
  // Set suggestions for form.
  form_suggestion = response.add_form_suggestions();
  std::vector<ServerFieldType> server_types{
      NAME_FULL, ADDRESS_HOME_STREET_ADDRESS, ADDRESS_HOME_STREET_ADDRESS,
      ADDRESS_HOME_CITY};
  for (size_t i = 0; i < server_types.size(); ++i) {
    autofill::test::AddFieldPredictionToForm(form.fields[i], server_types[i],
                                             form_suggestion);
  }

  std::string response_string;
  ASSERT_TRUE(response.SerializeToString(&response_string));
  std::string encoded_response_string;
  base::Base64Encode(response_string, &encoded_response_string);

  // Query autofill server for the field type prediction.
  browser_autofill_manager_->OnLoadedServerPredictionsForTest(
      encoded_response_string, test::GetEncodedSignatures(*form_structure));
  std::vector<ServerFieldType> overall_types{
      NAME_FULL, ADDRESS_HOME_LINE1, ADDRESS_HOME_LINE2, ADDRESS_HOME_CITY};
  for (size_t i = 0; i < server_types.size(); ++i) {
    EXPECT_EQ(overall_types[i],
              form_structure->field(i)->Type().GetStorableType());
  }

  // Simulate form submission.
  FormSubmitted(form);

  for (const auto& autofill_field_ptr : *form_structure) {
    SCOPED_TRACE(autofill_field_ptr->parseable_label());
    size_t field_signature_rank =
        autofill_field_ptr->heuristic_type() == ADDRESS_HOME_LINE2 ? 2 : 1;
    std::vector<AutofillField::FieldLogEventType> expected_events =
        ToFieldTypeEvents(autofill_field_ptr->heuristic_type(),
                          autofill_field_ptr->heuristic_type(),
                          field_signature_rank);
    expected_events.push_back(ServerPredictionFieldLogEvent{
        .server_type1 = autofill_field_ptr->server_type(),
        .prediction_source1 =
            autofill_field_ptr->server_predictions()[0].source(),
        .server_type2 = NO_SERVER_DATA,
        .prediction_source2 = FieldPrediction::SOURCE_UNSPECIFIED,
        .server_type_prediction_is_override = false,
        .rank_in_field_signature_group = field_signature_rank,
    });
    // Rationalization.
    bool type_changed =
        autofill_field_ptr->parseable_label() == u"Address" ? true : false;
    expected_events.push_back(RationalizationFieldLogEvent{
        .field_type = autofill_field_ptr->Type().GetStorableType(),
        .section_id = 1,
        .type_changed = type_changed,
    });
    EXPECT_THAT(autofill_field_ptr->field_log_events(),
                ArrayEquals(expected_events));
  }
}

// Test that when Autocomplete is enabled and Autofill is disabled, form
// submissions are still received by the SingleFieldFormFillRouter.
TEST_F(BrowserAutofillManagerTest, FormSubmittedAutocompleteEnabled) {
  browser_autofill_manager_->SetAutofillProfileEnabled(autofill_client_, false);
  browser_autofill_manager_->SetAutofillCreditCardEnabled(autofill_client_,
                                                          false);

  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);

  EXPECT_CALL(*single_field_form_fill_router_, OnWillSubmitForm(_, _, true));
  FormSubmitted(form);
}

// Test that the value patterns metric is reported.
TEST_F(BrowserAutofillManagerTest, ValuePatternsMetric) {
  struct ValuePatternTestCase {
    const char* value;
    autofill::ValuePatternsMetric pattern;
  } kTestCases[] = {
      {"user@okaxis", autofill::ValuePatternsMetric::kUpiVpa},
      {"IT60X0542811101000000123456", autofill::ValuePatternsMetric::kIban}};
  for (const ValuePatternTestCase test_case : kTestCases) {
    // Set up our form data.
    FormData form;
    FormFieldData field;
    test::CreateTestFormField("Some label", "my-field", test_case.value, "text",
                              &field);
    field.is_focusable = true;  // The metric skips hidden fields.
    form.name = u"my-form";
    form.url = GURL("https://myform.com/form.html");
    form.action = GURL("https://myform.com/submit.html");
    form.fields.push_back(field);
    FormsSeen({form});

    base::HistogramTester histogram_tester;
    FormSubmitted(form);
    histogram_tester.ExpectUniqueSample("Autofill.SubmittedValuePatterns",
                                        test_case.pattern, 1);
  }
}

// Test that when Autofill is disabled, single field form fill suggestions are
// still queried as a fallback.
TEST_F(BrowserAutofillManagerTest,
       SingleFieldFormFillSuggestions_SomeWhenAutofillDisabled) {
  browser_autofill_manager_->SetAutofillProfileEnabled(autofill_client_, false);
  browser_autofill_manager_->SetAutofillCreditCardEnabled(autofill_client_,
                                                          false);
  auto external_delegate = std::make_unique<TestAutofillExternalDelegate>(
      browser_autofill_manager_.get(), autofill_driver_.get(),
      /*call_parent_methods=*/false);
  external_delegate_ = external_delegate.get();
  browser_autofill_manager_->SetExternalDelegateForTest(
      std::move(external_delegate));

  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  FormsSeen({form});

  // Expect the SingleFieldFormFillRouter to be called for suggestions.
  EXPECT_CALL(*single_field_form_fill_router_, OnGetSingleFieldSuggestions);

  GetAutofillSuggestions(form, form.fields[0]);

  // Single field form fill suggestions were returned, so we should not go
  // through the normal autofill flow.
  EXPECT_FALSE(external_delegate_->on_suggestions_returned_seen());
}

// Test that we do not query for single field form fill suggestions when there
// are Autofill suggestions available.
TEST_F(BrowserAutofillManagerTest,
       SingleFieldFormFillSuggestions_NoneWhenAutofillPresent) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  FormsSeen({form});

  // SingleFieldFormFillRouter is not called for suggestions.
  EXPECT_CALL(*single_field_form_fill_router_, OnGetSingleFieldSuggestions)
      .Times(0);

  GetAutofillSuggestions(form, form.fields[0]);
  // Verify that suggestions are returned.
  EXPECT_TRUE(external_delegate_->on_suggestions_returned_seen());
}

// Test that we query for single field form fill suggestions when there are no
// Autofill suggestions available.
TEST_F(BrowserAutofillManagerTest,
       SingleFieldFormFillSuggestions_SomeWhenAutofillEmpty) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  FormsSeen({form});

  // No suggestions matching "donkey".
  FormFieldData field;
  test::CreateTestFormField("Email", "email", "donkey", "email", &field);

  // Single field form fill manager is called for suggestions because Autofill
  // is empty.
  EXPECT_CALL(*single_field_form_fill_router_, OnGetSingleFieldSuggestions);

  GetAutofillSuggestions(form, field);
}

// Test that when Autofill is disabled and the field is a credit card name
// field, single field form fill suggestions are queried.
TEST_F(BrowserAutofillManagerTest,
       SingleFieldFormFillSuggestions_CreditCardNameFieldShouldAutocomplete) {
  TestAutofillClient client;
  // Since we are testing a form that submits over HTTP, we also need to set
  // the main frame to HTTP in the client, otherwise mixed form warnings will
  // trigger and autofill will be disabled.
  GURL::Replacements replacements;
  replacements.SetSchemeStr(url::kHttpScheme);
  client.set_form_origin(client.form_origin().ReplaceComponents(replacements));
  ResetBrowserAutofillManager(&client);
  browser_autofill_manager_->SetAutofillProfileEnabled(autofill_client_, false);
  browser_autofill_manager_->SetAutofillCreditCardEnabled(autofill_client_,
                                                          false);
  auto external_delegate = std::make_unique<TestAutofillExternalDelegate>(
      browser_autofill_manager_.get(), autofill_driver_.get(),
      /*call_parent_methods=*/false);
  external_delegate_ = external_delegate.get();
  browser_autofill_manager_->SetExternalDelegateForTest(
      std::move(external_delegate));

  // Set up our form data.
  FormData form = CreateTestCreditCardFormData(false, false);
  FormsSeen({form});
  // The first field is "Name on card", which should autocomplete.
  FormFieldData field = form.fields[0];
  field.should_autocomplete = true;

  // SingleFieldFormFillRouter is called for suggestions.
  EXPECT_CALL(*single_field_form_fill_router_, OnGetSingleFieldSuggestions);

  GetAutofillSuggestions(form, field);
}

// Test that when Autofill is disabled and the field is a credit card number
// field, single field form fill suggestions are not queried.
TEST_F(BrowserAutofillManagerTest,
       SingleFieldFormFillSuggestions_CreditCardNumberShouldNotAutocomplete) {
  TestAutofillClient client;
  // Since we are testing a form that submits over HTTP, we also need to set
  // the main frame to HTTP in the client, otherwise mixed form warnings will
  // trigger and autofill will be disabled.
  GURL::Replacements replacements;
  replacements.SetSchemeStr(url::kHttpScheme);
  client.set_form_origin(client.form_origin().ReplaceComponents(replacements));
  ResetBrowserAutofillManager(&client);
  browser_autofill_manager_->SetAutofillProfileEnabled(autofill_client_, false);
  browser_autofill_manager_->SetAutofillCreditCardEnabled(autofill_client_,
                                                          false);
  auto external_delegate = std::make_unique<TestAutofillExternalDelegate>(
      browser_autofill_manager_.get(), autofill_driver_.get(),
      /*call_parent_methods=*/false);
  external_delegate_ = external_delegate.get();
  browser_autofill_manager_->SetExternalDelegateForTest(
      std::move(external_delegate));

  // Set up our form data.
  FormData form = CreateTestCreditCardFormData(false, false);
  FormsSeen({form});
  // The second field is "Card Number", which should not autocomplete.
  FormFieldData field = form.fields[1];
  field.should_autocomplete = true;

  // SingleFieldFormFillRouter is not called for suggestions.
  EXPECT_CALL(*single_field_form_fill_router_, OnGetSingleFieldSuggestions)
      .Times(0);

  GetAutofillSuggestions(form, field);
}

// Test that the situation where there are no Autofill suggestions available,
// and no single field form fill conditions were met is correctly handled. The
// single field form fill conditions were not met because autocomplete is set to
// off and the field is not recognized as a promo code field.
TEST_F(
    BrowserAutofillManagerTest,
    SingleFieldFormFillSuggestions_NoneWhenAutofillEmptyAndSingleFieldFormFillConditionsNotMet) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  FormsSeen({form});

  // No suggestions matching "donkey".
  FormFieldData field;
  field.should_autocomplete = false;
  test::CreateTestFormField("Email", "email", "donkey", "email", &field);

  // Autocomplete is set to off, so suggestions should not get returned from
  // |single_field_form_fill_router_|.
  EXPECT_CALL(*single_field_form_fill_router_, OnGetSingleFieldSuggestions)
      .WillRepeatedly(testing::Return(false));

  GetAutofillSuggestions(form, field);

  // Single field form fill was not triggered, so go through the normal autofill
  // flow.
  EXPECT_TRUE(external_delegate_->on_suggestions_returned_seen());
}

// Test that the situation where no single field form fill conditions were met
// is handled correctly. The single field form fill conditions were not met
// because autocomplete is set to off and the field is not recognized as a promo
// code field.
TEST_F(
    BrowserAutofillManagerTest,
    SingleFieldFormFillSuggestions_NoneWhenSingleFieldFormFillConditionsNotMet) {
  browser_autofill_manager_->SetAutofillProfileEnabled(autofill_client_, false);
  browser_autofill_manager_->SetAutofillCreditCardEnabled(autofill_client_,
                                                          false);
  auto external_delegate = std::make_unique<TestAutofillExternalDelegate>(
      browser_autofill_manager_.get(), autofill_driver_.get(),
      /*call_parent_methods=*/false);
  external_delegate_ = external_delegate.get();
  browser_autofill_manager_->SetExternalDelegateForTest(
      std::move(external_delegate));

  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  FormsSeen({form});
  FormFieldData* field = &form.fields[0];
  field->should_autocomplete = false;

  // Autocomplete is set to off, so suggestions should not get returned from
  // |single_field_form_fill_router_|.
  EXPECT_CALL(*single_field_form_fill_router_, OnGetSingleFieldSuggestions)
      .WillRepeatedly(testing::Return(false));

  GetAutofillSuggestions(form, *field);

  // Single field form fill was not triggered, so go through the normal autofill
  // flow.
  EXPECT_TRUE(external_delegate_->on_suggestions_returned_seen());
}

TEST_F(BrowserAutofillManagerTest,
       DestructorCancelsSingleFieldFormFillQueries) {
  EXPECT_CALL(*single_field_form_fill_router_, CancelPendingQueries).Times(1);
  browser_autofill_manager_.reset();
}

// Make sure that we don't error out when AutocompleteHistoryManager was
// destroyed before BrowserAutofillManager.
TEST_F(BrowserAutofillManagerTest, Destructor_DeletedAutocomplete_Works) {
  // The assertion here is that no exceptions will be thrown.
  autocomplete_history_manager_.reset();
  browser_autofill_manager_.reset();
}

// Test that OnLoadedServerPredictions can obtain the FormStructure with the
// signature of the queried form from the API and apply type predictions.
// What we test here:
//  * The API response parser is used.
//  * The query can be processed with a response from the API.
TEST_F(BrowserAutofillManagerTest, OnLoadedServerPredictionsFromApi) {
  // First form on the page.
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.unique_renderer_id = test::MakeFormRendererId();
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  FormFieldData field;
  test::CreateTestFormField(/*label=*/"City", /*name=*/"city",
                            /*value=*/"", /*type=*/"text", /*field=*/&field);
  form.fields.push_back(field);
  test::CreateTestFormField(/*label=*/"State", /*name=*/"state",
                            /*value=*/"", /*type=*/"text", /*field=*/&field);
  form.fields.push_back(field);
  test::CreateTestFormField(/*label=*/"Postal Code", /*name=*/"zipcode",
                            /*value=*/"", /*type=*/"text", /*field=*/&field);
  form.fields.push_back(field);
  // Simulate having seen this form on page load.
  // |form_structure_instance| will be owned by |browser_autofill_manager_|.
  auto form_structure_instance = std::make_unique<FormStructure>(form);
  // This pointer is valid as long as autofill manager lives.
  FormStructure* form_structure = form_structure_instance.get();
  form_structure->DetermineHeuristicTypes(nullptr, nullptr);
  browser_autofill_manager_->AddSeenFormStructure(
      std::move(form_structure_instance));

  // Second form on the page.
  FormData form2;
  form2.host_frame = test::MakeLocalFrameToken();
  form2.unique_renderer_id = test::MakeFormRendererId();
  form2.name = u"MyForm2";
  form2.url = GURL("https://myform.com/form.html");
  form2.action = GURL("https://myform.com/submit.html");
  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Middle Name", "middlename", "", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Postal Code", "zipcode", "", "text", &field);
  form2.fields.push_back(field);
  auto form_structure_instance2 = std::make_unique<FormStructure>(form2);
  // This pointer is valid as long as autofill manager lives.
  FormStructure* form_structure2 = form_structure_instance2.get();
  form_structure2->DetermineHeuristicTypes(nullptr, nullptr);
  browser_autofill_manager_->AddSeenFormStructure(
      std::move(form_structure_instance2));

  // Make API response with suggestions.
  AutofillQueryResponse response;
  AutofillQueryResponse::FormSuggestion* form_suggestion;
  // Set suggestions for form 1.
  form_suggestion = response.add_form_suggestions();
  autofill::test::AddFieldPredictionToForm(form.fields[0], ADDRESS_HOME_CITY,
                                           form_suggestion);
  autofill::test::AddFieldPredictionToForm(form.fields[1], ADDRESS_HOME_STATE,
                                           form_suggestion);
  autofill::test::AddFieldPredictionToForm(form.fields[2], ADDRESS_HOME_ZIP,
                                           form_suggestion);
  // Set suggestions for form 2.
  form_suggestion = response.add_form_suggestions();
  autofill::test::AddFieldPredictionToForm(form2.fields[0], NAME_LAST,
                                           form_suggestion);
  autofill::test::AddFieldPredictionToForm(form2.fields[1], NAME_MIDDLE,
                                           form_suggestion);
  autofill::test::AddFieldPredictionToForm(form2.fields[2], ADDRESS_HOME_ZIP,
                                           form_suggestion);

  std::string response_string;
  ASSERT_TRUE(response.SerializeToString(&response_string));
  std::string encoded_response_string;
  base::Base64Encode(response_string, &encoded_response_string);

  std::vector<FormSignature> signatures =
      test::GetEncodedSignatures({form_structure, form_structure2});

  // Run method under test.
  base::HistogramTester histogram_tester;
  browser_autofill_manager_->OnLoadedServerPredictionsForTest(
      encoded_response_string, signatures);

  // Verify whether the relevant histograms were updated.
  histogram_tester.ExpectBucketCount("Autofill.ServerQueryResponse",
                                     AutofillMetrics::QUERY_RESPONSE_RECEIVED,
                                     1);
  histogram_tester.ExpectBucketCount("Autofill.ServerQueryResponse",
                                     AutofillMetrics::QUERY_RESPONSE_PARSED, 1);

  // We expect the server suggestions to have been applied to the first field of
  // the first form.
  EXPECT_EQ(ADDRESS_HOME_CITY,
            form_structure->field(0)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_STATE,
            form_structure->field(1)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_ZIP,
            form_structure->field(2)->Type().GetStorableType());
  // We expect the server suggestions to have been applied to the second form as
  // well.
  EXPECT_EQ(NAME_LAST, form_structure2->field(0)->Type().GetStorableType());
  EXPECT_EQ(NAME_MIDDLE, form_structure2->field(1)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_ZIP,
            form_structure2->field(2)->Type().GetStorableType());
}

// Test that OnLoadedServerPredictions does not call ParseQueryResponse if the
// BrowserAutofillManager has been reset between the time the query was sent and
// the response received.
TEST_F(BrowserAutofillManagerTest, OnLoadedServerPredictions_ResetManager) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |browser_autofill_manager_|.
  auto form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes(nullptr, nullptr);
  std::vector<FormSignature> signatures =
      test::GetEncodedSignatures(*form_structure);
  browser_autofill_manager_->AddSeenFormStructure(std::move(form_structure));

  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  form_suggestion->add_field_suggestions()->add_predictions()->set_type(3);
  for (int i = 0; i < 7; ++i) {
    form_suggestion->add_field_suggestions()->add_predictions()->set_type(0);
  }
  form_suggestion->add_field_suggestions()->add_predictions()->set_type(3);
  form_suggestion->add_field_suggestions()->add_predictions()->set_type(2);
  form_suggestion->add_field_suggestions()->add_predictions()->set_type(61);

  std::string response_string;
  ASSERT_TRUE(response.SerializeToString(&response_string));

  std::string response_string_base64;
  base::Base64Encode(response_string, &response_string_base64);

  // Reset the manager (such as during a navigation).
  browser_autofill_manager_->Reset();

  base::HistogramTester histogram_tester;
  browser_autofill_manager_->OnLoadedServerPredictionsForTest(
      response_string_base64, signatures);

  // Verify that FormStructure::ParseQueryResponse was NOT called.
  histogram_tester.ExpectTotalCount("Autofill.ServerQueryResponse", 0);
}

// Test that when server predictions disagree with the heuristic ones, the
// overall types and sections would be set based on the server one.
TEST_F(BrowserAutofillManagerTest, DetermineHeuristicsWithOverallPrediction) {
  // Set up our form data.
  FormData form;
  form.url = GURL("https://www.myform.com");
  FormFieldData field;
  test::CreateTestFormField("First Name", "firstname", "", "text", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("Expiration Year", "exp_year", "", "text", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("Expiration Month", "exp_month", "", "text",
                            &field);
  form.fields.push_back(field);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |browser_autofill_manager_|.
  FormStructure* form_structure = [&] {
    auto form_structure = std::make_unique<FormStructure>(form);
    FormStructure* ptr = form_structure.get();
    form_structure->DetermineHeuristicTypes(nullptr, nullptr);
    browser_autofill_manager_->AddSeenFormStructure(std::move(form_structure));
    return ptr;
  }();

  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  autofill::test::AddFieldPredictionToForm(
      form.fields[0], CREDIT_CARD_NAME_FIRST, form_suggestion);
  autofill::test::AddFieldPredictionToForm(
      form.fields[1], CREDIT_CARD_NAME_LAST, form_suggestion);
  autofill::test::AddFieldPredictionToForm(form.fields[2], CREDIT_CARD_NUMBER,
                                           form_suggestion);
  autofill::test::AddFieldPredictionToForm(
      form.fields[3], CREDIT_CARD_EXP_MONTH, form_suggestion);
  autofill::test::AddFieldPredictionToForm(
      form.fields[4], CREDIT_CARD_EXP_4_DIGIT_YEAR, form_suggestion);

  std::string response_string;
  ASSERT_TRUE(response.SerializeToString(&response_string));

  std::string response_string_base64;
  base::Base64Encode(response_string, &response_string_base64);

  base::HistogramTester histogram_tester;
  browser_autofill_manager_->OnLoadedServerPredictionsForTest(
      response_string_base64, test::GetEncodedSignatures(*form_structure));
  // Verify that FormStructure::ParseQueryResponse was called (here and below).
  histogram_tester.ExpectBucketCount("Autofill.ServerQueryResponse",
                                     AutofillMetrics::QUERY_RESPONSE_RECEIVED,
                                     1);
  histogram_tester.ExpectBucketCount("Autofill.ServerQueryResponse",
                                     AutofillMetrics::QUERY_RESPONSE_PARSED, 1);

  // Since the card holder name appears as the first name + last name (rather
  // than the full name), and since they appears as the first fields of the
  // section, the heuristics detect them as the address first/last name.
  EXPECT_EQ(NAME_FIRST, form_structure->field(0)->heuristic_type());
  EXPECT_EQ(NAME_LAST, form_structure->field(1)->heuristic_type());
  EXPECT_EQ(CREDIT_CARD_NUMBER, form_structure->field(2)->heuristic_type());
  EXPECT_EQ(CREDIT_CARD_EXP_MONTH, form_structure->field(3)->heuristic_type());
  EXPECT_EQ(CREDIT_CARD_EXP_4_DIGIT_YEAR,
            form_structure->field(4)->heuristic_type());

  // We expect to see the server type as the overall type.
  EXPECT_EQ(CREDIT_CARD_NAME_FIRST,
            form_structure->field(0)->Type().GetStorableType());
  EXPECT_EQ(CREDIT_CARD_NAME_LAST,
            form_structure->field(1)->Type().GetStorableType());
  EXPECT_EQ(CREDIT_CARD_NUMBER, form_structure->field(2)->heuristic_type());
  EXPECT_EQ(CREDIT_CARD_EXP_MONTH, form_structure->field(3)->heuristic_type());
  EXPECT_EQ(CREDIT_CARD_EXP_4_DIGIT_YEAR,
            form_structure->field(4)->heuristic_type());

  // Although the heuristic types of the first two fields belongs to the address
  // section, the final fields' section should be based on the overall
  // prediction, therefore they should be grouped in one section.
  const auto section = form_structure->field(0)->section;
  EXPECT_EQ(section, form_structure->field(1)->section);
  EXPECT_EQ(section, form_structure->field(2)->section);
  EXPECT_EQ(section, form_structure->field(3)->section);
  EXPECT_EQ(section, form_structure->field(4)->section);
}

// Test that we are able to save form data when forms are submitted and we only
// have server data for the field types.
TEST_F(BrowserAutofillManagerTest, FormSubmittedServerTypes) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  FormsSeen({form});

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |browser_autofill_manager_|.
  auto form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes(nullptr, nullptr);

  // Clear the heuristic types, and instead set the appropriate server types.
  std::vector<ServerFieldType> heuristic_types, server_types;
  for (size_t i = 0; i < form.fields.size(); ++i) {
    heuristic_types.push_back(UNKNOWN_TYPE);
    server_types.push_back(form_structure->field(i)->heuristic_type());
  }
  FormStructureTestApi(form_structure.get())
      .SetFieldTypes(heuristic_types, server_types);
  browser_autofill_manager_->AddSeenFormStructure(std::move(form_structure));

  // Fill the form.
  const char guid[] = "00000000-0000-0000-0000-000000000001";
  FormData response_data;
  FillAutofillFormDataAndSaveResults(form, form.fields[0],
                                     MakeFrontendId({.profile_id = guid}),
                                     &response_data);
  ExpectFilledAddressFormElvis(response_data, false);

  // Simulate form submission. We should call into the PDM to try to save the
  // filled data.
  FormSubmitted(response_data);
  EXPECT_EQ(1, personal_data().num_times_save_imported_profile_called());
}

// Test that the form signature for an uploaded form always matches the form
// signature from the query.
TEST_F(BrowserAutofillManagerTest, FormSubmittedWithDifferentFields) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  FormsSeen({form});

  // Cache the expected form signature.
  std::string signature = FormStructure(form).FormSignatureAsStr();

  // Change the structure of the form prior to submission.
  // Websites would typically invoke JavaScript either on page load or on form
  // submit to achieve this.
  form.fields.pop_back();
  FormFieldData field = form.fields[3];
  form.fields[3] = form.fields[7];
  form.fields[7] = field;

  // Simulate form submission.
  FormSubmitted(form);
  EXPECT_EQ(signature, browser_autofill_manager_->GetSubmittedFormSignature());
}

// Test that we do not save form data when submitted fields contain default
// values.
TEST_F(BrowserAutofillManagerTest, FormSubmittedWithDefaultValues) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  form.fields[3].value = u"Enter your address";

  // Convert the state field to a <select> popup, to make sure that we only
  // reject default values for text fields.
  ASSERT_TRUE(form.fields[6].name == u"state");
  form.fields[6].form_control_type = "select-one";
  form.fields[6].value = u"Tennessee";

  FormsSeen({form});

  // Fill the form.
  const char guid[] = "00000000-0000-0000-0000-000000000001";
  FormData response_data;
  FillAutofillFormDataAndSaveResults(form, form.fields[3],
                                     MakeFrontendId({.profile_id = guid}),
                                     &response_data);
  // Set the address field's value back to the default value.
  response_data.fields[3].value = u"Enter your address";

  // Simulate form submission.  We should not call into the PDM to try to save
  // the filled data, since the filled form is effectively missing an address.
  FormSubmitted(response_data);
  EXPECT_EQ(0, personal_data().num_times_save_imported_profile_called());
}

struct ProfileMatchingTypesTestCase {
  const char* input_value;         // The value to input in the field.
  ServerFieldTypeSet field_types;  // The expected field types to be determined.
};

class ProfileMatchingTypesTest
    : public BrowserAutofillManagerTest,
      public ::testing::WithParamInterface<ProfileMatchingTypesTestCase> {
 protected:
  void SetUp() override { BrowserAutofillManagerTest::SetUp(); }
};

const ProfileMatchingTypesTestCase kProfileMatchingTypesTestCases[] = {
    // Profile fields matches.
    {"Elvis", {NAME_FIRST}},
    {"Aaron", {NAME_MIDDLE}},
    {"A", {NAME_MIDDLE_INITIAL}},
    {"Presley", {NAME_LAST, NAME_LAST_SECOND}},
    {"Elvis Aaron Presley", {NAME_FULL}},
    {"theking@gmail.com", {EMAIL_ADDRESS}},
    {"RCA", {COMPANY_NAME}},
    {"3734 Elvis Presley Blvd.", {ADDRESS_HOME_LINE1}},
    {"3734", {ADDRESS_HOME_HOUSE_NUMBER}},
    {"Elvis Presley Blvd.",
     {ADDRESS_HOME_STREET_NAME, ADDRESS_HOME_STREET_AND_DEPENDENT_STREET_NAME}},
    {"Apt. 10", {ADDRESS_HOME_LINE2, ADDRESS_HOME_SUBPREMISE}},
    {"Memphis", {ADDRESS_HOME_CITY}},
    {"Tennessee", {ADDRESS_HOME_STATE}},
    {"38116", {ADDRESS_HOME_ZIP}},
    {"USA", {ADDRESS_HOME_COUNTRY}},
    {"United States", {ADDRESS_HOME_COUNTRY}},
    {"12345678901", {PHONE_HOME_WHOLE_NUMBER}},
    {"+1 (234) 567-8901", {PHONE_HOME_WHOLE_NUMBER}},
    {"(234)567-8901", {PHONE_HOME_CITY_AND_NUMBER}},
    {"2345678901", {PHONE_HOME_CITY_AND_NUMBER}},
    {"1", {PHONE_HOME_COUNTRY_CODE}},
    {"234", {PHONE_HOME_CITY_CODE}},
    {"5678901", {PHONE_HOME_NUMBER}},
    {"567", {PHONE_HOME_NUMBER_PREFIX}},
    {"8901", {PHONE_HOME_NUMBER_SUFFIX}},

    // Test a European profile.
    {"Paris", {ADDRESS_HOME_CITY}},
    {"Île de France", {ADDRESS_HOME_STATE}},    // Exact match
    {"Ile de France", {ADDRESS_HOME_STATE}},    // Missing accent.
    {"-Ile-de-France-", {ADDRESS_HOME_STATE}},  // Extra punctuation.
    {"île dÉ FrÃÑÇË", {ADDRESS_HOME_STATE}},  // Other accents & case mismatch.
    {"75008", {ADDRESS_HOME_ZIP}},
    {"FR", {ADDRESS_HOME_COUNTRY}},
    {"France", {ADDRESS_HOME_COUNTRY}},
    {"33249197070", {PHONE_HOME_WHOLE_NUMBER}},
    {"+33 2 49 19 70 70", {PHONE_HOME_WHOLE_NUMBER}},
    {"02 49 19 70 70", {PHONE_HOME_CITY_AND_NUMBER}},
    {"0249197070", {PHONE_HOME_CITY_AND_NUMBER}},
    {"33", {PHONE_HOME_COUNTRY_CODE}},
    {"2", {PHONE_HOME_CITY_CODE}},

    // Credit card fields matches.
    {"John Doe", {CREDIT_CARD_NAME_FULL}},
    {"John", {CREDIT_CARD_NAME_FIRST}},
    {"Doe", {CREDIT_CARD_NAME_LAST}},
    {"4234-5678-9012-3456", {CREDIT_CARD_NUMBER}},
    {"04", {CREDIT_CARD_EXP_MONTH}},
    {"April", {CREDIT_CARD_EXP_MONTH}},
    {"2999", {CREDIT_CARD_EXP_4_DIGIT_YEAR}},
    {"99", {CREDIT_CARD_EXP_2_DIGIT_YEAR}},
    {"04/2999", {CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR}},

    // Make sure whitespace and invalid characters are handled properly.
    {"", {EMPTY_TYPE}},
    {" ", {EMPTY_TYPE}},
    {"***", {UNKNOWN_TYPE}},
    {" Elvis", {NAME_FIRST}},
    {"Elvis ", {NAME_FIRST}},

    // Make sure fields that differ by case match.
    {"elvis ", {NAME_FIRST}},
    {"UnItEd StAtEs", {ADDRESS_HOME_COUNTRY}},

    // Make sure fields that differ by punctuation match.
    {"3734 Elvis Presley Blvd", {ADDRESS_HOME_LINE1}},
    {"3734, Elvis    Presley Blvd.", {ADDRESS_HOME_LINE1}},

    // Make sure that a state's full name and abbreviation match.
    {"TN", {ADDRESS_HOME_STATE}},     // Saved as "Tennessee" in profile.
    {"Texas", {ADDRESS_HOME_STATE}},  // Saved as "TX" in profile.

    // Special phone number case. A profile with no country code should
    // only match PHONE_HOME_CITY_AND_NUMBER.
    {"5142821292", {PHONE_HOME_CITY_AND_NUMBER}},

    // Make sure unsupported variants do not match.
    {"Elvis Aaron", {UNKNOWN_TYPE}},
    {"Mr. Presley", {UNKNOWN_TYPE}},
    {"3734 Elvis Presley", {UNKNOWN_TYPE}},
    {"38116-1023", {UNKNOWN_TYPE}},
    {"5", {UNKNOWN_TYPE}},
    {"56", {UNKNOWN_TYPE}},
    {"901", {UNKNOWN_TYPE}},
};

// Tests that DeterminePossibleFieldTypesForUpload finds accurate possible
// types.
TEST_P(ProfileMatchingTypesTest, DeterminePossibleFieldTypesForUpload) {
  // Unpack the test parameters
  const auto& test_case = GetParam();

  SCOPED_TRACE(base::StringPrintf(
      "Test: input_value='%s', field_type=%s, structured_names=%s ",
      test_case.input_value,
      AutofillType(*test_case.field_types.begin()).ToString().c_str(), "true"));

  // Take the field types depending on the state of the structured names
  // feature.
  const ServerFieldTypeSet& expected_possible_types = test_case.field_types;

  // Set up the test profiles.
  std::vector<AutofillProfile> profiles;
  profiles.resize(3);
  test::SetProfileInfo(&profiles[0], "Elvis", "Aaron", "Presley",
                       "theking@gmail.com", "RCA", "3734 Elvis Presley Blvd.",
                       "Apt. 10", "Memphis", "Tennessee", "38116", "US",
                       "+1 (234) 567-8901");
  profiles[0].set_guid("00000000-0000-0000-0000-000000000001");

  test::SetProfileInfo(&profiles[1], "Charles", "", "Holley", "buddy@gmail.com",
                       "Decca", "123 Apple St.", "unit 6", "Lubbock", "TX",
                       "79401", "US", "5142821292");
  profiles[1].set_guid("00000000-0000-0000-0000-000000000002");

  test::SetProfileInfo(&profiles[2], "Charles", "", "Baudelaire",
                       "lesfleursdumal@gmail.com", "", "108 Rue Saint-Lazare",
                       "Apt. 11", "Paris", "Île de France", "75008", "FR",
                       "+33 2 49 19 70 70");
  profiles[2].set_guid("00000000-0000-0000-0000-000000000001");

  // Set up the test credit cards.
  std::vector<CreditCard> credit_cards;
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "John Doe", "4234-5678-9012-3456", "04",
                          "2999", "1");
  credit_card.set_guid("00000000-0000-0000-0000-000000000003");
  credit_cards.push_back(credit_card);

  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");

  FormFieldData field;
  test::CreateTestFormField("", "1", "", "text", &field);
  field.value = UTF8ToUTF16(test_case.input_value);
  form.fields.push_back(field);

  FormStructure form_structure(form);

  BrowserAutofillManager::DeterminePossibleFieldTypesForUploadForTest(
      profiles, credit_cards, std::u16string(), "en-us", &form_structure);

  ASSERT_EQ(1U, form_structure.field_count());

  ServerFieldTypeSet possible_types = form_structure.field(0)->possible_types();
  EXPECT_EQ(possible_types, expected_possible_types);
}

// TODO(crbug.com/1395740). Remove parameter, once
// kAutofillVoteForSelectOptionValues has settled on stable.
class DeterminePossibleFieldTypesForUploadOfSelectTest
    : public BrowserAutofillManagerTest,
      public ::testing::WithParamInterface<bool> {
 protected:
  void SetUp() override { BrowserAutofillManagerTest::SetUp(); }
};

// Tests that DeterminePossibleFieldTypesForUpload considers both the value
// and the human readable part of an <option> element in a <select> element:
// <option value="this is the value">this is the human readable part</option>
//
// In particular <option value="US">USA (+1)</option> is probably part of a
// phone number country code.
TEST_P(DeterminePossibleFieldTypesForUploadOfSelectTest,
       DeterminePossibleFieldTypesForUploadOfSelect) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatureState(features::kAutofillVoteForSelectOptionValues,
                                GetParam());

  // Set up a profile and no credit cards.
  std::vector<AutofillProfile> profiles(1);
  test::SetProfileInfo(&profiles[0], "Elvis", "Aaron", "Presley",
                       "theking@gmail.com", "RCA", "3734 Elvis Presley Blvd.",
                       "Apt. 10", "Memphis", "Tennessee", "38116", "US",
                       "+1 (234) 567-8901");
  profiles[0].set_guid("00000000-0000-0000-0000-000000000001");
  std::vector<CreditCard> credit_cards;

  // Set up the form to be tested.
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");

  // We want the "Memphis" in <option value="2">Memphis</option> to be
  // recognized.
  FormFieldData city_field;
  test::CreateTestSelectField(
      "label", "name", /*value=*/"2",
      /*values=*/{"1", "2", "3"},
      /*contents=*/{"New York", "Memphis", "Gotham City"}, &city_field);

  // We want the +1 in <option value="US">USA (+1)</option> to be recognized
  // as a phone country code. Despite the value "US", we don't want this to be
  // recognized as a country field.
  FormFieldData phone_country_code_field;
  test::CreateTestSelectField(
      "label", "name", /*value=*/"US", /*values=*/{"US", "DE"},
      /*contents=*/{"USA (+1)", "Germany (+49)"}, &phone_country_code_field);

  form.fields = {city_field, phone_country_code_field};

  FormStructure form_structure(form);

  // Validate expectations.
  BrowserAutofillManager::DeterminePossibleFieldTypesForUploadForTest(
      profiles, credit_cards, std::u16string(), "en-us", &form_structure);

  ASSERT_EQ(2U, form_structure.field_count());
  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillVoteForSelectOptionValues)) {
    EXPECT_EQ(form_structure.field(0)->possible_types(),
              ServerFieldTypeSet({ADDRESS_HOME_CITY}));
    EXPECT_EQ(form_structure.field(1)->possible_types(),
              ServerFieldTypeSet({PHONE_HOME_COUNTRY_CODE}));
  } else {
    EXPECT_EQ(form_structure.field(0)->possible_types(),
              ServerFieldTypeSet({UNKNOWN_TYPE}));
    EXPECT_EQ(form_structure.field(1)->possible_types(),
              ServerFieldTypeSet({ADDRESS_HOME_COUNTRY}));
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         DeterminePossibleFieldTypesForUploadOfSelectTest,
                         testing::Bool());

// Tests that DeterminePossibleFieldTypesForUpload is called when a form is
// submitted.
TEST_F(BrowserAutofillManagerTest,
       DeterminePossibleFieldTypesForUpload_IsTriggered) {
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");

  std::vector<ServerFieldTypeSet> expected_types;
  std::vector<std::u16string> expected_values;

  // These fields should all match.
  FormFieldData field;
  ServerFieldTypeSet types;

  test::CreateTestFormField("", "1", "", "text", &field);
  expected_values.push_back(u"Elvis");
  types.clear();
  types.insert(NAME_FIRST);
  form.fields.push_back(field);
  expected_types.push_back(types);

  test::CreateTestFormField("", "2", "", "text", &field);
  expected_values.push_back(u"Aaron");
  types.clear();
  types.insert(NAME_MIDDLE);
  form.fields.push_back(field);
  expected_types.push_back(types);

  test::CreateTestFormField("", "3", "", "text", &field);
  expected_values.push_back(u"A");
  types.clear();
  types.insert(NAME_MIDDLE_INITIAL);
  form.fields.push_back(field);
  expected_types.push_back(types);

  // Make sure the form is in the cache so that it is processed for Autofill
  // upload.
  FormsSeen({form});

  // Once the form is cached, fill the values.
  EXPECT_EQ(form.fields.size(), expected_values.size());
  for (size_t i = 0; i < expected_values.size(); i++) {
    form.fields[i].value = expected_values[i];
  }

  browser_autofill_manager_->SetExpectedSubmittedFieldTypes(expected_types);
  FormSubmitted(form);
}

// Tests that DisambiguateUploadTypes makes the correct choices.
TEST_F(BrowserAutofillManagerTest, DisambiguateUploadTypes) {
  // Set up the test profile.
  std::vector<AutofillProfile> profiles;
  AutofillProfile profile;
  test::SetProfileInfo(&profile, "Elvis", "Aaron", "Presley",
                       "theking@gmail.com", "RCA", "3734 Elvis Presley Blvd.",
                       "", "Memphis", "Tennessee", "38116", "US",
                       "(234) 567-8901");
  profile.set_guid("00000000-0000-0000-0000-000000000001");
  profiles.push_back(profile);

  // Set up the test credit card.
  std::vector<CreditCard> credit_cards;
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "Elvis Presley", "4234-5678-9012-3456",
                          "04", "2999", "1");
  credit_card.set_guid("00000000-0000-0000-0000-000000000003");
  credit_cards.push_back(credit_card);

  struct TestFieldData {
    std::u16string input_value;
    ServerFieldType predicted_type;
    bool expect_disambiguation;
    ServerFieldType expected_upload_type;
  };
  using TestCase = std::vector<TestFieldData>;

  std::vector<TestCase> test_cases;

  // Address disambiguation.
  // An ambiguous address line followed by a field predicted as a line 2 and
  // that is empty should be disambiguated as an ADDRESS_HOME_LINE1.
  test_cases.push_back({{u"3734 Elvis Presley Blvd.", ADDRESS_HOME_LINE1, true,
                         ADDRESS_HOME_LINE1},
                        {u"", ADDRESS_HOME_LINE2, true, EMPTY_TYPE}});

  // An ambiguous address line followed by a field predicted as a line 2 but
  // filled with another know profile value should be disambiguated as an
  // ADDRESS_HOME_STREET_ADDRESS.
  test_cases.push_back(
      {{u"3734 Elvis Presley Blvd.", ADDRESS_HOME_STREET_ADDRESS, true,
        ADDRESS_HOME_STREET_ADDRESS},
       {u"38116", ADDRESS_HOME_LINE2, true, ADDRESS_HOME_ZIP}});

  // An ambiguous address line followed by an empty field predicted as
  // something other than a line 2 should be disambiguated as an
  // ADDRESS_HOME_STREET_ADDRESS.
  test_cases.push_back(
      {{u"3734 Elvis Presley Blvd.", ADDRESS_HOME_STREET_ADDRESS, true,
        ADDRESS_HOME_STREET_ADDRESS},
       {u"", ADDRESS_HOME_ZIP, true, EMPTY_TYPE}});

  // An ambiguous address line followed by no other field should be
  // disambiguated as an ADDRESS_HOME_STREET_ADDRESS.
  test_cases.push_back(
      {{u"3734 Elvis Presley Blvd.", ADDRESS_HOME_STREET_ADDRESS, true,
        ADDRESS_HOME_STREET_ADDRESS}});

  // Name disambiguation.
  // An ambiguous name field that has no next field and that is preceded by
  // a non credit card field should be disambiguated as a non credit card
  // name.
  test_cases.push_back(
      {{u"Memphis", ADDRESS_HOME_CITY, true, ADDRESS_HOME_CITY},
       {u"Elvis", CREDIT_CARD_NAME_FIRST, true, NAME_FIRST},
       {u"Presley", CREDIT_CARD_NAME_LAST, true, NAME_LAST}});

  // An ambiguous name field that has no next field and that is preceded by
  // a credit card field should be disambiguated as a credit card name.
  test_cases.push_back(
      {{u"4234-5678-9012-3456", CREDIT_CARD_NUMBER, true, CREDIT_CARD_NUMBER},
       {u"Elvis", NAME_FIRST, true, CREDIT_CARD_NAME_FIRST},
       {u"Presley", NAME_LAST, true, CREDIT_CARD_NAME_LAST}});

  // An ambiguous name field that has no previous field and that is
  // followed by a non credit card field should be disambiguated as a non
  // credit card name.
  test_cases.push_back(
      {{u"Elvis", CREDIT_CARD_NAME_FIRST, true, NAME_FIRST},
       {u"Presley", CREDIT_CARD_NAME_LAST, true, NAME_LAST},

       {u"Memphis", ADDRESS_HOME_CITY, true, ADDRESS_HOME_CITY}});

  // An ambiguous name field that has no previous field and that is followed
  // by a credit card field should be disambiguated as a credit card name.
  test_cases.push_back(
      {{u"Elvis", NAME_FIRST, true, CREDIT_CARD_NAME_FIRST},
       {u"Presley", NAME_LAST, true, CREDIT_CARD_NAME_LAST},
       {u"4234-5678-9012-3456", CREDIT_CARD_NUMBER, true, CREDIT_CARD_NUMBER}});

  // An ambiguous name field that is preceded and followed by non credit
  // card fields should be disambiguated as a non credit card name.
  test_cases.push_back(
      {{u"Memphis", ADDRESS_HOME_CITY, true, ADDRESS_HOME_CITY},
       {u"Elvis", CREDIT_CARD_NAME_FIRST, true, NAME_FIRST},
       {u"Presley", CREDIT_CARD_NAME_LAST, true, NAME_LAST},
       {u"Tennessee", ADDRESS_HOME_STATE, true, ADDRESS_HOME_STATE}});

  // An ambiguous name field that is preceded and followed by credit card
  // fields should be disambiguated as a credit card name.
  test_cases.push_back(
      {{u"4234-5678-9012-3456", CREDIT_CARD_NUMBER, true, CREDIT_CARD_NUMBER},
       {u"Elvis", NAME_FIRST, true, CREDIT_CARD_NAME_FIRST},
       {u"Presley", NAME_LAST, true, CREDIT_CARD_NAME_LAST},
       {u"2999", CREDIT_CARD_EXP_4_DIGIT_YEAR, true,
        CREDIT_CARD_EXP_4_DIGIT_YEAR}});

  // An ambiguous name field that is preceded by a non credit card field and
  // followed by a credit card field should not be disambiguated.
  test_cases.push_back(
      {{u"Memphis", ADDRESS_HOME_CITY, true, ADDRESS_HOME_CITY},
       {u"Elvis", NAME_FIRST, false, CREDIT_CARD_NAME_FIRST},
       {u"Presley", NAME_LAST, false, CREDIT_CARD_NAME_LAST},
       {u"2999", CREDIT_CARD_EXP_4_DIGIT_YEAR, true,
        CREDIT_CARD_EXP_4_DIGIT_YEAR}});

  // An ambiguous name field that is preceded by a credit card field and
  // followed by a non credit card field should not be disambiguated.
  test_cases.push_back(
      {{u"2999", CREDIT_CARD_EXP_4_DIGIT_YEAR, true,
        CREDIT_CARD_EXP_4_DIGIT_YEAR},
       {u"Elvis", NAME_FIRST, false, CREDIT_CARD_NAME_FIRST},
       {u"Presley", NAME_LAST, false, CREDIT_CARD_NAME_LAST},
       {u"Memphis", ADDRESS_HOME_CITY, true, ADDRESS_HOME_CITY}});

  for (const TestCase& test_fields : test_cases) {
    FormData form;
    form.name = u"MyForm";
    form.url = GURL("https://myform.com/form.html");
    form.action = GURL("https://myform.com/submit.html");

    // Create the form fields specified in the test case.
    FormFieldData field;
    for (const TestFieldData& test_field : test_fields) {
      test::CreateTestFormField("", "1", "", "text", &field);
      field.value = test_field.input_value;
      form.fields.push_back(field);
    }

    // Assign the specified predicted type for each field in the test case.
    FormStructure form_structure(form);
    for (size_t i = 0; i < test_fields.size(); ++i) {
      form_structure.field(i)->set_server_predictions(
          {::autofill::test::CreateFieldPrediction(
              test_fields[i].predicted_type)});
    }

    BrowserAutofillManager::DeterminePossibleFieldTypesForUploadForTest(
        profiles, credit_cards, std::u16string(), "en-us", &form_structure);
    ASSERT_EQ(test_fields.size(), form_structure.field_count());

    // Make sure the disambiguation method selects the expected upload type.
    ServerFieldTypeSet possible_types;
    for (size_t i = 0; i < test_fields.size(); ++i) {
      possible_types = form_structure.field(i)->possible_types();
      if (test_fields[i].expect_disambiguation) {
        // It is possible that a field as two out of three
        // possible classifications: NAME_FULL, NAME_LAST,
        // NAME_LAST_FIRST/SECOND. Note, all cases contain NAME_LAST.
        // Alternatively, if the street address contains only one line, the
        // street address and the address line1 are identical resulting in a
        // vote for each.
        if (possible_types.size() == 2) {
          EXPECT_TRUE((possible_types.contains(NAME_LAST) &&
                       (possible_types.contains(NAME_LAST_SECOND) ||
                        possible_types.contains(NAME_LAST_FIRST) ||
                        possible_types.contains(NAME_FULL))) ||
                      (possible_types.contains(ADDRESS_HOME_LINE1) &&
                       possible_types.contains(ADDRESS_HOME_STREET_ADDRESS)));
        } else if (possible_types.size() == 3) {
          // Or even all three.
          EXPECT_TRUE(possible_types.contains(NAME_FULL) &&
                      possible_types.contains(NAME_LAST) &&
                      (possible_types.contains(NAME_LAST_SECOND) ||
                       possible_types.contains(NAME_LAST_FIRST)));
        } else {
          EXPECT_EQ(1U, possible_types.size());
        }
        EXPECT_NE(possible_types.end(),
                  possible_types.find(test_fields[i].expected_upload_type));
      } else {
        // In the context of those tests, it is expected that the type is
        // ambiguous.
        EXPECT_NE(1U, possible_types.size());
      }
    }
  }
}

// When a field contains fields with UPI ID values, a crowdsourcing vote should
// be uploaded.
TEST_F(BrowserAutofillManagerTest, CrowdsourceUPIVPA) {
  std::vector<AutofillProfile> profiles;
  std::vector<CreditCard> credit_cards;

  FormData form;
  FormFieldData field;
  test::CreateTestFormField("", "name1", "1234@indianbank", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("", "name2", "not-upi@gmail.com", "text", &field);
  form.fields.push_back(field);
  FormStructure form_structure(form);

  BrowserAutofillManager::DeterminePossibleFieldTypesForUploadForTest(
      profiles, credit_cards, std::u16string(), "en-us", &form_structure);

  EXPECT_THAT(form_structure.field(0)->possible_types(), ElementsAre(UPI_VPA));
  EXPECT_THAT(form_structure.field(1)->possible_types(),
              Not(Contains(UPI_VPA)));
}

// If a server-side credit card is unmasked by entering the CVC, the
// BrowserAutofillManager reuses the CVC value to identify a potentially
// existing CVC form field to cast a |CREDIT_CARD_VERIFICATION_CODE|-type vote.
TEST_F(BrowserAutofillManagerTest, CrowdsourceCVCFieldByValue) {
  std::vector<AutofillProfile> profiles;
  std::vector<CreditCard> credit_cards;

  const char kCvc[] = "1234";
  const char16_t kCvc16[] = u"1234";
  const char kFourDigitButNotCvc[] = "6676";
  const char kCreditCardNumber[] = "4234-5678-9012-3456";

  FormData form;
  FormFieldData field1;
  test::CreateTestFormField("number", "number", kCreditCardNumber, "text",
                            &field1);
  form.fields.push_back(field1);

  // This field would not be detected as CVC heuristically if the CVC value
  // wouldn't be known.
  FormFieldData field2;
  test::CreateTestFormField("not_cvc", "not_cvc", kFourDigitButNotCvc, "text",
                            &field2);
  form.fields.push_back(field2);

  // This field has the CVC value used to unlock the card and should be detected
  // as the CVC field.
  FormFieldData field3;
  test::CreateTestFormField("c_v_c", "c_v_c", kCvc, "text", &field3);
  form.fields.push_back(field3);

  FormStructure form_structure(form);
  form_structure.field(0)->set_possible_types({CREDIT_CARD_NUMBER});

  BrowserAutofillManager::DeterminePossibleFieldTypesForUploadForTest(
      profiles, credit_cards, kCvc16, "en-us", &form_structure);

  CheckThatOnlyFieldByIndexHasThisPossibleType(
      form_structure, 2, CREDIT_CARD_VERIFICATION_CODE,
      FieldPropertiesFlags::kKnownValue);
}

// Expiration year field was detected by the server. The other field with a
// 4-digit value should be detected as CVC.
TEST_F(BrowserAutofillManagerTest,
       CrowdsourceCVCFieldAfterInvalidExpDateByHeuristics) {
  FormData form;
  FormFieldData field;

  const char credit_card_number[] = "4234-5678-9012-3456";
  const char actual_credit_card_exp_year[] = "2030";
  const char user_entered_credit_card_exp_year[] = "2031";
  const char cvc[] = "1234";

  test::CreateTestFormField("number", "number", credit_card_number, "text",
                            &field);
  form.fields.push_back(field);

  // Expiration date, but is not the expiration date of the used credit card.
  FormFieldData field1;
  test::CreateTestFormField("exp_year", "exp_year",
                            user_entered_credit_card_exp_year, "text", &field1);
  form.fields.push_back(field1);

  // Must be CVC since expiration date was already identified.
  FormFieldData field2;
  test::CreateTestFormField("cvc_number", "cvc_number", cvc, "text", &field2);
  form.fields.push_back(field2);

  FormStructure form_structure(form);

  // Set the field types.
  form_structure.field(0)->set_possible_types({CREDIT_CARD_NUMBER});
  form_structure.field(1)->set_possible_types({CREDIT_CARD_EXP_4_DIGIT_YEAR});
  form_structure.field(2)->set_possible_types({UNKNOWN_TYPE});

  // Set up the test credit cards.
  std::vector<CreditCard> credit_cards;
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "John Doe", credit_card_number, "04",
                          actual_credit_card_exp_year, "1");
  credit_card.set_guid("00000000-0000-0000-0000-000000000003");
  credit_cards.push_back(credit_card);

  // Set up the test profiles.
  std::vector<AutofillProfile> profiles;

  BrowserAutofillManager::DeterminePossibleFieldTypesForUploadForTest(
      profiles, credit_cards, std::u16string(), "en-us", &form_structure);

  CheckThatOnlyFieldByIndexHasThisPossibleType(form_structure, 2,
                                               CREDIT_CARD_VERIFICATION_CODE,
                                               FieldPropertiesFlags::kNoFlags);
}

// Tests if the CVC field is heuristically detected if it appears after the
// expiration year field as it was predicted by the server.
// The value in the CVC field would be a valid expiration year value.
TEST_F(BrowserAutofillManagerTest,
       CrowdsourceCVCFieldAfterExpDateByHeuristics) {
  FormData form;
  FormFieldData field;

  const char credit_card_number[] = "4234-5678-9012-3456";
  const char actual_credit_card_exp_year[] = "2030";
  const char cvc[] = "1234";

  test::CreateTestFormField("number", "number", credit_card_number, "text",
                            &field);
  form.fields.push_back(field);

  // Expiration date, that is the expiration date of the used credit card.
  FormFieldData field1;
  test::CreateTestFormField("date_or_cvc1", "date_or_cvc1",
                            actual_credit_card_exp_year, "text", &field1);
  form.fields.push_back(field1);

  // Must be CVC since expiration date was already identified.
  FormFieldData field2;
  test::CreateTestFormField("date_or_cvc2", "date_or_cvc2", cvc, "text",
                            &field2);
  form.fields.push_back(field2);

  FormStructure form_structure(form);

  // Set the field types.
  form_structure.field(0)->set_possible_types({CREDIT_CARD_NUMBER});
  form_structure.field(1)->set_possible_types({CREDIT_CARD_EXP_4_DIGIT_YEAR});
  form_structure.field(2)->set_possible_types({UNKNOWN_TYPE});

  // Set up the test credit cards.
  std::vector<CreditCard> credit_cards;
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "John Doe", credit_card_number, "04",
                          actual_credit_card_exp_year, "1");
  credit_card.set_guid("00000000-0000-0000-0000-000000000003");
  credit_cards.push_back(credit_card);

  // Set up the test profiles.
  std::vector<AutofillProfile> profiles;

  BrowserAutofillManager::DeterminePossibleFieldTypesForUploadForTest(
      profiles, credit_cards, std::u16string(), "en-us", &form_structure);

  CheckThatOnlyFieldByIndexHasThisPossibleType(form_structure, 2,
                                               CREDIT_CARD_VERIFICATION_CODE,
                                               FieldPropertiesFlags::kNoFlags);
}

// Tests if the CVC field is heuristically detected if it contains a value which
// is not a valid expiration year.
TEST_F(BrowserAutofillManagerTest,
       CrowdsourceCVCFieldBeforeExpDateByHeuristics) {
  FormData form;
  FormFieldData field;

  const char credit_card_number[] = "4234-5678-9012-3456";
  const char actual_credit_card_exp_year[] = "2030";
  const char user_entered_credit_card_exp_year[] = "2031";

  test::CreateTestFormField("number", "number", credit_card_number, "text",
                            &field);
  form.fields.push_back(field);

  // Must be CVC since it is an implausible expiration date.
  FormFieldData field2;
  test::CreateTestFormField("date_or_cvc2", "date_or_cvc2", "2130", "text",
                            &field2);
  form.fields.push_back(field2);

  // A field which is filled with a plausible expiration date which is not the
  // date of the credit card.
  FormFieldData field1;
  test::CreateTestFormField("date_or_cvc1", "date_or_cvc1",
                            user_entered_credit_card_exp_year, "text", &field1);
  form.fields.push_back(field1);

  FormStructure form_structure(form);

  // Set the field types.
  form_structure.field(0)->set_possible_types({CREDIT_CARD_NUMBER});
  form_structure.field(1)->set_possible_types({UNKNOWN_TYPE});
  form_structure.field(2)->set_possible_types({UNKNOWN_TYPE});

  // Set up the test credit cards.
  std::vector<CreditCard> credit_cards;
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "John Doe", credit_card_number, "04",
                          actual_credit_card_exp_year, "1");
  credit_card.set_guid("00000000-0000-0000-0000-000000000003");
  credit_cards.push_back(credit_card);

  // Set up the test profiles.
  std::vector<AutofillProfile> profiles;

  BrowserAutofillManager::DeterminePossibleFieldTypesForUploadForTest(
      profiles, credit_cards, std::u16string(), "en-us", &form_structure);

  CheckThatOnlyFieldByIndexHasThisPossibleType(form_structure, 1,
                                               CREDIT_CARD_VERIFICATION_CODE,
                                               FieldPropertiesFlags::kNoFlags);
}

// Tests if no CVC field is heuristically detected due to the missing of a
// credit card number field.
TEST_F(BrowserAutofillManagerTest,
       CrowdsourceNoCVCFieldDueToMissingCreditCardNumber) {
  FormData form;
  FormFieldData field;

  const char credit_card_number[] = "4234-5678-9012-3456";
  const char actual_credit_card_exp_year[] = "2030";
  const char user_entered_credit_card_exp_year[] = "2031";
  const char cvc[] = "2031";

  test::CreateTestFormField("number", "number", credit_card_number, "text",
                            &field);
  form.fields.push_back(field);

  // Server predicted as expiration year.
  FormFieldData field1;
  test::CreateTestFormField("date_or_cvc1", "date_or_cvc1",
                            user_entered_credit_card_exp_year, "text", &field1);
  form.fields.push_back(field1);

  // Must be CVC since expiration date was already identified.
  FormFieldData field2;
  test::CreateTestFormField("date_or_cvc2", "date_or_cvc2", cvc, "text",
                            &field2);
  form.fields.push_back(field2);

  FormStructure form_structure(form);

  // Set the field types.
  form_structure.field(0)->set_possible_types({UNKNOWN_TYPE});
  form_structure.field(1)->set_possible_types({CREDIT_CARD_EXP_4_DIGIT_YEAR});
  form_structure.field(2)->set_possible_types({UNKNOWN_TYPE});

  // Set up the test credit cards.
  std::vector<CreditCard> credit_cards;
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "John Doe", credit_card_number, "04",
                          actual_credit_card_exp_year, "1");
  credit_card.set_guid("00000000-0000-0000-0000-000000000003");
  credit_cards.push_back(credit_card);

  // Set up the test profiles.
  std::vector<AutofillProfile> profiles;

  BrowserAutofillManager::DeterminePossibleFieldTypesForUploadForTest(
      profiles, credit_cards, std::u16string(), "en-us", &form_structure);
  CheckThatNoFieldHasThisPossibleType(form_structure,
                                      CREDIT_CARD_VERIFICATION_CODE);
}

// Test if no CVC is found because the candidate has no valid CVC value.
TEST_F(BrowserAutofillManagerTest, CrowdsourceNoCVCDueToInvalidCandidateValue) {
  FormData form;
  FormFieldData field;

  const char credit_card_number[] = "4234-5678-9012-3456";
  const char credit_card_exp_year[] = "2030";
  const char cvc[] = "12";

  test::CreateTestFormField("number", "number", credit_card_number, "text",
                            &field);
  form.fields.push_back(field);

  // Server predicted as expiration year.
  FormFieldData field1;
  test::CreateTestFormField("date_or_cvc1", "date_or_cvc1",
                            credit_card_exp_year, "text", &field1);
  form.fields.push_back(field1);

  // Must be CVC since expiration date was already identified.
  FormFieldData field2;
  test::CreateTestFormField("date_or_cvc2", "date_or_cvc2", cvc, "text",
                            &field2);
  form.fields.push_back(field2);

  FormStructure form_structure(form);

  // Set the field types.
  form_structure.field(0)->set_possible_types(
      {CREDIT_CARD_NUMBER, UNKNOWN_TYPE});
  form_structure.field(1)->set_possible_types({CREDIT_CARD_EXP_4_DIGIT_YEAR});
  form_structure.field(2)->set_possible_types({UNKNOWN_TYPE});

  // Set up the test credit cards.
  std::vector<CreditCard> credit_cards;
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "John Doe", credit_card_number, "04",
                          credit_card_exp_year, "1");
  credit_card.set_guid("00000000-0000-0000-0000-000000000003");
  credit_cards.push_back(credit_card);

  // Set up the test profiles.
  std::vector<AutofillProfile> profiles;

  BrowserAutofillManager::DeterminePossibleFieldTypesForUploadForTest(
      profiles, credit_cards, std::u16string(), "en-us", &form_structure);

  CheckThatNoFieldHasThisPossibleType(form_structure,
                                      CREDIT_CARD_VERIFICATION_CODE);
}

TEST_F(BrowserAutofillManagerTest, RemoveProfile) {
  // Add and remove an Autofill profile.
  AutofillProfile profile;
  const char guid[] = "00000000-0000-0000-0000-000000000102";
  profile.set_guid(guid);
  personal_data().AddProfile(profile);

  int id = MakeFrontendId({.profile_id = guid});

  browser_autofill_manager_->RemoveAutofillProfileOrCreditCard(id);

  EXPECT_FALSE(personal_data().GetProfileByGUID(guid));
}

TEST_F(BrowserAutofillManagerTest, RemoveCreditCard) {
  // Add and remove an Autofill credit card.
  CreditCard credit_card;
  const char guid[] = "00000000-0000-0000-0000-000000100007";
  credit_card.set_guid(guid);
  personal_data().AddCreditCard(credit_card);

  int id = MakeFrontendId({.credit_card_id = guid});

  browser_autofill_manager_->RemoveAutofillProfileOrCreditCard(id);

  EXPECT_FALSE(personal_data().GetCreditCardByGUID(guid));
}

// Test our external delegate is called at the right time.
TEST_F(BrowserAutofillManagerTest, TestExternalDelegate) {
  FormData form;
  test::CreateTestAddressFormData(&form);
  FormsSeen({form});
  // Should call the delegate's OnQuery().
  GetAutofillSuggestions(form, form.fields[0]);

  EXPECT_TRUE(external_delegate_->on_query_seen());
}

// Test that unfocusing a filled form sends an upload with types matching the
// fields.
TEST_F(BrowserAutofillManagerTest, OnTextFieldDidChangeAndUnfocus_Upload) {
  // Set up our form data (it's already filled out with user data).
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");

  std::vector<ServerFieldTypeSet> expected_types;
  ServerFieldTypeSet types;

  FormFieldData field;
  test::CreateTestFormField("First Name", "firstname", "", "text", &field);
  form.fields.push_back(field);
  types.insert(NAME_FIRST);
  expected_types.push_back(types);

  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  form.fields.push_back(field);
  types.clear();
  types.insert(NAME_LAST);
  types.insert(NAME_LAST_SECOND);
  expected_types.push_back(types);

  test::CreateTestFormField("Email", "email", "", "text", &field);
  form.fields.push_back(field);
  types.clear();
  types.insert(EMAIL_ADDRESS);
  expected_types.push_back(types);

  FormsSeen({form});

  // We will expect these types in the upload and no observed submission (the
  // callback initiated by WaitForAsyncUploadProcess checks these expectations.)
  browser_autofill_manager_->SetExpectedSubmittedFieldTypes(expected_types);
  browser_autofill_manager_->SetExpectedObservedSubmission(false);

  // The fields are edited after calling FormsSeen on them. This is because
  // default values are not used for upload comparisons.
  form.fields[0].value = u"Elvis";
  form.fields[1].value = u"Presley";
  form.fields[2].value = u"theking@gmail.com";
  // Simulate editing a field.
  browser_autofill_manager_->OnTextFieldDidChange(
      form, form.fields.front(), gfx::RectF(), AutofillTickClock::NowTicks());

  // Simulate lost of focus on the form.
  browser_autofill_manager_->OnFocusNoLongerOnForm(true);
}

// Test that navigating with a filled form sends an upload with types matching
// the fields.
TEST_F(BrowserAutofillManagerTest, OnTextFieldDidChangeAndNavigation_Upload) {
  // Set up our form data (it's already filled out with user data).
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");

  std::vector<ServerFieldTypeSet> expected_types;
  ServerFieldTypeSet types;

  FormFieldData field;
  test::CreateTestFormField("First Name", "firstname", "", "text", &field);
  form.fields.push_back(field);
  types.insert(NAME_FIRST);
  expected_types.push_back(types);

  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  form.fields.push_back(field);
  types.clear();
  types.insert(NAME_LAST_SECOND);
  types.insert(NAME_LAST);
  expected_types.push_back(types);

  test::CreateTestFormField("Email", "email", "", "text", &field);
  form.fields.push_back(field);
  types.clear();
  types.insert(EMAIL_ADDRESS);
  expected_types.push_back(types);

  FormsSeen({form});

  // We will expect these types in the upload and no observed submission. (the
  // callback initiated by WaitForAsyncUploadProcess checks these expectations.)
  browser_autofill_manager_->SetExpectedSubmittedFieldTypes(expected_types);
  browser_autofill_manager_->SetExpectedObservedSubmission(false);

  // The fields are edited after calling FormsSeen on them. This is because
  // default values are not used for upload comparisons.
  form.fields[0].value = u"Elvis";
  form.fields[1].value = u"Presley";
  form.fields[2].value = u"theking@gmail.com";
  // Simulate editing a field.
  browser_autofill_manager_->OnTextFieldDidChange(
      form, form.fields.front(), gfx::RectF(), AutofillTickClock::NowTicks());

  // Simulate a navigation so that the pending form is uploaded.
  browser_autofill_manager_->Reset();
}

// Test that unfocusing a filled form sends an upload with types matching the
// fields.
TEST_F(BrowserAutofillManagerTest, OnDidFillAutofillFormDataAndUnfocus_Upload) {
  // Set up our form data (empty).
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");

  std::vector<ServerFieldTypeSet> expected_types;

  // These fields should all match.
  ServerFieldTypeSet types;
  FormFieldData field;
  test::CreateTestFormField("First Name", "firstname", "", "text", &field);
  form.fields.push_back(field);
  types.insert(NAME_FIRST);
  expected_types.push_back(types);

  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  form.fields.push_back(field);
  types.clear();
  types.insert(NAME_LAST_SECOND);
  types.insert(NAME_LAST);
  expected_types.push_back(types);

  test::CreateTestFormField("Email", "email", "", "text", &field);
  form.fields.push_back(field);
  types.clear();
  types.insert(EMAIL_ADDRESS);
  expected_types.push_back(types);

  FormsSeen({form});

  // We will expect these types in the upload and no observed submission. (the
  // callback initiated by WaitForAsyncUploadProcess checks these expectations.)
  browser_autofill_manager_->SetExpectedSubmittedFieldTypes(expected_types);
  browser_autofill_manager_->SetExpectedObservedSubmission(false);

  // Form was autofilled with user data.
  form.fields[0].value = u"Elvis";
  form.fields[1].value = u"Presley";
  form.fields[2].value = u"theking@gmail.com";
  browser_autofill_manager_->OnDidFillAutofillFormData(
      form, AutofillTickClock::NowTicks());

  // Simulate lost of focus on the form.
  browser_autofill_manager_->OnFocusNoLongerOnForm(true);
}

// Test that suggestions are returned for credit card fields with an
// unrecognized
// autocomplete attribute.
TEST_F(BrowserAutofillManagerTest,
       GetCreditCardSuggestions_UnrecognizedAttribute) {
  // Set up the form data.
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");

  FormFieldData field;
  // Set a valid autocomplete attribute on the card name.
  test::CreateTestFormField("Name on Card", "nameoncard", "", "text", "cc-name",
                            &field);
  form.fields.push_back(field);
  // Set no autocomplete attribute on the card number.
  test::CreateTestFormField("Card Number", "cardnumber", "", "text", "",
                            &field);
  form.fields.push_back(field);
  // Set an unrecognized autocomplete attribute on the expiration month.
  test::CreateTestFormField("Expiration Date", "ccmonth", "", "text",
                            "unrecognized", &field);
  form.fields.push_back(field);
  FormsSeen({form});

  // Suggestions should be returned for the first two fields
  GetAutofillSuggestions(form, form.fields[0]);
  EXPECT_TRUE(external_delegate_->on_suggestions_returned_seen());
  GetAutofillSuggestions(form, form.fields[1]);
  EXPECT_TRUE(external_delegate_->on_suggestions_returned_seen());

  // Suggestions should still be returned for the third field because it is a
  // credit card field.
  GetAutofillSuggestions(form, form.fields[2]);
  EXPECT_TRUE(external_delegate_->on_suggestions_returned_seen());
}

// Test to verify suggestions appears for forms having credit card number split
// across fields.
TEST_F(BrowserAutofillManagerTest,
       GetCreditCardSuggestions_ForNumberSplitAcrossFields) {
  // Set up our form data with credit card number split across fields.
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");

  FormFieldData name_field;
  test::CreateTestFormField("Name on Card", "nameoncard", "", "text",
                            &name_field);
  form.fields.push_back(name_field);

  // Add new 4 |card_number_field|s to the |form|.
  FormFieldData card_number_field;
  card_number_field.max_length = 4;
  test::CreateTestFormField("Card Number", "cardnumber_1", "", "text",
                            &card_number_field);
  form.fields.push_back(card_number_field);

  test::CreateTestFormField("", "cardnumber_2", "", "text", &card_number_field);
  form.fields.push_back(card_number_field);

  test::CreateTestFormField("", "cardnumber_3", "", "text", &card_number_field);
  form.fields.push_back(card_number_field);

  test::CreateTestFormField("", "cardnumber_4", "", "text", &card_number_field);
  form.fields.push_back(card_number_field);

  FormFieldData exp_field;
  test::CreateTestFormField("Expiration Date", "ccmonth", "", "text",
                            &exp_field);
  form.fields.push_back(exp_field);

  test::CreateTestFormField("", "ccyear", "", "text", &exp_field);
  form.fields.push_back(exp_field);

  FormsSeen({form});

  // Verify whether suggestions are populated correctly for one of the middle
  // credit card number fields when filled partially.
  FormFieldData number_field = form.fields[3];
  number_field.value = u"901";

  // Get the suggestions for already filled credit card |number_field|.
  GetAutofillSuggestions(form, number_field);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  const std::string visa_label = std::string("04/99");
  const std::string master_card_label = std::string("10/98");
#else
  const std::string visa_label = std::string("Expires on 04/99");
  const std::string master_card_label = std::string("Expires on 10/98");
#endif

  CheckSuggestions(
      form.fields[3].global_id(),
      Suggestion(
          std::string("Visa  ") + test::ObfuscatedCardDigitsAsUTF8("3456"),
          visa_label, kVisaCard,
          browser_autofill_manager_->GetPackedCreditCardID(4)));
}

// Test that inputs detected to be CVC inputs are forced to
// !should_autocomplete for SingleFieldFormFillRouter::OnWillSubmitForm.
TEST_F(BrowserAutofillManagerTest, DontSaveCvcInAutocompleteHistory) {
  FormData form_seen_by_ahm;
  EXPECT_CALL(*single_field_form_fill_router_, OnWillSubmitForm(_, _, true))
      .WillOnce(SaveArg<0>(&form_seen_by_ahm));

  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");

  struct {
    const char* label;
    const char* name;
    const char* value;
    ServerFieldType expected_field_type;
  } test_fields[] = {
      {"Card number", "1", "4234-5678-9012-3456", CREDIT_CARD_NUMBER},
      {"Card verification code", "2", "123", CREDIT_CARD_VERIFICATION_CODE},
      {"expiration date", "3", "04/2020", CREDIT_CARD_EXP_4_DIGIT_YEAR},
  };

  for (const auto& test_field : test_fields) {
    FormFieldData field;
    test::CreateTestFormField(test_field.label, test_field.name,
                              test_field.value, "text", &field);
    form.fields.push_back(field);
  }

  FormsSeen({form});
  FormSubmitted(form);

  EXPECT_EQ(form.fields.size(), form_seen_by_ahm.fields.size());
  ASSERT_EQ(std::size(test_fields), form_seen_by_ahm.fields.size());
  for (size_t i = 0; i < std::size(test_fields); ++i) {
    EXPECT_EQ(
        form_seen_by_ahm.fields[i].should_autocomplete,
        test_fields[i].expected_field_type != CREDIT_CARD_VERIFICATION_CODE);
  }
}

TEST_F(BrowserAutofillManagerTest, DontOfferToSavePaymentsCard) {
  FormData form;
  CreditCard card;
  PrepareForRealPanResponse(&form, &card);

  // Manually fill out |form| so we can use it in OnFormSubmitted.
  for (auto& field : form.fields) {
    if (field.name == u"cardnumber")
      field.value = u"4012888888881881";
    else if (field.name == u"nameoncard")
      field.value = u"John H Dillinger";
    else if (field.name == u"ccmonth")
      field.value = u"01";
    else if (field.name == u"ccyear")
      field.value = u"2017";
  }

  CardUnmaskDelegate::UserProvidedUnmaskDetails details;
  details.cvc = u"123";
  full_card_unmask_delegate()->OnUnmaskPromptAccepted(details);
  OnDidGetRealPan(AutofillClient::PaymentsRpcResult::kSuccess,
                  "4012888888881881");
  browser_autofill_manager_->OnFormSubmitted(form, false,
                                             SubmissionSource::FORM_SUBMISSION);
}

TEST_F(BrowserAutofillManagerTest, FillInUpdatedExpirationDate) {
  FormData form;
  CreditCard card;
  PrepareForRealPanResponse(&form, &card);

  CardUnmaskDelegate::UserProvidedUnmaskDetails details;
  details.cvc = u"123";
  details.exp_month = u"02";
  details.exp_year = u"2018";
  full_card_unmask_delegate()->OnUnmaskPromptAccepted(details);
  OnDidGetRealPan(AutofillClient::PaymentsRpcResult::kSuccess,
                  "4012888888881881");
}

TEST_F(BrowserAutofillManagerTest, ProfileDisabledDoesNotFillFormData) {
  browser_autofill_manager_->SetAutofillProfileEnabled(autofill_client_, false);

  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  FormsSeen({form});

  const char guid[] = "00000000-0000-0000-0000-000000000001";

  // Expect no fields filled, no form data sent to renderer.
  EXPECT_CALL(*autofill_driver_, FillOrPreviewForm(_, _, _, _)).Times(0);

  FillAutofillFormData(form, *form.fields.begin(),
                       MakeFrontendId({.profile_id = guid}));
}

TEST_F(BrowserAutofillManagerTest, ProfileDisabledDoesNotSuggest) {
  browser_autofill_manager_->SetAutofillProfileEnabled(autofill_client_, false);

  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  FormsSeen({form});

  FormFieldData field;
  test::CreateTestFormField("Email", "email", "", "email", &field);
  GetAutofillSuggestions(form, field);
  // Expect no suggestions as autofill and autocomplete are disabled for
  // addresses.
  EXPECT_FALSE(external_delegate_->on_suggestions_returned_seen());
}

TEST_F(BrowserAutofillManagerTest, CreditCardDisabledDoesNotFillFormData) {
  browser_autofill_manager_->SetAutofillCreditCardEnabled(autofill_client_,
                                                          false);

  // Set up our form data.
  FormData form = CreateTestCreditCardFormData(true, false);
  FormsSeen({form});

  const char guid[] = "00000000-0000-0000-0000-000000000004";

  // Expect no fields filled, no form data sent to renderer.
  EXPECT_CALL(*autofill_driver_, FillOrPreviewForm(_, _, _, _)).Times(0);

  FillAutofillFormData(form, *form.fields.begin(),
                       MakeFrontendId({.credit_card_id = guid}));
}

TEST_F(BrowserAutofillManagerTest, CreditCardDisabledDoesNotSuggest) {
  browser_autofill_manager_->SetAutofillCreditCardEnabled(autofill_client_,
                                                          false);

  // Set up our form data.
  FormData form = CreateTestCreditCardFormData(true, false);
  FormsSeen({form});

  FormFieldData field;
  test::CreateTestFormField("Name on Card", "nameoncard", "", "text", &field);
  GetAutofillSuggestions(form, field);
  // Expect no suggestions as autofill and autocomplete are disabled for credit
  // cards.
  EXPECT_FALSE(external_delegate_->on_suggestions_returned_seen());
}

// Verify that typing "gmail" matches "theking@gmail.com" and "buddy@gmail.com"
// when substring matching is enabled.
TEST_P(SuggestionMatchingTest, DisplaySuggestionsWithMatchingTokens) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kAutofillTokenPrefixMatching);

  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  FormsSeen({form});

  FormFieldData field;
  test::CreateTestFormField("Email", "email", "gmail", "email", &field);
  GetAutofillSuggestions(form, field);

  std::string label1;
  std::string label2;

  switch (enabled_feature_) {
      // 23456789012 is not formatted because it is invalid for the app locale.
      // It has an extra digit.
    case EnabledFeature::kDesktop:
      label1 =
          MakeLabel({"Charles Holley", "123 Apple St., unit 6", "23456789012"});
      label2 = MakeLabel({"Elvis Presley", "3734 Elvis Presley Blvd., Apt. 10",
                          "(234) 567-8901"});
      break;
    case EnabledFeature::kMobileShowAll:
      // 23456789012 is not formatted because it is invalid for the app locale.
      // It
      // has an extra digit.
      label1 =
          MakeMobileLabel({"Charles", "123 Apple St., unit 6", "23456789012"});
      label2 = MakeMobileLabel(
          {"Elvis", "3734 Elvis Presley Blvd., Apt. 10", "(234) 567-8901"});
      break;
    case EnabledFeature::kMobileShowOne:
      label1 = "123 Apple St., unit 6";
      label2 = "3734 Elvis Presley Blvd., Apt. 10";
      break;
    case EnabledFeature::kNone:
      label1 = "123 Apple St.";
      label2 = "3734 Elvis Presley Blvd.";
  }
  // Test that we sent the right values to the external delegate.
  CheckSuggestions(
      field.global_id(),
      Suggestion("buddy@gmail.com", label1, kAddressEntryIcon, 1),
      Suggestion("theking@gmail.com", label2, kAddressEntryIcon, 2));
}

// Verify that typing "apple" will match "123 Apple St." when substring matching
// is enabled.
TEST_P(SuggestionMatchingTest,
       DisplaySuggestionsWithMatchingTokens_CaseIgnored) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kAutofillTokenPrefixMatching);

  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  FormsSeen({form});

  FormFieldData field;
  test::CreateTestFormField("Address Line 2", "addr2", "apple", "text", &field);
  GetAutofillSuggestions(form, field);

  std::string label;

  switch (enabled_feature_) {
    case EnabledFeature::kDesktop:
      label = "Charles Holley";
      break;
    case EnabledFeature::kMobileShowAll:
    case EnabledFeature::kMobileShowOne:
      // 23456789012 is not formatted because it is invalid for the app locale.
      // It has an extra digit.
      label = "23456789012";
      break;
    case EnabledFeature::kNone:
      label = "123 Apple St.";
  }
  // Test that we sent the right values to the external delegate.
  CheckSuggestions(field.global_id(), Suggestion("123 Apple St., unit 6", label,
                                                 kAddressEntryIcon, 1));
}

// Verify that typing "mail" will not match any of the "@gmail.com" email
// addresses when substring matching is enabled.
TEST_F(BrowserAutofillManagerTest, NoSuggestionForNonPrefixTokenMatch) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kAutofillTokenPrefixMatching);

  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  FormsSeen({form});

  FormFieldData field;
  test::CreateTestFormField("Email", "email", "mail", "email", &field);
  GetAutofillSuggestions(form, field);
  EXPECT_FALSE(external_delegate_->on_suggestions_returned_seen());
}

// Verify that typing "dre" matches "Nancy Drew" when substring matching is
// enabled.
TEST_P(CreditCardSuggestionTest,
       DisplayCreditCardSuggestionsWithMatchingTokens) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kAutofillTokenPrefixMatching);

  // Set up our form data.
  FormData form = CreateTestCreditCardFormData(true, false);
  FormsSeen({form});

  FormFieldData field;
  test::CreateTestFormField("Name on Card", "nameoncard", "dre", "text",
                            &field);

  const char guid[] = "00000000-0000-0000-0000-000000000030";
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "Nancy Drew",
                          "4444555566667777",  // Visa
                          "01", "2030", "1");
  credit_card.set_guid(guid);
  credit_card.SetNickname(kArbitraryNickname16);
  personal_data().AddCreditCard(credit_card);

#if BUILDFLAG(IS_ANDROID)
  // When keyboard accessary is enabled, always show "7777".
  // When keyboard accessary is disabled, if nickname is valid, show "Nickname
  // ****7777", otherwise, show "Visa  ****7777".
  const std::string visa_label =
      IsKeyboardAccessoryEnabled()
          ? test::ObfuscatedCardDigitsAsUTF8("7777", ObfuscationLength())
          : kArbitraryNickname + "  " +
                test::ObfuscatedCardDigitsAsUTF8("7777", ObfuscationLength());

#elif BUILDFLAG(IS_IOS)
  const std::string visa_label =
      test::ObfuscatedCardDigitsAsUTF8("7777", ObfuscationLength());

#else
  const std::string visa_label = base::JoinString(
      {kArbitraryNickname + "  ",
       test::ObfuscatedCardDigitsAsUTF8("7777", ObfuscationLength()),
       ", expires on 01/30"},
      "");
#endif

  GetAutofillSuggestions(form, field);
  CheckSuggestions(field.global_id(),
                   Suggestion("Nancy Drew", visa_label, kVisaCard,
                              MakeFrontendId({.credit_card_id = guid})));
}

// Verify that typing "lvis" will not match any of the credit card name when
// substring matching is enabled.
TEST_F(BrowserAutofillManagerTest,
       NoCreditCardSuggestionsForNonPrefixTokenMatch) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kAutofillTokenPrefixMatching);

  // Set up our form data.
  FormData form = CreateTestCreditCardFormData(true, false);
  FormsSeen({form});

  FormFieldData field;
  test::CreateTestFormField("Name on Card", "nameoncard", "lvis", "text",
                            &field);
  GetAutofillSuggestions(form, field);
  // Autocomplete suggestions are queried, but not Autofill.
  EXPECT_FALSE(external_delegate_->on_suggestions_returned_seen());
}

TEST_F(BrowserAutofillManagerTest, GetPopupType_CreditCardForm) {
  // Set up our form data.
  FormData form = CreateTestCreditCardFormData(true, false);
  FormsSeen({form});

  for (const FormFieldData& field : form.fields) {
    EXPECT_EQ(PopupType::kCreditCards,
              browser_autofill_manager_->GetPopupType(form, field));
  }
}

TEST_F(BrowserAutofillManagerTest, GetPopupType_AddressForm) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  FormsSeen({form});

  for (const FormFieldData& field : form.fields) {
    EXPECT_EQ(PopupType::kAddresses,
              browser_autofill_manager_->GetPopupType(form, field));
  }
}

TEST_F(BrowserAutofillManagerTest, GetPopupType_PersonalInformationForm) {
  // Set up our form data.
  FormData form;
  test::CreateTestPersonalInformationFormData(&form);
  FormsSeen({form});

  for (const FormFieldData& field : form.fields) {
    EXPECT_EQ(PopupType::kPersonalInformation,
              browser_autofill_manager_->GetPopupType(form, field));
  }
}

// Verify that typing "S" into the middle name field will match and order middle
// names "Shawn Smith" followed by "Adam Smith" i.e. prefix matched followed by
// substring matched.
TEST_P(SuggestionMatchingTest,
       DisplaySuggestionsWithPrefixesPrecedeSubstringMatched) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kAutofillTokenPrefixMatching);

  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  FormsSeen({form});

  AutofillProfile profile1;
  profile1.set_guid("00000000-0000-0000-0000-000000000103");
  profile1.SetInfo(NAME_FIRST, u"Robin", "en-US");
  profile1.SetInfo(NAME_MIDDLE, u"Adam Smith", "en-US");
  profile1.SetInfo(NAME_LAST, u"Grimes", "en-US");
  profile1.SetInfo(ADDRESS_HOME_LINE1, u"1234 Smith Blvd.", "en-US");
  personal_data().AddProfile(profile1);

  AutofillProfile profile2;
  profile2.set_guid("00000000-0000-0000-0000-000000000124");
  profile2.SetInfo(NAME_FIRST, u"Carl", "en-US");
  profile2.SetInfo(NAME_MIDDLE, u"Shawn Smith", "en-US");
  profile2.SetInfo(NAME_LAST, u"Grimes", "en-US");
  profile2.SetInfo(ADDRESS_HOME_LINE1, u"1234 Smith Blvd.", "en-US");
  personal_data().AddProfile(profile2);

  FormFieldData field;
  test::CreateTestFormField("Middle Name", "middlename", "S", "text", &field);
  GetAutofillSuggestions(form, field);

  std::string label1;
  std::string label2;

  switch (enabled_feature_) {
    case EnabledFeature::kDesktop:
    case EnabledFeature::kMobileShowAll:
    case EnabledFeature::kMobileShowOne:
      label1 = "1234 Smith Blvd.";
      label2 = "1234 Smith Blvd.";
      break;
    case EnabledFeature::kNone:
      label1 = "1234 Smith Blvd., Carl Shawn Smith Grimes";
      label2 = "1234 Smith Blvd., Robin Adam Smith Grimes";
  }
  CheckSuggestions(field.global_id(),
                   Suggestion("Shawn Smith", label1, kAddressEntryIcon, 1),
                   Suggestion("Adam Smith", label2, kAddressEntryIcon, 2));
}

TEST_F(BrowserAutofillManagerTest, ShouldUploadForm) {
  // Note: The enforcement of a minimum number of required fields for upload
  // is disabled by default. This tests validates both the disabled and enabled
  // scenarios.
  FormData form;
  form.name = u"TestForm";
  form.url = GURL("https://example.com/form.html");
  form.action = GURL("https://example.com/submit.html");

  // Empty Form.
  EXPECT_FALSE(
      browser_autofill_manager_->ShouldUploadForm(FormStructure(form)));

  // Add a field to the form.
  FormFieldData field;
  test::CreateTestFormField("Name", "name", "", "text", &field);
  form.fields.push_back(field);

  EXPECT_TRUE(browser_autofill_manager_->ShouldUploadForm(FormStructure(form)));

  // Add a second field to the form.
  test::CreateTestFormField("Email", "email", "", "text", &field);
  form.fields.push_back(field);

  EXPECT_TRUE(browser_autofill_manager_->ShouldUploadForm(FormStructure(form)));

  // Has less than 3 fields but has autocomplete attribute.
  const char* autocomplete = "given-name";
  form.fields[0].autocomplete_attribute = autocomplete;
  form.fields[0].parsed_autocomplete =
      ParseAutocompleteAttribute(autocomplete, form.fields[0].max_length);

  EXPECT_TRUE(browser_autofill_manager_->ShouldUploadForm(FormStructure(form)));

  // Has more than 3 fields, no autocomplete attribute.
  test::CreateTestFormField("Country", "country", "", "text", "", &field);
  form.fields.push_back(field);
  FormStructure form_structure_3(form);
  EXPECT_TRUE(browser_autofill_manager_->ShouldUploadForm(FormStructure(form)));

  // Has more than 3 fields and at least one autocomplete attribute.
  form.fields[0].autocomplete_attribute = autocomplete;
  form.fields[0].parsed_autocomplete =
      ParseAutocompleteAttribute(autocomplete, form.fields[0].max_length);
  EXPECT_TRUE(browser_autofill_manager_->ShouldUploadForm(FormStructure(form)));

  // Is off the record.
  autofill_client_.set_is_off_the_record(true);
  EXPECT_FALSE(
      browser_autofill_manager_->ShouldUploadForm(FormStructure(form)));

  // Make sure it's reset for the next test case.
  autofill_client_.set_is_off_the_record(false);
  EXPECT_TRUE(browser_autofill_manager_->ShouldUploadForm(FormStructure(form)));

  // Has one field which is appears to be a password field.
  form.fields.clear();
  test::CreateTestFormField("Password", "password", "", "password", &field);
  form.fields.push_back(field);

  // With min required fields disabled.
  EXPECT_TRUE(browser_autofill_manager_->ShouldUploadForm(FormStructure(form)));

  // Autofill disabled.
  browser_autofill_manager_->SetAutofillProfileEnabled(autofill_client_, false);
  browser_autofill_manager_->SetAutofillCreditCardEnabled(autofill_client_,
                                                          false);
  EXPECT_FALSE(
      browser_autofill_manager_->ShouldUploadForm(FormStructure(form)));
}

// Verify that no suggestions are shown on desktop for non credit card related
// fields if the initiating field has the "autocomplete" attribute set to off.
TEST_F(BrowserAutofillManagerTest,
       DisplaySuggestions_AutocompleteOffNotRespected_AddressField) {
  // Set up an address form.
  FormData mixed_form;
  mixed_form.name = u"MyForm";
  mixed_form.url = GURL("https://myform.com/form.html");
  mixed_form.action = GURL("https://myform.com/submit.html");
  FormFieldData field;
  test::CreateTestFormField("First name", "firstname", "", "text", &field);
  field.should_autocomplete = false;
  mixed_form.fields.push_back(field);
  test::CreateTestFormField("Last name", "lastname", "", "text", &field);
  field.should_autocomplete = true;
  mixed_form.fields.push_back(field);
  test::CreateTestFormField("Address", "address", "", "text", &field);
  field.should_autocomplete = true;
  mixed_form.fields.push_back(field);
  FormsSeen({mixed_form});

  // Suggestions should be displayed on desktop for this field in all
  // circumstances.
  GetAutofillSuggestions(mixed_form, mixed_form.fields[0]);
  EXPECT_TRUE(external_delegate_->on_suggestions_returned_seen());

  // Suggestions should always be displayed for all the other fields.
  for (size_t i = 1U; i < mixed_form.fields.size(); ++i) {
    GetAutofillSuggestions(mixed_form, mixed_form.fields[i]);
    EXPECT_TRUE(external_delegate_->on_suggestions_returned_seen());
  }
}

// Verify that suggestions are shown on desktop for credit card related fields
// even if the initiating field has the "autocomplete" attribute set to off.
TEST_F(BrowserAutofillManagerTest,
       DisplaySuggestions_AutocompleteOff_CreditCardField) {
  // Set up a credit card form.
  FormData mixed_form;
  mixed_form.name = u"MyForm";
  mixed_form.url = GURL("https://myform.com/form.html");
  mixed_form.action = GURL("https://myform.com/submit.html");
  FormFieldData field;
  test::CreateTestFormField("Name on Card", "nameoncard", "", "text", &field);
  field.should_autocomplete = false;
  mixed_form.fields.push_back(field);
  test::CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
  field.should_autocomplete = true;
  mixed_form.fields.push_back(field);
  test::CreateTestFormField("Expiration Month", "ccexpiresmonth", "", "text",
                            &field);
  field.should_autocomplete = false;
  mixed_form.fields.push_back(field);
  mixed_form.fields.push_back(field);
  FormsSeen({mixed_form});

  // Suggestions should always be displayed.
  for (const FormFieldData& mixed_form_field : mixed_form.fields) {
    // Single field form fill suggestions being returned are directly correlated
    // to whether or not the field has autocomplete set to true or false. We
    // know autocomplete must be the single field form filler in this case due
    // to the field not having a type that would route to any of the other
    // single field form fillers.
    ON_CALL(*single_field_form_fill_router_, OnGetSingleFieldSuggestions)
        .WillByDefault(testing::Return(mixed_form_field.should_autocomplete));
    GetAutofillSuggestions(mixed_form, mixed_form_field);

    EXPECT_TRUE(external_delegate_->on_suggestions_returned_seen());
  }
}

// Tests that a form with server only types is still autofillable if the form
// gets updated in cache.
TEST_F(BrowserAutofillManagerTest,
       DisplaySuggestionsForUpdatedServerTypedForm) {
  // Create a form with unknown heuristic fields.
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");

  FormFieldData field;
  test::CreateTestFormField("Field 1", "field1", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Field 2", "field2", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Field 3", "field3", "", "text", &field);
  form.fields.push_back(field);

  auto form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes(nullptr, nullptr);
  // Make sure the form can not be autofilled now.
  ASSERT_EQ(0u, form_structure->autofill_count());
  for (size_t idx = 0; idx < form_structure->field_count(); ++idx) {
    ASSERT_EQ(UNKNOWN_TYPE, form_structure->field(idx)->heuristic_type());
  }

  // Prepare and set known server fields.
  const std::vector<ServerFieldType> heuristic_types(form.fields.size(),
                                                     UNKNOWN_TYPE);
  const std::vector<ServerFieldType> server_types{NAME_FIRST, NAME_MIDDLE,
                                                  NAME_LAST};
  FormStructureTestApi(form_structure.get())
      .SetFieldTypes(heuristic_types, server_types);
  browser_autofill_manager_->AddSeenFormStructure(std::move(form_structure));

  // Make sure the form can be autofilled.
  for (const FormFieldData& form_field : form.fields) {
    GetAutofillSuggestions(form, form_field);
    ASSERT_TRUE(external_delegate_->on_suggestions_returned_seen());
  }

  // Modify one of the fields in the original form.
  form.fields[0].css_classes += u"a";

  // Expect the form still can be autofilled.
  for (const FormFieldData& form_field : form.fields) {
    GetAutofillSuggestions(form, form_field);
    EXPECT_TRUE(external_delegate_->on_suggestions_returned_seen());
  }

  // Modify form action URL. This can happen on in-page navitaion if the form
  // doesn't have an actual action (attribute is empty).
  form.action = net::AppendQueryParameter(form.action, "arg", "value");

  // Expect the form still can be autofilled.
  for (const FormFieldData& form_field : form.fields) {
    GetAutofillSuggestions(form, form_field);
    EXPECT_TRUE(external_delegate_->on_suggestions_returned_seen());
  }
}

// Test that is_all_server_suggestions is true if there are only
// full_server_card and masked_server_card on file.
TEST_F(BrowserAutofillManagerTest,
       GetCreditCardSuggestions_IsAllServerSuggestionsTrue) {
  // Create server credit cards.
  CreateTestServerCreditCards();

  // Set up our form data.
  FormData form = CreateTestCreditCardFormData(true, false);
  FormsSeen({form});

  GetAutofillSuggestions(form, form.fields[1]);

  // Test that we sent the right values to the external delegate.
  ASSERT_TRUE(external_delegate_->is_all_server_suggestions());
}

// Test that is_all_server_suggestions is false if there is at least one
// local_card on file.
TEST_F(BrowserAutofillManagerTest,
       GetCreditCardSuggestions_IsAllServerSuggestionsFalse) {
  // Create server and local credit cards.
  CreateTestServerAndLocalCreditCards();

  // Set up our form data.
  FormData form = CreateTestCreditCardFormData(true, false);
  FormsSeen({form});

  GetAutofillSuggestions(form, form.fields[1]);

  // Test that we sent the right values to the external delegate.
  ASSERT_FALSE(external_delegate_->is_all_server_suggestions());
}

TEST_F(BrowserAutofillManagerTest, GetCreditCardSuggestions_VirtualCard) {
  personal_data().ClearCreditCards();
  CreditCard masked_server_card(CreditCard::MASKED_SERVER_CARD,
                                /*server_id=*/"a123");
  test::SetCreditCardInfo(&masked_server_card, "Elvis Presley",
                          "4234567890123456",  // Visa
                          "04", "2999", "1");
  masked_server_card.SetNetworkForMaskedCard(kVisaCard);
  masked_server_card.set_guid("00000000-0000-0000-0000-000000000007");
  masked_server_card.set_virtual_card_enrollment_state(CreditCard::ENROLLED);
  masked_server_card.SetNickname(u"nickname");
  personal_data().AddServerCreditCard(masked_server_card);

  // Set up our form data.
  FormData form = CreateTestCreditCardFormData(true, false);
  FormsSeen({form});

  // Card number field.
  GetAutofillSuggestions(form, form.fields[1]);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  std::string label = std::string("04/99");
#else
  std::string label = std::string("Expires on 04/99");
#endif

  Suggestion virtual_card_suggestion = Suggestion(
      "Virtual card",
      std::string("nickname  ") + test::ObfuscatedCardDigitsAsUTF8("3456"),
      label, kVisaCard, autofill::POPUP_ITEM_ID_VIRTUAL_CREDIT_CARD_ENTRY);

  CheckSuggestions(
      form.fields[1].global_id(), virtual_card_suggestion,
      Suggestion(
          std::string("nickname  ") + test::ObfuscatedCardDigitsAsUTF8("3456"),
          label, kVisaCard,
          browser_autofill_manager_->GetPackedCreditCardID(7)));

  // Non card number field (cardholder name field).
  GetAutofillSuggestions(form, form.fields[0]);

#if BUILDFLAG(IS_ANDROID)
  label = std::string("nickname  ") + test::ObfuscatedCardDigitsAsUTF8("3456");
#elif BUILDFLAG(IS_IOS)
  label = test::ObfuscatedCardDigitsAsUTF8("3456");
#else
  label = std::string("nickname  ") + test::ObfuscatedCardDigitsAsUTF8("3456") +
          std::string(", expires on 04/99");
#endif

  virtual_card_suggestion =
      Suggestion("Virtual card", std::string("Elvis Presley"), label, kVisaCard,
                 autofill::POPUP_ITEM_ID_VIRTUAL_CREDIT_CARD_ENTRY);

  CheckSuggestions(
      form.fields[0].global_id(), virtual_card_suggestion,
      Suggestion("Elvis Presley", label, kVisaCard,
                 browser_autofill_manager_->GetPackedCreditCardID(7)));

  // Incomplete form.
  GetAutofillSuggestions(form, form.fields[0]);

  CheckSuggestions(
      form.fields[0].global_id(), virtual_card_suggestion,
      Suggestion("Elvis Presley", label, kVisaCard,
                 browser_autofill_manager_->GetPackedCreditCardID(7)));
}

TEST_F(BrowserAutofillManagerTest,
       IbanFormProcessed_AutofillOptimizationGuidePresent) {
  FormData form_data;
  test::CreateTestIbanFormData(&form_data);
  FormStructure form_structure{form_data};
  FormStructureTestApi(&form_structure)
      .SetFieldTypes({IBAN_VALUE}, {IBAN_VALUE});

  MockAutofillOptimizationGuide autofill_optimization_guide;
  ON_CALL(autofill_client_, GetAutofillOptimizationGuide)
      .WillByDefault(testing::Return(&autofill_optimization_guide));

  // We reset `browser_autofill_manager_` here so that `autofill_client_`
  // initializes `autofill_optimization_guide` in `browser_autofill_manager_`.
  browser_autofill_manager_ = std::make_unique<TestBrowserAutofillManager>(
      autofill_driver_.get(), &autofill_client_);
  EXPECT_CALL(autofill_optimization_guide, OnDidParseForm).Times(1);

  browser_autofill_manager_->OnFormProcessedForTesting(form_data,
                                                       form_structure);
}

TEST_F(BrowserAutofillManagerTest,
       IbanFormProcessed_AutofillOptimizationGuideNotPresent) {
  FormData form_data;
  test::CreateTestIbanFormData(&form_data);
  FormStructure form_structure{form_data};
  FormStructureTestApi(&form_structure)
      .SetFieldTypes({IBAN_VALUE}, {IBAN_VALUE});

  // Test that form processing doesn't crash when we have an IBAN form but no
  // AutofillOptimizationGuide present.
  browser_autofill_manager_->OnFormProcessedForTesting(form_data,
                                                       form_structure);
}

TEST_F(BrowserAutofillManagerTest,
       DidShowSuggestions_LogAutocompleteShownMetric) {
  FormData form;
  form.name = u"NothingSpecial";

  FormFieldData field;
  test::CreateTestFormField("Something", "something", "", "text", &field);
  form.fields.push_back(field);
  FormsSeen({form});

  base::HistogramTester histogram_tester;
  browser_autofill_manager_->DidShowSuggestions(
      /*has_autofill_suggestions=*/false, form, field);
  histogram_tester.ExpectBucketCount(
      "Autocomplete.Events", AutofillMetrics::AUTOCOMPLETE_SUGGESTIONS_SHOWN,
      1);

  // No Autofill logs.
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(histograms,
              Not(AnyOf(HasSubstr("Autofill.UserHappiness"),
                        HasSubstr("Autofill.FormEvents.Address"),
                        HasSubstr("Autofill.FormEvents.CreditCard"))));
}

TEST_F(BrowserAutofillManagerTest,
       DidShowSuggestions_LogAutofillAddressShownMetric) {
  FormData form;
  test::CreateTestAddressFormData(&form);
  FormsSeen({form});

  base::HistogramTester histogram_tester;
  browser_autofill_manager_->DidShowSuggestions(
      /*has_autofill_suggestions=*/true, form, form.fields[0]);
  histogram_tester.ExpectBucketCount("Autofill.UserHappiness",
                                     AutofillMetrics::SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount("Autofill.UserHappiness.Address",
                                     AutofillMetrics::SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.UserHappiness", AutofillMetrics::SUGGESTIONS_SHOWN_ONCE, 1);
  histogram_tester.ExpectBucketCount("Autofill.UserHappiness.Address",
                                     AutofillMetrics::SUGGESTIONS_SHOWN_ONCE,
                                     1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);

  // No Autocomplete or credit cards logs.
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(histograms,
              Not(AnyOf(HasSubstr("Autocomplete.Events"),
                        HasSubstr("Autofill.FormEvents.CreditCard"))));
}

TEST_F(BrowserAutofillManagerTest, DidShowSuggestions_LogByType_AddressOnly) {
  // Create a form with name and address fields.
  FormData form;
  form.name = u"MyForm";
  form.button_titles = {std::make_pair(
      u"Submit", mojom::ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE)};
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  form.main_frame_origin =
      url::Origin::Create(GURL("https://myform_root.com/form.html"));
  form.submission_event = SubmissionIndicatorEvent::SAME_DOCUMENT_NAVIGATION;

  FormFieldData field;
  test::CreateTestFormField("First Name", "firstname", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address Line 1", "addr1", "", "text", &field);
  form.fields.push_back(field);
  FormsSeen({form});

  base::HistogramTester histogram_tester;
  browser_autofill_manager_->DidShowSuggestions(
      /*has_autofill_suggestions=*/true, form, form.fields[0]);

  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressOnly",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressOnly",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);

  // Logging is not done for other types of address forms.
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(
      histograms,
      Not(AnyOf(
          HasSubstr("Autofill.FormEvents.Address.AddressPlusContact"),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusEmail "),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusEmailPlusPhone"),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusPhone"),
          HasSubstr("Autofill.FormEvents.Address.ContactOnly"),
          HasSubstr("Autofill.FormEvents.Address.PhoneOnly"),
          HasSubstr("Autofill.FormEvents.Address.Other"))));
}

TEST_F(BrowserAutofillManagerTest,
       DidShowSuggestions_LogByType_AddressOnlyWithoutName) {
  // Create a form with address fields.
  FormData form;
  form.name = u"MyForm";
  form.button_titles = {std::make_pair(
      u"Submit", mojom::ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE)};
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  form.main_frame_origin =
      url::Origin::Create(GURL("https://myform_root.com/form.html"));
  form.submission_event = SubmissionIndicatorEvent::SAME_DOCUMENT_NAVIGATION;

  FormFieldData field;
  test::CreateTestFormField("Address Line 1", "addr1", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address Line 2", "addr2", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Postal Code", "zipcode", "", "text", &field);
  form.fields.push_back(field);
  FormsSeen({form});

  base::HistogramTester histogram_tester;
  browser_autofill_manager_->DidShowSuggestions(
      /*has_autofill_suggestions=*/true, form, form.fields[0]);

  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressOnly",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressOnly",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);

  // Logging is not done for other types of address forms.
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(
      histograms,
      Not(AnyOf(
          HasSubstr("Autofill.FormEvents.Address.AddressPlusContact"),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusEmail "),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusEmailPlusPhone"),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusPhone"),
          HasSubstr("Autofill.FormEvents.Address.ContactOnly"),
          HasSubstr("Autofill.FormEvents.Address.PhoneOnly"),
          HasSubstr("Autofill.FormEvents.Address.Other"))));
}

TEST_F(BrowserAutofillManagerTest, DidShowSuggestions_LogByType_ContactOnly) {
  // Create a form with name and contact fields.
  FormData form;
  form.name = u"MyForm";
  form.button_titles = {std::make_pair(
      u"Submit", mojom::ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE)};
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  form.main_frame_origin =
      url::Origin::Create(GURL("https://myform_root.com/form.html"));
  form.submission_event = SubmissionIndicatorEvent::SAME_DOCUMENT_NAVIGATION;

  FormFieldData field;
  test::CreateTestFormField("First Name", "firstname", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email", "email", "", "email", &field);
  form.fields.push_back(field);
  FormsSeen({form});

  base::HistogramTester histogram_tester;
  browser_autofill_manager_->DidShowSuggestions(
      /*has_autofill_suggestions=*/true, form, form.fields[0]);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.ContactOnly",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.ContactOnly",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);

  // Logging is not done for other types of address forms.
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(
      histograms,
      Not(AnyOf(
          HasSubstr("Autofill.FormEvents.Address.AddressPlusContact"),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusEmail "),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusEmailPlusPhone"),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusPhone"),
          HasSubstr("Autofill.FormEvents.Address.AddressOnly"),
          HasSubstr("Autofill.FormEvents.Address.PhoneOnly"),
          HasSubstr("Autofill.FormEvents.Address.Other"))));
}

TEST_F(BrowserAutofillManagerTest,
       DidShowSuggestions_LogByType_ContactOnlyWithoutName) {
  // Create a form with contact fields.
  FormData form;
  form.name = u"MyForm";
  form.button_titles = {std::make_pair(
      u"Submit", mojom::ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE)};
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  form.main_frame_origin =
      url::Origin::Create(GURL("https://myform_root.com/form.html"));
  form.submission_event = SubmissionIndicatorEvent::SAME_DOCUMENT_NAVIGATION;

  FormFieldData field;
  test::CreateTestFormField("Phone Number", "phonenumber1", "", "tel",
                            "tel-country-code", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Phone Number", "phonenumber2", "", "tel",
                            "tel-national", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email", "email", "", "email", "email", &field);
  form.fields.push_back(field);
  FormsSeen({form});

  base::HistogramTester histogram_tester;
  browser_autofill_manager_->DidShowSuggestions(
      /*has_autofill_suggestions=*/true, form, form.fields[0]);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.ContactOnly",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.ContactOnly",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);

  // Logging is not done for other types of address forms.
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(
      histograms,
      Not(AnyOf(
          HasSubstr("Autofill.FormEvents.Address.AddressPlusContact"),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusEmail "),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusEmailPlusPhone"),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusPhone"),
          HasSubstr("Autofill.FormEvents.Address.AddressOnly"),
          HasSubstr("Autofill.FormEvents.Address.PhoneOnly"),
          HasSubstr("Autofill.FormEvents.Address.Other"))));
}

TEST_F(BrowserAutofillManagerTest, DidShowSuggestions_LogByType_PhoneOnly) {
  // Create a form with phone field.
  FormData form;
  form.name = u"MyForm";
  form.button_titles = {std::make_pair(
      u"Submit", mojom::ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE)};
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  form.main_frame_origin =
      url::Origin::Create(GURL("https://myform_root.com/form.html"));
  form.submission_event = SubmissionIndicatorEvent::SAME_DOCUMENT_NAVIGATION;

  FormFieldData field;
  test::CreateTestFormField("Phone Number", "phonenumber", "", "tel",
                            "tel-country-code", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Phone Number", "phonenumber", "", "tel",
                            "tel-area-code", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Phone Number", "phonenumber", "", "tel",
                            "tel-local", &field);
  form.fields.push_back(field);
  FormsSeen({form});

  base::HistogramTester histogram_tester;
  browser_autofill_manager_->DidShowSuggestions(
      /*has_autofill_suggestions=*/true, form, form.fields[0]);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.PhoneOnly",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.PhoneOnly",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);

  // Logging is not done for other types of address forms.
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(
      histograms,
      Not(AnyOf(
          HasSubstr("Autofill.FormEvents.Address.AddressPlusEmailPlusPhone"),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusEmail"),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusPhone"),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusContact"),
          HasSubstr("Autofill.FormEvents.Address.AddressOnly"),
          HasSubstr("Autofill.FormEvents.Address.ContactOnly"),
          HasSubstr("Autofill.FormEvents.Address.Other"))));
}

TEST_F(BrowserAutofillManagerTest, DidShowSuggestions_LogByType_Other) {
  // Create a form with name fields.
  FormData form;
  form.name = u"MyForm";
  form.button_titles = {std::make_pair(
      u"Submit", mojom::ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE)};
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  form.main_frame_origin =
      url::Origin::Create(GURL("https://myform_root.com/form.html"));
  form.submission_event = SubmissionIndicatorEvent::SAME_DOCUMENT_NAVIGATION;

  FormFieldData field;
  test::CreateTestFormField("First Name", "firstname", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Middle Name", "middlename", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  form.fields.push_back(field);
  FormsSeen({form});

  base::HistogramTester histogram_tester;
  browser_autofill_manager_->DidShowSuggestions(
      /*has_autofill_suggestions=*/true, form, form.fields[0]);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.Other",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.Other",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);

  // Logging is not done for other types of address forms.
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(
      histograms,
      Not(AnyOf(
          HasSubstr("Autofill.FormEvents.Address.AddressPlusContact"),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusEmail "),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusEmailPlusPhone"),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusPhone"),
          HasSubstr("Autofill.FormEvents.Address.AddressOnly"),
          HasSubstr("Autofill.FormEvents.Address.PhoneOnly"),
          HasSubstr("Autofill.FormEvents.Address.ContactOnly"))));
}

TEST_F(BrowserAutofillManagerTest,
       DidShowSuggestions_LogByType_AddressPlusEmail) {
  // Create a form with name, address, and email fields.
  FormData form;
  form.name = u"MyForm";
  form.button_titles = {std::make_pair(
      u"Submit", mojom::ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE)};
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  form.main_frame_origin =
      url::Origin::Create(GURL("https://myform_root.com/form.html"));
  form.submission_event = SubmissionIndicatorEvent::SAME_DOCUMENT_NAVIGATION;

  FormFieldData field;
  test::CreateTestFormField("First Name", "firstname", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address Line 1", "addr1", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email", "email", "", "email", &field);
  form.fields.push_back(field);
  FormsSeen({form});

  base::HistogramTester histogram_tester;
  browser_autofill_manager_->DidShowSuggestions(
      /*has_autofill_suggestions=*/true, form, form.fields[0]);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusEmail",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusEmail",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusContact",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusContact",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);

  // Logging is not done for other types of address forms.
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(
      histograms,
      Not(AnyOf(
          HasSubstr("Autofill.FormEvents.Address.AddressPlusEmailPlusPhone"),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusPhone"),
          HasSubstr("Autofill.FormEvents.Address.AddressOnly"),
          HasSubstr("Autofill.FormEvents.Address.ContactOnly"),
          HasSubstr("Autofill.FormEvents.Address.PhoneOnly"),
          HasSubstr("Autofill.FormEvents.Address.Other"))));
}

TEST_F(BrowserAutofillManagerTest,
       DidShowSuggestions_LogByType_AddressPlusEmailWithoutName) {
  // Create a form with address and email fields.
  FormData form;
  form.name = u"MyForm";
  form.button_titles = {std::make_pair(
      u"Submit", mojom::ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE)};
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  form.main_frame_origin =
      url::Origin::Create(GURL("https://myform_root.com/form.html"));
  form.submission_event = SubmissionIndicatorEvent::SAME_DOCUMENT_NAVIGATION;

  FormFieldData field;
  test::CreateTestFormField("Address Line 1", "addr1", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address Line 2", "addr2", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email", "email", "", "email", &field);
  form.fields.push_back(field);
  FormsSeen({form});

  base::HistogramTester histogram_tester;
  browser_autofill_manager_->DidShowSuggestions(
      /*has_autofill_suggestions=*/true, form, form.fields[0]);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusEmail",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusEmail",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusContact",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusContact",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);

  // Logging is not done for other types of address forms.
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(
      histograms,
      Not(AnyOf(
          HasSubstr("Autofill.FormEvents.Address.AddressPlusEmailPlusPhone"),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusPhone"),
          HasSubstr("Autofill.FormEvents.Address.AddressOnly"),
          HasSubstr("Autofill.FormEvents.Address.ContactOnly"),
          HasSubstr("Autofill.FormEvents.Address.PhoneOnly"),
          HasSubstr("Autofill.FormEvents.Address.Other"))));
}

TEST_F(BrowserAutofillManagerTest,
       DidShowSuggestions_LogByType_AddressPlusPhone) {
  // Create a form with name fields.
  FormData form;
  form.name = u"MyForm";
  form.button_titles = {std::make_pair(
      u"Submit", mojom::ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE)};
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  form.main_frame_origin =
      url::Origin::Create(GURL("https://myform_root.com/form.html"));
  form.submission_event = SubmissionIndicatorEvent::SAME_DOCUMENT_NAVIGATION;

  FormFieldData field;
  test::CreateTestFormField("First Name", "firstname", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address Line 1", "addr1", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Phone Number", "phonenumber", "", "tel", &field);
  form.fields.push_back(field);
  FormsSeen({form});

  base::HistogramTester histogram_tester;
  browser_autofill_manager_->DidShowSuggestions(
      /*has_autofill_suggestions=*/true, form, form.fields[0]);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusPhone",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusPhone",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusContact",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusContact",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);

  // Logging is not done for other types of address forms.
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(
      histograms,
      Not(AnyOf(
          HasSubstr("Autofill.FormEvents.Address.AddressPlusEmailPlusPhone"),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusEmail "),
          HasSubstr("Autofill.FormEvents.Address.AddressOnly"),
          HasSubstr("Autofill.FormEvents.Address.ContactOnly"),
          HasSubstr("Autofill.FormEvents.Address.PhoneOnly"),
          HasSubstr("Autofill.FormEvents.Address.Other"))));
}

TEST_F(BrowserAutofillManagerTest,
       DidShowSuggestions_LogByType_AddressPlusPhoneWithoutName) {
  // Create a form with name, address, and phone fields.
  FormData form;
  form.name = u"MyForm";
  form.button_titles = {std::make_pair(
      u"Submit", mojom::ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE)};
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  form.main_frame_origin =
      url::Origin::Create(GURL("https://myform_root.com/form.html"));
  form.submission_event = SubmissionIndicatorEvent::SAME_DOCUMENT_NAVIGATION;

  FormFieldData field;
  test::CreateTestFormField("Address Line 1", "addr1", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address Line 2", "addr2", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Phone Number", "phonenumber", "", "tel", &field);
  form.fields.push_back(field);
  FormsSeen({form});

  base::HistogramTester histogram_tester;
  browser_autofill_manager_->DidShowSuggestions(
      /*has_autofill_suggestions=*/true, form, form.fields[0]);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusPhone",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusPhone",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusContact",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusContact",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);

  // Logging is not done for other types of address forms.
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(
      histograms,
      Not(AnyOf(
          HasSubstr("Autofill.FormEvents.Address.AddressPlusEmailPlusPhone"),
          HasSubstr("Autofill.FormEvents.Address.AddressPlusEmail "),
          HasSubstr("Autofill.FormEvents.Address.AddressOnly"),
          HasSubstr("Autofill.FormEvents.Address.ContactOnly"),
          HasSubstr("Autofill.FormEvents.Address.PhoneOnly"),
          HasSubstr("Autofill.FormEvents.Address.Other"))));
}

TEST_F(BrowserAutofillManagerTest,
       DidShowSuggestions_LogByType_AddressPlusEmailPlusPhone) {
  // Create a form with name, address, phone, and email fields.
  FormData form;
  form.name = u"MyForm";
  form.button_titles = {std::make_pair(
      u"Submit", mojom::ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE)};
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  form.main_frame_origin =
      url::Origin::Create(GURL("https://myform_root.com/form.html"));
  form.submission_event = SubmissionIndicatorEvent::SAME_DOCUMENT_NAVIGATION;

  FormFieldData field;
  test::CreateTestFormField("First Name", "firstname", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address Line 1", "addr1", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Phone Number", "phonenumber", "", "tel", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email", "email", "", "email", &field);
  form.fields.push_back(field);
  FormsSeen({form});

  base::HistogramTester histogram_tester;
  browser_autofill_manager_->DidShowSuggestions(
      /*has_autofill_suggestions=*/true, form, form.fields[0]);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusEmailPlusPhone",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusEmailPlusPhone",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusContact",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusContact",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);

  // Logging is not done for other types of address forms.
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(
      histograms,
      Not(AnyOf(HasSubstr("Autofill.FormEvents.Address.AddressPlusPhone"),
                HasSubstr("Autofill.FormEvents.Address.AddressPlusEmail "),
                HasSubstr("Autofill.FormEvents.Address.AddressOnly"),
                HasSubstr("Autofill.FormEvents.Address.ContactOnly"),
                HasSubstr("Autofill.FormEvents.Address.PhoneOnly"),
                HasSubstr("Autofill.FormEvents.Address.Other"))));
}

TEST_F(BrowserAutofillManagerTest,
       DidShowSuggestions_LogByType_AddressPlusEmailPlusPhoneWithoutName) {
  // Create a form with address, phone, and email fields.
  FormData form;
  form.name = u"MyForm";
  form.button_titles = {std::make_pair(
      u"Submit", mojom::ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE)};
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  form.main_frame_origin =
      url::Origin::Create(GURL("https://myform_root.com/form.html"));
  form.submission_event = SubmissionIndicatorEvent::SAME_DOCUMENT_NAVIGATION;

  FormFieldData field;
  test::CreateTestFormField("Address Line 1", "addr1", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Phone Number", "phonenumber", "", "tel", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email", "email", "", "email", &field);
  form.fields.push_back(field);
  FormsSeen({form});

  base::HistogramTester histogram_tester;
  browser_autofill_manager_->DidShowSuggestions(
      /*has_autofill_suggestions=*/true, form, form.fields[0]);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusEmailPlusPhone",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusEmailPlusPhone",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusContact",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address.AddressPlusContact",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);

  // Logging is not done for other types of address forms.
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(
      histograms,
      Not(AnyOf(HasSubstr("Autofill.FormEvents.Address.AddressPlusPhone"),
                HasSubstr("Autofill.FormEvents.Address.AddressPlusEmail "),
                HasSubstr("Autofill.FormEvents.Address.AddressOnly"),
                HasSubstr("Autofill.FormEvents.Address.ContactOnly"),
                HasSubstr("Autofill.FormEvents.Address.PhoneOnly"),
                HasSubstr("Autofill.FormEvents.Address.Other"))));
}

TEST_F(BrowserAutofillManagerTest,
       DidShowSuggestions_LogAutofillCreditCardShownMetric) {
  FormData form = CreateTestCreditCardFormData(true, false);
  FormsSeen({form});

  base::HistogramTester histogram_tester;
  browser_autofill_manager_->DidShowSuggestions(
      /*has_autofill_suggestions=*/true, form, form.fields[0]);
  histogram_tester.ExpectBucketCount("Autofill.UserHappiness",
                                     AutofillMetrics::SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount("Autofill.UserHappiness.CreditCard",
                                     AutofillMetrics::SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.UserHappiness", AutofillMetrics::SUGGESTIONS_SHOWN_ONCE, 1);
  histogram_tester.ExpectBucketCount("Autofill.UserHappiness.CreditCard",
                                     AutofillMetrics::SUGGESTIONS_SHOWN_ONCE,
                                     1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.CreditCard",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.CreditCard",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);

  // No Autocomplete or address logs.
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(histograms, Not(AnyOf(HasSubstr("Autocomplete.Events"),
                                    HasSubstr("Autofill.FormEvents.Address"))));
}

TEST_F(BrowserAutofillManagerTest,
       DidSuppressPopup_LogAutofillAddressPopupSuppressed) {
  FormData form;
  test::CreateTestAddressFormData(&form);
  browser_autofill_manager_->OnFormsSeen({form}, {});

  base::HistogramTester histogram_tester;
  browser_autofill_manager_->DidSuppressPopup(form, form.fields[0]);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address",
      autofill_metrics::FORM_EVENT_POPUP_SUPPRESSED, 1);

  // No Autocomplete or credit cards logs.
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(histograms,
              Not(AnyOf(HasSubstr("Autofill.UserHappiness"),
                        HasSubstr("Autocomplete.Events"),
                        HasSubstr("Autofill.FormEvents.CreditCard"))));
}

TEST_F(BrowserAutofillManagerTest,
       DidSuppressPopup_LogAutofillCreditCardPopupSuppressed) {
  FormData form = CreateTestCreditCardFormData(true, false);

  browser_autofill_manager_->OnFormsSeen({form}, {});
  base::HistogramTester histogram_tester;
  browser_autofill_manager_->DidSuppressPopup(form, form.fields[0]);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.CreditCard",
      autofill_metrics::FORM_EVENT_POPUP_SUPPRESSED, 1);

  // No Autocomplete or address logs.
  const std::string histograms = histogram_tester.GetAllHistogramsRecorded();
  EXPECT_THAT(histograms, Not(AnyOf(HasSubstr("Autofill.UserHappiness"),
                                    HasSubstr("Autocomplete.Events"),
                                    HasSubstr("Autofill.FormEvents.Address"))));
}

// Test that we import data when the field type is determined by the value and
// without any heuristics on the attributes.
TEST_F(BrowserAutofillManagerTest, ImportDataWhenValueDetected) {
  const std::string test_upi_id_value = "user@indianbank";

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kAutofillSaveAndFillVPA);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  EXPECT_CALL(autofill_client_, ConfirmSaveUpiIdLocally(test_upi_id_value, _))
      .WillOnce([](std::string upi_id,
                   base::OnceCallback<void(bool user_decision)> callback) {
        std::move(callback).Run(true);
      });
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("UPI ID:", "upi_id", "", "text", &field);
  form.fields.push_back(field);

  FormsSeen({form});
  browser_autofill_manager_->SetExpectedSubmittedFieldTypes({{UPI_VPA}});
  browser_autofill_manager_->SetExpectedObservedSubmission(true);
  form.submission_event =
      mojom::SubmissionIndicatorEvent::SAME_DOCUMENT_NAVIGATION;

  form.fields[0].value = base::UTF8ToUTF16(test_upi_id_value);
  FormSubmitted(form);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  // The feature is not implemented for mobile.
  EXPECT_EQ(0, personal_data().num_times_save_upi_id_called());
#else
  EXPECT_EQ(1, personal_data().num_times_save_upi_id_called());
#endif
}

// Test that we do not import UPI data when in incognito.
TEST_F(BrowserAutofillManagerTest, DontImportUpiIdWhenIncognito) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kAutofillSaveAndFillVPA);
  autofill_client_.set_is_off_the_record(true);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  EXPECT_CALL(autofill_client_, ConfirmSaveUpiIdLocally(_, _)).Times(0);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("UPI ID:", "upi_id", "", "text", &field);
  form.fields.push_back(field);

  FormsSeen({form});
  browser_autofill_manager_->SetExpectedSubmittedFieldTypes({{UPI_VPA}});
  browser_autofill_manager_->SetExpectedObservedSubmission(true);
  form.submission_event =
      mojom::SubmissionIndicatorEvent::SAME_DOCUMENT_NAVIGATION;

  form.fields[0].value = u"user@indianbank";
  FormSubmitted(form);

  EXPECT_EQ(0, personal_data().num_times_save_upi_id_called());
}

TEST_F(BrowserAutofillManagerTest, PageLanguageGetsCorrectlySet) {
  FormData form;
  test::CreateTestAddressFormData(&form);

  browser_autofill_manager_->OnFormsSeen({form}, {});
  FormStructure* parsed_form =
      browser_autofill_manager_->FindCachedFormById(form.global_id());

  ASSERT_TRUE(parsed_form);
  ASSERT_EQ(LanguageCode(), parsed_form->current_page_language());

  autofill_client_.GetLanguageState()->SetCurrentLanguage("zh");

  browser_autofill_manager_->OnFormsSeen({form}, {});
  parsed_form = browser_autofill_manager_->FindCachedFormById(form.global_id());

  ASSERT_EQ(LanguageCode("zh"), parsed_form->current_page_language());
}

// Test language detection on frames depending on whether the frame is active or
// not.
class BrowserAutofillManagerTestPageLanguageDetection
    : public BrowserAutofillManagerTest,
      public testing::WithParamInterface<bool> {
 public:
  BrowserAutofillManagerTestPageLanguageDetection() {
    scoped_features_.InitAndEnableFeature(
        features::kAutofillPageLanguageDetection);
  }

  bool is_in_active_frame() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_features_;
};

TEST_P(BrowserAutofillManagerTestPageLanguageDetection, GetsCorrectlyDetected) {
  FormData form;
  test::CreateTestAddressFormData(&form);

  browser_autofill_manager_->OnFormsSeen({form}, {});
  FormStructure* parsed_form =
      browser_autofill_manager_->FindCachedFormById(form.global_id());

  ASSERT_TRUE(parsed_form);
  ASSERT_EQ(LanguageCode(), parsed_form->current_page_language());

  translate::LanguageDetectionDetails language_detection_details;
  language_detection_details.adopted_language = "hu";
  autofill_driver_->SetIsInActiveFrame(is_in_active_frame());
  browser_autofill_manager_->OnLanguageDetermined(language_detection_details);

  autofill_client_.GetLanguageState()->SetCurrentLanguage("hu");

  parsed_form = browser_autofill_manager_->FindCachedFormById(form.global_id());

  // Language detection is used only for active frames.
  auto expected_language_code =
      is_in_active_frame() ? LanguageCode("hu") : LanguageCode();

  ASSERT_EQ(*expected_language_code, *parsed_form->current_page_language());
}

INSTANTIATE_TEST_SUITE_P(All,
                         BrowserAutofillManagerTestPageLanguageDetection,
                         testing::Bool());

// BrowserAutofillManagerTest with different browser profile types.
class BrowserAutofillManagerProfileMetricsTest
    : public BrowserAutofillManagerTest,
      public testing::WithParamInterface<profile_metrics::BrowserProfileType> {
 public:
  BrowserAutofillManagerProfileMetricsTest() : profile_type_(GetParam()) {
    EXPECT_CALL(autofill_client_, GetProfileType())
        .WillRepeatedly(Return(profile_type_));
  }

  const profile_metrics::BrowserProfileType profile_type_;
};

// Tests if submitting a form in different browser profile types records correct
// |Autofill.FormSubmission.PerProfileType| metric.
TEST_P(BrowserAutofillManagerProfileMetricsTest,
       FormSubmissionPerProfileTypeMetrics) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  FormsSeen({form});
  GetAutofillSuggestions(form, form.fields[0]);

  base::HistogramTester histogram_tester;

  FormSubmitted(form);
  histogram_tester.ExpectBucketCount("Autofill.FormSubmission.PerProfileType",
                                     profile_type_, 1);
  histogram_tester.ExpectTotalCount("Autofill.FormSubmission.PerProfileType",
                                    1);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    BrowserAutofillManagerProfileMetricsTest,
    testing::ValuesIn({profile_metrics::BrowserProfileType::kRegular,
                       profile_metrics::BrowserProfileType::kIncognito,
                       profile_metrics::BrowserProfileType::kGuest}));

// Tests that autocomplete-related metrics are emitted correctly on form
// submission.
TEST_F(BrowserAutofillManagerTest, AutocompleteMetrics) {
  // `kAutocompleteValues` corresponds to empty, valid, garbage and off.
  constexpr const char* kAutocompleteValues[]{"", "name", "asdf", "off"};
  // The 4 possible combinations of heuristic and server type status:
  // - Neither a fillable heuristic type nor a fillable server type.
  // - Only a fillable server type.
  // - Only a fillable heuristic type.
  // - Both a fillable heuristic type and a fillable server type.
  // NO_SERVER_DATA and UNKNOWN_TYPE are both unfillable types, but
  // NO_SERVER_DATA is ignored in the PredictionCollisionType metric.
  constexpr ServerFieldType kTypeClasses[][2]{
      {UNKNOWN_TYPE, NO_SERVER_DATA},
      {UNKNOWN_TYPE, EMAIL_ADDRESS},
      {ADDRESS_HOME_COUNTRY, UNKNOWN_TYPE},
      {ADDRESS_HOME_COUNTRY, EMAIL_ADDRESS}};

  // Create a form with one field per kAutofillValue x kTypeClass combination.
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  std::vector<ServerFieldType> heuristic_types, server_types;
  for (const char* autocomplete : kAutocompleteValues) {
    for (const auto& types : kTypeClasses) {
      FormFieldData field;
      test::CreateTestFormField("", "", "", "text", autocomplete, &field);
      form.fields.push_back(field);
      heuristic_types.push_back(types[0]);
      server_types.push_back(types[1]);
    }
  }
  // Override the types and simulate seeing the form on page load.
  auto form_structure = std::make_unique<FormStructure>(form);
  form_structure->DetermineHeuristicTypes(nullptr, nullptr);
  FormStructureTestApi(form_structure.get())
      .SetFieldTypes(heuristic_types, server_types);
  browser_autofill_manager_->AddSeenFormStructure(std::move(form_structure));

  // Submit the form and verify that all metrics are collected correctly.
  base::HistogramTester histogram_tester;
  FormSubmitted(form);

  // Expect one entry for each possible PredictionStateAutocompleteStatePair.
  // Fields without type predictions and autocomplete attributes are ignored.
  histogram_tester.ExpectTotalCount(
      "Autofill.Autocomplete.PredictionCollisionState", form.fields.size() - 1);
  for (int i = 0; i < 15; i++) {
    histogram_tester.ExpectBucketCount(
        "Autofill.Autocomplete.PredictionCollisionState", i, 1);
  }

  // A separate PredictionCollisionType metric exists for every prediction-type
  // autocomplete status combination. Above, we created four fields per
  // `kAutocompleteValues` - so we expect 4 samples in every bucket.
  // The exception is the .Server metric, which is not emitted for
  // NO_SERVER_DATA and hence expects only three samples.
  const std::string kTypeHistogram =
      "Autofill.Autocomplete.PredictionCollisionType2.";
  for (const char* suffix : {"Garbage", "None", "Off", "Valid"}) {
    histogram_tester.ExpectTotalCount(kTypeHistogram + "Heuristics." + suffix,
                                      4);
    histogram_tester.ExpectTotalCount(kTypeHistogram + "Server." + suffix, 3);
    histogram_tester.ExpectTotalCount(
        kTypeHistogram + "ServerOrHeuristics." + suffix, 4);
  }
}

struct ContextMenuImpressionTestCase {
  // Autocomplete attribute value.
  const char* autocomplete_attribute_value;
  // Heuristic type for the field in the test case.
  ServerFieldType heuristic_type;
  // Server type for the field in the test case.
  ServerFieldType server_type;
  // Expected autocomplete state that would be logged in the metrics.
  AutofillMetrics::AutocompleteState expected_autocomplete_state;
  // Expected autofill type that would be logged in the metrics.
  ServerFieldType expected_autofill_type;
};

class BrowserAutofillManagerContextMenuImpressionsTest
    : public testing::WithParamInterface<ContextMenuImpressionTestCase>,
      public BrowserAutofillManagerTest {};

INSTANTIATE_TEST_SUITE_P(
    BrowserAutofillManagerContextMenuTests,
    BrowserAutofillManagerContextMenuImpressionsTest,
    testing::Values(
        // Empty Autocomplete attribute
        ContextMenuImpressionTestCase{"", UNKNOWN_TYPE, NO_SERVER_DATA,
                                      AutofillMetrics::AutocompleteState::kNone,
                                      UNKNOWN_TYPE},
        // Valid Autocomplete attribute
        ContextMenuImpressionTestCase{
            "name", UNKNOWN_TYPE, EMAIL_ADDRESS,
            AutofillMetrics::AutocompleteState::kValid, NAME_FULL},
        // Garbage Autocomplete attribute
        ContextMenuImpressionTestCase{
            "asdf", ADDRESS_HOME_COUNTRY, UNKNOWN_TYPE,
            AutofillMetrics::AutocompleteState::kGarbage, UNKNOWN_TYPE},
        // Off Autocomplete attribute
        ContextMenuImpressionTestCase{
            "off", ADDRESS_HOME_COUNTRY, EMAIL_ADDRESS,
            AutofillMetrics::AutocompleteState::kOff, EMAIL_ADDRESS}));

// Tests that metrics are emitted correctly on form submission for the fields
// from where the context menu was triggered.
TEST_P(BrowserAutofillManagerContextMenuImpressionsTest,
       ContextMenuImpressionMetrics) {
  auto test_case = GetParam();

  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  FormFieldData field;
  test::CreateTestFormField("", "", "", "text",
                            test_case.autocomplete_attribute_value, &field);
  form.fields.push_back(field);

  // Override the types and simulate seeing the form on page load.
  auto form_structure = std::make_unique<FormStructure>(form);
  FormStructureTestApi(form_structure.get())
      .SetFieldTypes({test_case.heuristic_type}, {test_case.server_type});
  browser_autofill_manager_->AddSeenFormStructure(std::move(form_structure));

  // Simulate context menu trigger for all the fields.
  browser_autofill_manager_->OnContextMenuShownInField(
      form.global_id(), form.fields[0].global_id());

  // Submit the form and verify that all metrics are collected correctly.
  base::HistogramTester histogram_tester;
  FormSubmitted(form);

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Autofill.FieldContextMenuImpressions.ByAutocomplete"),
      BucketsAre(base::Bucket(test_case.expected_autocomplete_state, 1)));

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Autofill.FieldContextMenuImpressions.ByAutofillType"),
              BucketsAre(base::Bucket(test_case.expected_autofill_type, 1)));

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Autofill.FormContextMenuImpressions.ByNumberOfFields"),
              BucketsAre(base::Bucket(1, 1)));
}

// Test that if a form is mixed content we show a warning instead of any
// suggestions.
TEST_F(BrowserAutofillManagerTest, GetSuggestions_MixedForm) {
  // Set up our form data.
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");
  FormFieldData field;
  test::CreateTestFormField("Name on Card", "nameoncard", "", "text", &field);
  form.fields.push_back(field);

  GetAutofillSuggestions(form, form.fields[0]);

  // Test that we sent the right values to the external delegate.
  CheckSuggestions(
      field.global_id(),
      Suggestion(l10n_util::GetStringUTF8(IDS_AUTOFILL_WARNING_MIXED_FORM), "",
                 "", POPUP_ITEM_ID_MIXED_FORM_MESSAGE));
}

// Test that if a form is mixed content we do not show a warning if the opt out
// policy is set.
TEST_F(BrowserAutofillManagerTest, GetSuggestions_MixedFormOptoutPolicy) {
  // Set pref to disabled.
  autofill_client_.GetPrefs()->SetBoolean(::prefs::kMixedFormsWarningsEnabled,
                                          false);

  // Set up our form data.
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");
  FormFieldData field;
  test::CreateTestFormField("Name on Card", "nameoncard", "", "text", &field);
  form.fields.push_back(field);
  GetAutofillSuggestions(form, form.fields[0]);

  // Check there is no warning.
  EXPECT_FALSE(external_delegate_->on_suggestions_returned_seen());
}

// Test that we dismiss the mixed form warning if user starts typing.
TEST_F(BrowserAutofillManagerTest, GetSuggestions_MixedFormUserTyped) {
  // Set up our form data.
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");
  FormFieldData field;
  test::CreateTestFormField("Name on Card", "nameoncard", "", "text", &field);
  form.fields.push_back(field);

  GetAutofillSuggestions(form, form.fields[0]);

  // Test that we sent the right values to the external delegate.
  CheckSuggestions(
      field.global_id(),
      Suggestion(l10n_util::GetStringUTF8(IDS_AUTOFILL_WARNING_MIXED_FORM), "",
                 "", POPUP_ITEM_ID_MIXED_FORM_MESSAGE));

  // Pretend user started typing and make sure we no longer set suggestions.
  form.fields[0].value = u"Michael";
  form.fields[0].properties_mask |= kUserTyped;
  GetAutofillSuggestions(form, form.fields[0]);
  external_delegate_->CheckNoSuggestions(form.fields[0].global_id());
}

// Test that we don't treat javascript scheme target URLs as mixed forms.
// Regression test for crbug.com/1135173
TEST_F(BrowserAutofillManagerTest, GetSuggestions_JavascriptUrlTarget) {
  // Set up our form data, using a javascript scheme target URL.
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("javascript:alert('hello');");
  FormFieldData field;
  test::CreateTestFormField("Name on Card", "nameoncard", "", "text", &field);
  form.fields.push_back(field);
  GetAutofillSuggestions(form, form.fields[0]);

  // Check there is no warning.
  EXPECT_FALSE(external_delegate_->on_suggestions_returned_seen());
}

// Test that we don't treat about:blank target URLs as mixed forms.
TEST_F(BrowserAutofillManagerTest, GetSuggestions_AboutBlankTarget) {
  // Set up our form data, using a javascript scheme target URL.
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("about:blank");
  FormFieldData field;
  test::CreateTestFormField("Name on Card", "nameoncard", "", "text", &field);
  form.fields.push_back(field);
  GetAutofillSuggestions(form, form.fields[0]);

  // Check there is no warning.
  EXPECT_FALSE(external_delegate_->on_suggestions_returned_seen());
}

// Test that the Autofill does not override input field values that were already
// prefilled.
TEST_F(BrowserAutofillManagerTest, PreventOverridingOfPrefilledValues) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      autofill::features::kAutofillPreventOverridingPrefilledValues);
  // Set up our form data.
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("about:blank");
  FormFieldData field;
  test::CreateTestFormField("Name", "name", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City", "city", "Test City", "text", &field);
  form.fields.push_back(field);
  test::CreateTestSelectField("State", "state", "California",
                              {"Washington", "Tennessee", "California"},
                              {"DC", "TN", "CA"}, &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Country", "country", "Test Country", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Phone Number", "phonenumber", "12345678901", "tel",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip Code", "zipcode", "_____", "text", &field);
  form.fields.push_back(field);
  FormsSeen({form});

  const char guid[] = "00000000-0000-0000-0000-000000000001";
  FormData response_data;
  FillAutofillFormDataAndSaveResults(form, form.fields[0],
                                     MakeFrontendId({.profile_id = guid}),
                                     &response_data);
  EXPECT_EQ(response_data.fields[0].value, u"Elvis Aaron Presley");
  EXPECT_EQ(response_data.fields[1].value, u"Test City");
  EXPECT_EQ(response_data.fields[2].value, u"Tennessee");
  EXPECT_EQ(response_data.fields[3].value, u"Test Country");
  EXPECT_EQ(response_data.fields[4].value, u"12345678901");
  EXPECT_EQ(response_data.fields[5].value, u"38116");

  {
    FormStructure* form_structure;
    AutofillField* autofill_field;
    std::vector<std::string> expected_values = {
        "", "Memphis", "", "United States", "", ""};
    bool found = browser_autofill_manager_->GetCachedFormAndField(
        form, form.fields[0], &form_structure, &autofill_field);
    ASSERT_TRUE(found);
    for (size_t i = 0; i < form.fields.size(); ++i) {
      ASSERT_TRUE(form_structure->field(i)->SameFieldAs(form.fields[i]));
      if (!expected_values[i].empty()) {
        EXPECT_TRUE(form_structure->field(i)
                        ->value_not_autofilled_over_existing_value_hash()
                        .has_value());
        EXPECT_FALSE(form_structure->field(i)->is_autofilled);
        EXPECT_EQ(form_structure->field(i)
                      ->value_not_autofilled_over_existing_value_hash(),
                  base::FastHash(expected_values[i]));
      }
    }

    EXPECT_TRUE(form_structure->field(0)->is_autofilled);  // No prefilled value
    EXPECT_TRUE(form_structure->field(2)->is_autofilled);  // Selection field.

    // Prefilled value is same as the value to be autofilled so the field is
    // overridden.
    EXPECT_FALSE(form_structure->field(4)->is_autofilled);
    EXPECT_FALSE(form_structure->field(4)
                     ->value_not_autofilled_over_existing_value_hash());

    // Field contained placeholder "______" value.
    EXPECT_TRUE(form_structure->field(5)->is_autofilled);
  }

  features.Reset();
  features.InitAndDisableFeature(
      autofill::features::kAutofillPreventOverridingPrefilledValues);

  FillAutofillFormDataAndSaveResults(form, form.fields[0],
                                     MakeFrontendId({.profile_id = guid}),
                                     &response_data);
  EXPECT_EQ(response_data.fields[0].value, u"Elvis Aaron Presley");
  EXPECT_EQ(response_data.fields[1].value, u"Memphis");
  EXPECT_EQ(response_data.fields[2].value, u"Tennessee");
  EXPECT_EQ(response_data.fields[3].value, u"United States");
  EXPECT_EQ(response_data.fields[4].value, u"12345678901");
  EXPECT_EQ(response_data.fields[5].value, u"38116");
}

// Tests that the Autofill does override the prefilled field value since the
// field is the initiating field for the Autofill and has a prefilled value
// which is a substring of the autofillable value.
TEST_F(BrowserAutofillManagerTest, AutofillOverridePrefilledValue) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      autofill::features::kAutofillPreventOverridingPrefilledValues);

  // Set up our form data.
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("about:blank");
  FormFieldData field;
  test::CreateTestFormField("Name", "name", "Test Name", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City", "city", "Test City", "text", &field);
  form.fields.push_back(field);
  test::CreateTestSelectField("State", "state", "California",
                              {"Washington", "Tennessee", "California"},
                              {"DC", "TN", "CA"}, &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Country", "country", "Test Country", "text",
                            &field);
  form.fields.push_back(field);
  FormsSeen({form});

  // "Elv" is a substring of "Elvis Aaron Presley".
  form.fields[0].value = u"Elv";
  // Simulate editing a field.
  browser_autofill_manager_->OnTextFieldDidChange(
      form, form.fields.front(), gfx::RectF(), AutofillTickClock::NowTicks());

  const char guid[] = "00000000-0000-0000-0000-000000000001";
  FormData response_data;
  FillAutofillFormDataAndSaveResults(form, form.fields[0],
                                     MakeFrontendId({.profile_id = guid}),
                                     &response_data);
  EXPECT_EQ(response_data.fields[0].value, u"Elvis Aaron Presley");
  EXPECT_EQ(response_data.fields[1].value, u"Test City");
  EXPECT_EQ(response_data.fields[2].value, u"Tennessee");
  EXPECT_EQ(response_data.fields[3].value, u"Test Country");
}

// Tests that both Autofill popup and TTF are hidden on renderer event.
TEST_F(BrowserAutofillManagerTest, HideAutofillPopupAndOtherPopups) {
  EXPECT_CALL(autofill_client_,
              HideAutofillPopup(PopupHidingReason::kRendererEvent));
  EXPECT_CALL(touch_to_fill_delegate(), HideTouchToFill);
  EXPECT_CALL(fast_checkout_delegate(),
              HideFastCheckout(/*allow_further_runs=*/false));
  browser_autofill_manager_->OnHidePopup();
}

// Tests that only Autofill popup is hidden on editing end, but not TTF or FC.
TEST_F(BrowserAutofillManagerTest, OnDidEndTextFieldEditing) {
  EXPECT_CALL(autofill_client_,
              HideAutofillPopup(PopupHidingReason::kEndEditing));
  EXPECT_CALL(touch_to_fill_delegate(), HideTouchToFill).Times(0);
  EXPECT_CALL(fast_checkout_delegate(),
              HideFastCheckout(/*allow_further_runs=*/false))
      .Times(0);
  browser_autofill_manager_->OnDidEndTextFieldEditing();
}

// Tests that Autofill suggestions are not shown if TTF is eligible and shown.
TEST_F(BrowserAutofillManagerTest, AutofillSuggestionsOrTouchToFill) {
  FormData form;
  CreateTestCreditCardFormData(&form, /*is_https=*/true,
                               /*use_month_type=*/false);
  FormsSeen({form});
  const FormFieldData& field = form.fields[1];

  // Not a form element click, Autofill suggestions shown.
  EXPECT_CALL(touch_to_fill_delegate(), TryToShowTouchToFill(_, _)).Times(0);
  TryToShowTouchToFill(form, field, FormElementWasClicked(false));
  EXPECT_TRUE(external_delegate_->on_suggestions_returned_seen());

  // TTF not available, Autofill suggestions shown.
  EXPECT_CALL(touch_to_fill_delegate(), TryToShowTouchToFill(_, _))
      .WillOnce(Return(false));
  TryToShowTouchToFill(form, field, FormElementWasClicked(true));
  EXPECT_TRUE(external_delegate_->on_suggestions_returned_seen());

  // A form element click and TTF available, Autofill suggestions not shown.
  EXPECT_CALL(touch_to_fill_delegate(), TryToShowTouchToFill(_, _))
      .WillOnce(Return(true));
  TryToShowTouchToFill(form, field, FormElementWasClicked(true));
  EXPECT_FALSE(external_delegate_->on_suggestions_returned_seen());
}

// Tests that neither Autofill suggestions nor TTF is triggered if TTF is
// already shown.
TEST_F(BrowserAutofillManagerTest, ShowNothingIfTouchToFillAlreadyShown) {
  FormData form;
  CreateTestCreditCardFormData(&form, /*is_https=*/true,
                               /*use_month_type=*/false);
  FormsSeen({form});
  const FormFieldData& field = form.fields[1];

  EXPECT_CALL(touch_to_fill_delegate(), IsShowingTouchToFill)
      .WillOnce(Return(true));
  EXPECT_CALL(touch_to_fill_delegate(), TryToShowTouchToFill(_, _)).Times(0);
  TryToShowTouchToFill(form, field, FormElementWasClicked(true));
  EXPECT_FALSE(external_delegate_->on_suggestions_returned_seen());
}

// Test that fields will be assigned with the source profile that was used for
// autofill.
TEST_F(BrowserAutofillManagerTest, TrackFillingOrigin) {
  // Set up our form data.
  FormData form;
  test::CreateTestPersonalInformationFormData(&form);

  FormsSeen({form});

  AutofillProfile profile = test::GetFullProfile();
  personal_data().AddProfile(profile);
  FillAutofillFormData(form, form.fields[0],
                       MakeFrontendId({.profile_id = profile.guid()}));

  FormStructure* form_structure =
      browser_autofill_manager_->FindCachedFormById(form.global_id());
  ASSERT_TRUE(form_structure);
  for (const auto& autofill_field_ptr : form_structure->fields()) {
    EXPECT_THAT(autofill_field_ptr->autofill_source_profile_guid(),
                testing::Optional(profile.guid()));
  }
}

// Test that filling with multiple autofill profiles will set different source
// profiles for fields.
TEST_F(BrowserAutofillManagerTest,
       TrackFillingOriginWithUsingMultipleProfiles) {
  // Set up our form data.
  FormData form;
  test::CreateTestPersonalInformationFormData(&form);

  FormsSeen({form});

  // Fill the form with a profile without email
  AutofillProfile profile1 = test::GetFullProfile();
  profile1.ClearFields({EMAIL_ADDRESS});
  personal_data().AddProfile(profile1);
  FormData response_data;
  FillAutofillFormDataAndSaveResults(
      form, form.fields[0], MakeFrontendId({.profile_id = profile1.guid()}),
      &response_data);

  // Check that the email field has no filling source.
  FormStructure* form_structure =
      browser_autofill_manager_->FindCachedFormById(form.global_id());
  ASSERT_EQ(form.fields[3].label, u"Email");
  EXPECT_EQ(form_structure->field(3)->autofill_source_profile_guid(),
            absl::nullopt);

  // Then fill the email field using the second profile
  AutofillProfile profile2 = test::GetFullProfile();
  personal_data().AddProfile(profile2);
  FormData later_response_data;
  FillAutofillFormDataAndSaveResults(
      response_data, form.fields[3],
      MakeFrontendId({.profile_id = profile2.guid()}), &later_response_data);

  // Check that the first three fields have the first profile as filling source
  // and the last field has the second profile.
  form_structure =
      browser_autofill_manager_->FindCachedFormById(form.global_id());
  ASSERT_TRUE(form_structure);
  EXPECT_THAT(form_structure->field(0)->autofill_source_profile_guid(),
              testing::Optional(profile1.guid()));
  EXPECT_THAT(form_structure->field(1)->autofill_source_profile_guid(),
              testing::Optional(profile1.guid()));
  EXPECT_THAT(form_structure->field(2)->autofill_source_profile_guid(),
              testing::Optional(profile1.guid()));
  EXPECT_THAT(form_structure->field(3)->autofill_source_profile_guid(),
              testing::Optional(profile2.guid()));
}

// Test that an autofilled and edited field will be assigned with the autofill
// profile.
TEST_F(BrowserAutofillManagerTest, TrackFillingOriginOnEditedField) {
  // Set up our form data.
  FormData form;
  test::CreateTestPersonalInformationFormData(&form);

  FormsSeen({form});

  AutofillProfile profile = test::GetFullProfile();
  personal_data().AddProfile(profile);
  FormData response_data;
  FillAutofillFormDataAndSaveResults(
      form, form.fields[0], MakeFrontendId({.profile_id = profile.guid()}),
      &response_data);

  // Simulate editing the first field.
  response_data.fields[0].value = u"Michael";
  browser_autofill_manager_->OnTextFieldDidChange(
      response_data, response_data.fields[0], gfx::RectF(),
      AutofillTickClock::NowTicks());

  FormStructure* form_structure =
      browser_autofill_manager_->FindCachedFormById(form.global_id());
  ASSERT_TRUE(form_structure);
  AutofillField* edited_field = form_structure->field(0);
  ASSERT_FALSE(edited_field->is_autofilled);
  ASSERT_TRUE(edited_field->previously_autofilled());
  EXPECT_THAT(edited_field->autofill_source_profile_guid(),
              testing::Optional(profile.guid()));
}

// Test that only autofilled fields will be assigned with the autofill profile.
TEST_F(BrowserAutofillManagerTest, TrackFillingOriginWorksOnlyOnFilledField) {
  // Set up our form data.
  FormData form;
  test::CreateTestPersonalInformationFormData(&form);

  FormsSeen({form});

  // Fill the form with a profile without email field.
  AutofillProfile profile = test::GetFullProfile();
  profile.ClearFields({EMAIL_ADDRESS});
  personal_data().AddProfile(profile);
  FillAutofillFormData(form, form.fields[0],
                       MakeFrontendId({.profile_id = profile.guid()}));

  FormStructure* form_structure =
      browser_autofill_manager_->FindCachedFormById(form.global_id());
  ASSERT_TRUE(form_structure);
  // Check that the email field has no filling source.
  EXPECT_EQ(form_structure->field(3)->autofill_source_profile_guid(),
            absl::nullopt);
}

// Desktop only tests.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
class BrowserAutofillManagerTestForVirtualCardOption
    : public BrowserAutofillManagerTest {
 protected:
  BrowserAutofillManagerTestForVirtualCardOption() = default;
  ~BrowserAutofillManagerTestForVirtualCardOption() override = default;

  void SetUp() override {
    BrowserAutofillManagerTest::SetUp();

    // The URL should always match the form URL in
    // CreateTestCreditCardFormData() to have the allowlist work correctly.
    autofill_client_.set_allowed_merchants({"https://myform.com/form.html"});

    scoped_feature_list_.InitAndEnableFeature(
        features::kAutofillEnableVirtualCard);

    // Add only one server card so the second suggestion (if any) must be the
    // "Use a virtual card number" option.
    personal_data().ClearCreditCards();
    CreditCard masked_server_card(CreditCard::MASKED_SERVER_CARD,
                                  /*server_id=*/"a123");
    // TODO(crbug.com/1020740): Replace all the hard-coded expiration year in
    // this file with NextYear().
    test::SetCreditCardInfo(&masked_server_card, "Elvis Presley",
                            "4234567890123456",  // Visa
                            "04", "2999", "1");
    masked_server_card.SetNetworkForMaskedCard(kVisaCard);
    masked_server_card.set_guid("00000000-0000-0000-0000-000000000007");
    personal_data().AddServerCreditCard(masked_server_card);
  }

  FieldGlobalId CreateCompleteFormAndGetSuggestions() {
    FormData form;
    CreateTestCreditCardFormData(&form, /*is_https=*/true,
                                 /*use_month_type=*/false);
    FormsSeen({form});
    GetAutofillSuggestions(form, form.fields[1]);  // Card number field.
    return form.fields[1].global_id();
  }

  // Adds a CreditCardCloudTokenData to PersonalDataManager. This needs to be
  // called before suggestions are fetched.
  void CreateCloudTokenDataForDefaultCard() {
    personal_data().ClearCloudTokenData();
    CreditCardCloudTokenData data1 = test::GetCreditCardCloudTokenData1();
    data1.masked_card_id = "a123";
    personal_data().AddCloudTokenData(data1);
  }

  void VerifyNoVirtualCardSuggestions(FieldGlobalId field_id) {
    external_delegate_->CheckSuggestionCount(field_id, 1);
    // Suggestion details need to match the credit card added in the SetUp()
    // above.
    CheckSuggestions(
        field_id,
        Suggestion(
            std::string("Visa  ") + test::ObfuscatedCardDigitsAsUTF8("3456"),
            "Expires on 04/99", kVisaCard,
            browser_autofill_manager_->GetPackedCreditCardID(7)));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Ensures the "Use a virtual card number" option should not be shown when
// experiment is disabled.
TEST_F(BrowserAutofillManagerTestForVirtualCardOption,
       ShouldNotShowDueToExperimentDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAutofillEnableVirtualCard);
  FieldGlobalId field_id = CreateCompleteFormAndGetSuggestions();
  VerifyNoVirtualCardSuggestions(field_id);
}

// Ensures the "Use a virtual card number" option should not be shown when the
// preference for credit card upload is set to disabled.
TEST_F(BrowserAutofillManagerTestForVirtualCardOption,
       ShouldNotShowDueToCreditCardUploadPrefDisabled) {
  browser_autofill_manager_->SetAutofillCreditCardEnabled(autofill_client_,
                                                          false);
  FieldGlobalId field_id = CreateCompleteFormAndGetSuggestions();
  external_delegate_->CheckSuggestionCount(field_id, 0);
}

// Ensures the "Use a virtual card number" option should not be shown when
// merchant is not allowlisted.
TEST_F(BrowserAutofillManagerTestForVirtualCardOption,
       ShouldNotShowDueToMerchantNotAllowlisted) {
  // Adds a different URL in the allowlist.
  autofill_client_.set_allowed_merchants(
      {"https://myform.anotherallowlist.com/form.html"});
  FieldGlobalId field_id = CreateCompleteFormAndGetSuggestions();
  VerifyNoVirtualCardSuggestions(field_id);
}

// Ensures the "Use a virtual card number" option should not be shown when card
// number field is not detected.
TEST_F(BrowserAutofillManagerTestForVirtualCardOption,
       ShouldNotShowDueToFormNotHavingCardNumberField) {
  // Creates an incomplete form without card number field.
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  form.main_frame_origin =
      url::Origin::Create(GURL("https://myform_root.com/form.html"));
  FormFieldData field;
  test::CreateTestFormField("Name on Card", "nameoncard", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Expiration Date", "ccmonth", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("", "ccyear", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("CVC", "cvc", "", "text", &field);
  form.fields.push_back(field);

  FormsSeen({form});
  GetAutofillSuggestions(form, form.fields[0]);  // Cardholder name field.

  external_delegate_->CheckSuggestionCount(form.fields[0].global_id(), 1);
  const std::string visa_label =
      base::JoinString({"Visa  ", test::ObfuscatedCardDigitsAsUTF8("3456"),
                        ", expires on 04/99"},
                       "");
  CheckSuggestions(
      form.fields[0].global_id(),
      Suggestion("Elvis Presley", visa_label, kVisaCard,
                 browser_autofill_manager_->GetPackedCreditCardID(7)));
}

// Ensures the "Use a virtual card number" option should not be shown when there
// is no cloud token data for the card.
TEST_F(BrowserAutofillManagerTestForVirtualCardOption,
       ShouldNotShowDueToNoCloudTokenData) {
  FieldGlobalId field_id = CreateCompleteFormAndGetSuggestions();
  VerifyNoVirtualCardSuggestions(field_id);
}

// Ensures the "Use a virtual card number" option should not be shown when there
// is multiple cloud token data for the card.
TEST_F(BrowserAutofillManagerTestForVirtualCardOption,
       ShouldNotShowDueToMultipleCloudTokenData) {
  CreateCloudTokenDataForDefaultCard();
  CreditCardCloudTokenData data2 = test::GetCreditCardCloudTokenData2();
  data2.masked_card_id = "a123";
  personal_data().AddCloudTokenData(data2);
  FieldGlobalId field_id = CreateCompleteFormAndGetSuggestions();
  VerifyNoVirtualCardSuggestions(field_id);
}

// Ensures the "Use a virtual card number" option should not be shown when card
// expiration date field is not detected.
TEST_F(BrowserAutofillManagerTestForVirtualCardOption,
       ShouldNotShowDueToFormNotHavingExpirationDateField) {
  // Creates an incomplete form without expiration date field.
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  form.main_frame_origin =
      url::Origin::Create(GURL("https://myform_root.com/form.html"));
  FormFieldData field;
  test::CreateTestFormField("Name on Card", "nameoncard", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("CVC", "cvc", "", "text", &field);
  form.fields.push_back(field);

  FormsSeen({form});
  GetAutofillSuggestions(form, form.fields[1]);  // Card number field.

  VerifyNoVirtualCardSuggestions(form.fields[1].global_id());
}

// Ensures the "Use a virtual card number" option should not be shown when card
// cvc field is not detected.
TEST_F(BrowserAutofillManagerTestForVirtualCardOption,
       ShouldNotShowDueToFormNotHavingCvcField) {
  // Creates an incomplete form without cvc field.
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  form.main_frame_origin =
      url::Origin::Create(GURL("https://myform_root.com/form.html"));
  FormFieldData field;
  test::CreateTestFormField("Name on Card", "nameoncard", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Expiration Date", "ccmonth", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("", "ccyear", "", "text", &field);
  form.fields.push_back(field);

  FormsSeen({form});
  GetAutofillSuggestions(form, form.fields[1]);  // Card number field.

  VerifyNoVirtualCardSuggestions(form.fields[1].global_id());
}

// Ensures the "Use a virtual card number" option should be shown when all
// requirements are met.
TEST_F(BrowserAutofillManagerTestForVirtualCardOption,
       ShouldShowVirtualCardOption_OneCard) {
  CreateCloudTokenDataForDefaultCard();
  FieldGlobalId field_id = CreateCompleteFormAndGetSuggestions();

  // Ensures the card suggestion and the virtual card suggestion are shown.
  external_delegate_->CheckSuggestionCount(field_id, 2);
  CheckSuggestions(
      field_id,
      Suggestion(
          std::string("Visa  ") + test::ObfuscatedCardDigitsAsUTF8("3456"),
          "Expires on 04/99", kVisaCard,
          browser_autofill_manager_->GetPackedCreditCardID(7)),
      Suggestion(l10n_util::GetStringUTF8(
                     IDS_AUTOFILL_CLOUD_TOKEN_DROPDOWN_OPTION_LABEL),
                 "", "", PopupItemId::POPUP_ITEM_ID_USE_VIRTUAL_CARD));
}

// Ensures the "Use a virtual card number" option should be shown when there are
// multiple cards and at least one card meets requirements.
TEST_F(BrowserAutofillManagerTestForVirtualCardOption,
       ShouldShowVirtualCardOption_MultipleCards) {
  CreateCloudTokenDataForDefaultCard();

  // Adds another card which does not meet the requirements (has two cloud
  // tokens).
  CreditCard masked_server_card(CreditCard::MASKED_SERVER_CARD,
                                /*server_id=*/"a456");
  // TODO(crbug.com/1020740): Replace all the hard-coded expiration year in
  // this file with NextYear().
  test::SetCreditCardInfo(&masked_server_card, "Elvis Presley",
                          "4111111111111111",  // Visa
                          "04", "2999", "1");
  masked_server_card.SetNetworkForMaskedCard(kVisaCard);
  masked_server_card.set_guid("00000000-0000-0000-0000-000000000008");
  personal_data().AddServerCreditCard(masked_server_card);
  CreditCardCloudTokenData data1 = test::GetCreditCardCloudTokenData1();
  data1.masked_card_id = "a456";
  personal_data().AddCloudTokenData(data1);
  CreditCardCloudTokenData data2 = test::GetCreditCardCloudTokenData2();
  data2.masked_card_id = "a456";
  personal_data().AddCloudTokenData(data2);

  FieldGlobalId field_id = CreateCompleteFormAndGetSuggestions();

  // Ensures the card suggestion and the virtual card suggestion are shown.
  external_delegate_->CheckSuggestionCount(field_id, 3);
  CheckSuggestions(
      field_id,
      Suggestion(
          std::string("Visa  ") + test::ObfuscatedCardDigitsAsUTF8("1111"),
          "Expires on 04/99", kVisaCard,
          browser_autofill_manager_->GetPackedCreditCardID(8)),
      Suggestion(
          std::string("Visa  ") + test::ObfuscatedCardDigitsAsUTF8("3456"),
          "Expires on 04/99", kVisaCard,
          browser_autofill_manager_->GetPackedCreditCardID(7)),
      Suggestion(l10n_util::GetStringUTF8(
                     IDS_AUTOFILL_CLOUD_TOKEN_DROPDOWN_OPTION_LABEL),
                 "", "", PopupItemId::POPUP_ITEM_ID_USE_VIRTUAL_CARD));
}
#endif

// Test param indicates if there is an active screen reader.
class OnFocusOnFormFieldTest : public BrowserAutofillManagerTest,
                               public testing::WithParamInterface<bool> {
 protected:
  OnFocusOnFormFieldTest() = default;
  ~OnFocusOnFormFieldTest() override = default;

  void SetUp() override {
    BrowserAutofillManagerTest::SetUp();

    has_active_screen_reader_ = GetParam();
    external_delegate_->set_has_active_screen_reader(has_active_screen_reader_);
  }

  void TearDown() override {
    external_delegate_->set_has_active_screen_reader(false);
    BrowserAutofillManagerTest::TearDown();
  }

  void CheckSuggestionsAvailableIfScreenReaderRunning() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // The only existing functions for determining whether ChromeVox is in use
    // are in the src/chrome directory, which cannot be included in components.
    // Thus, if the platform is ChromeOS, we assume that ChromeVox is in use at
    // this point in the code.
    EXPECT_EQ(true,
              external_delegate_->has_suggestions_available_on_field_focus());
#else
    EXPECT_EQ(has_active_screen_reader_,
              external_delegate_->has_suggestions_available_on_field_focus());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }

  void CheckNoSuggestionsAvailableOnFieldFocus() {
    EXPECT_FALSE(
        external_delegate_->has_suggestions_available_on_field_focus());
  }

  bool has_active_screen_reader_;
};

TEST_P(OnFocusOnFormFieldTest, AddressSuggestions) {
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  FormFieldData field;
  // Set a valid autocomplete attribute for the first name.
  test::CreateTestFormField("First name", "firstname", "", "text", "given-name",
                            &field);
  form.fields.push_back(field);
  // Set an unrecognized autocomplete attribute for the last name.
  test::CreateTestFormField("Last Name", "lastname", "", "text", "unrecognized",
                            &field);
  form.fields.push_back(field);
  FormsSeen({form});

  // Suggestions should be returned for the first field.
  browser_autofill_manager_->OnFocusOnFormFieldImpl(form, form.fields[0],
                                                    gfx::RectF());
  CheckSuggestionsAvailableIfScreenReaderRunning();

  // No suggestions should be provided for the second field because of its
  // unrecognized autocomplete attribute.
  browser_autofill_manager_->OnFocusOnFormFieldImpl(form, form.fields[1],
                                                    gfx::RectF());
  CheckNoSuggestionsAvailableOnFieldFocus();
}

TEST_P(OnFocusOnFormFieldTest, AddressSuggestions_AutocompleteOffNotRespected) {
  FormData form;
  form.name = u"MyForm";
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  FormFieldData field;
  // Set a valid autocomplete attribute for the first name.
  test::CreateTestFormField("First name", "firstname", "", "text", "given-name",
                            &field);
  form.fields.push_back(field);
  // Set an autocomplete=off attribute for the last name.
  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  field.should_autocomplete = false;
  form.fields.push_back(field);
  FormsSeen({form});

  browser_autofill_manager_->OnFocusOnFormFieldImpl(form, form.fields[1],
                                                    gfx::RectF());
  CheckSuggestionsAvailableIfScreenReaderRunning();
}

TEST_P(OnFocusOnFormFieldTest, AddressSuggestions_Ablation) {
  base::test::ScopedFeatureList scoped_feature_list;
  DisableAutofillViaAblation(scoped_feature_list, /*for_addresses=*/true,
                             /*for_credit_cards=*/false);

  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  // Clear the form action.
  form.action = GURL();
  FormsSeen({form});

  browser_autofill_manager_->OnFocusOnFormFieldImpl(form, form.fields[1],
                                                    gfx::RectF());
  CheckNoSuggestionsAvailableOnFieldFocus();
}

TEST_P(OnFocusOnFormFieldTest, CreditCardSuggestions_SecureContext) {
  // Set up our form data.
  FormData form = CreateTestCreditCardFormData(true, false);
  // Clear the form action.
  form.action = GURL();
  FormsSeen({form});

  browser_autofill_manager_->OnFocusOnFormFieldImpl(form, form.fields[1],
                                                    gfx::RectF());
  CheckSuggestionsAvailableIfScreenReaderRunning();
}

TEST_P(OnFocusOnFormFieldTest, CreditCardSuggestions_NonSecureContext) {
  // Set up our form data.
  FormData form = CreateTestCreditCardFormData(false, false);
  // Clear the form action.
  form.action = GURL();
  FormsSeen({form});

  browser_autofill_manager_->OnFocusOnFormFieldImpl(form, form.fields[1],
                                                    gfx::RectF());
  // In a non-HTTPS context, there will be a warning indicating the page is
  // insecure.
  CheckSuggestionsAvailableIfScreenReaderRunning();
}

TEST_P(OnFocusOnFormFieldTest, CreditCardSuggestions_Ablation) {
  base::test::ScopedFeatureList scoped_feature_list;
  DisableAutofillViaAblation(scoped_feature_list, /*for_addresses=*/false,
                             /*for_credit_cards=*/true);

  // Set up our form data.
  FormData form = CreateTestCreditCardFormData(true, false);
  // Clear the form action.
  form.action = GURL();
  FormsSeen({form});

  browser_autofill_manager_->OnFocusOnFormFieldImpl(form, form.fields[1],
                                                    gfx::RectF());
  CheckNoSuggestionsAvailableOnFieldFocus();
}

INSTANTIATE_TEST_SUITE_P(BrowserAutofillManagerTest,
                         ProfileMatchingTypesTest,
                         testing::ValuesIn(kProfileMatchingTypesTestCases));

INSTANTIATE_TEST_SUITE_P(All, OnFocusOnFormFieldTest, testing::Bool());

#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
INSTANTIATE_TEST_SUITE_P(,
                         SuggestionMatchingTest,
                         testing::Values(std::make_tuple(0, ""),
                                         std::make_tuple(1, "show-all"),
                                         std::make_tuple(1, "show-one")));
#else
INSTANTIATE_TEST_SUITE_P(All,
                         SuggestionMatchingTest,
                         testing::Values(std::make_tuple(0, ""),
                                         std::make_tuple(1, "")));
#endif  // BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)

// The parameter indicates whether the AutofillKeyboardAccessory feature is
// enabled or disabled.
INSTANTIATE_TEST_SUITE_P(All, CreditCardSuggestionTest, testing::Bool());

struct ShareNicknameTestParam {
  std::string local_nickname;
  std::string server_nickname;
  std::string expected_nickname;
};

const ShareNicknameTestParam kShareNicknameTestParam[] = {
    {"", "", ""},
    {"", "server nickname", "server nickname"},
    {"local nickname", "", "local nickname"},
    {"local nickname", "server nickname", "local nickname"},
};

class BrowserAutofillManagerTestForSharingNickname
    : public BrowserAutofillManagerTest,
      public testing::WithParamInterface<ShareNicknameTestParam> {
 public:
  BrowserAutofillManagerTestForSharingNickname()
      : local_nickname_(GetParam().local_nickname),
        server_nickname_(GetParam().server_nickname),
        expected_nickname_(GetParam().expected_nickname) {}

  CreditCard GetLocalCard() {
    CreditCard local_card("287151C8-6AB1-487C-9095-28E80BE5DA15",
                          test::kEmptyOrigin);
    test::SetCreditCardInfo(&local_card, "Clyde Barrow",
                            "378282246310005" /* American Express */, "04",
                            "2999", "1");
    local_card.set_use_count(3);
    local_card.set_use_date(AutofillClock::Now() - base::Days(1));
    local_card.SetNickname(base::UTF8ToUTF16(local_nickname_));
    local_card.set_guid("00000000-0000-0000-0000-000000000001");
    return local_card;
  }

  CreditCard GetServerCard() {
    CreditCard full_server_card(CreditCard::FULL_SERVER_CARD, "c789");
    test::SetCreditCardInfo(&full_server_card, "Clyde Barrow",
                            "378282246310005" /* American Express */, "04",
                            "2999", "1");
    full_server_card.SetNickname(base::UTF8ToUTF16(server_nickname_));
    full_server_card.set_guid("00000000-0000-0000-0000-000000000002");
    return full_server_card;
  }

  std::string local_nickname_;
  std::string server_nickname_;
  std::string expected_nickname_;
};

INSTANTIATE_TEST_SUITE_P(,
                         BrowserAutofillManagerTestForSharingNickname,
                         testing::ValuesIn(kShareNicknameTestParam));

TEST_P(BrowserAutofillManagerTestForSharingNickname,
       VerifySuggestion_DuplicateCards) {
  personal_data().ClearCreditCards();
  ASSERT_EQ(0U, personal_data().GetCreditCards().size());
  CreditCard local_card = GetLocalCard();
  personal_data().AddCreditCard(local_card);
  personal_data().AddServerCreditCard(GetServerCard());
  ASSERT_EQ(2U, personal_data().GetCreditCards().size());

  // Set up our form data.
  FormData form = CreateTestCreditCardFormData(true, false);
  FormsSeen({form});

  // Query by card number field.
  GetAutofillSuggestions(form, form.fields[1]);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  const std::string exp_label = std::string("04/99");
#else
  const std::string exp_label = std::string("Expires on 04/99");
#endif

  CheckSuggestions(
      form.fields[1].global_id(),
      Suggestion((expected_nickname_.empty() ? std::string("Amex")
                                             : expected_nickname_) +
                     "  " + test::ObfuscatedCardDigitsAsUTF8("0005"),
                 exp_label, kAmericanExpressCard,
                 browser_autofill_manager_->GetPackedCreditCardID(2)));
}

TEST_P(BrowserAutofillManagerTestForSharingNickname,
       VerifySuggestion_UnrelatedCards) {
  personal_data().ClearCreditCards();
  ASSERT_EQ(0U, personal_data().GetCreditCards().size());
  CreditCard local_card = GetLocalCard();
  personal_data().AddCreditCard(local_card);

  std::vector<CreditCard> server_cards;
  CreditCard server_card = GetServerCard();
  // Make sure the cards are different by giving a different card number.
  server_card.SetNumber(u"371449635398431");
  personal_data().AddServerCreditCard(server_card);

  ASSERT_EQ(2U, personal_data().GetCreditCards().size());

  // Set up our form data.
  FormData form = CreateTestCreditCardFormData(true, false);
  FormsSeen({form});

  // Query by card number field.
  GetAutofillSuggestions(form, form.fields[1]);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  const std::string exp_label = std::string("04/99");
#else
  const std::string exp_label = std::string("Expires on 04/99");
#endif

  CheckSuggestions(
      form.fields[1].global_id(),
      Suggestion(
          (local_nickname_.empty() ? std::string("Amex") : local_nickname_) +
              "  " + test::ObfuscatedCardDigitsAsUTF8("0005"),
          exp_label, kAmericanExpressCard,
          browser_autofill_manager_->GetPackedCreditCardID(1)),
      Suggestion(
          (server_nickname_.empty() ? std::string("Amex") : server_nickname_) +
              "  " + test::ObfuscatedCardDigitsAsUTF8("8431"),
          exp_label, kAmericanExpressCard,
          browser_autofill_manager_->GetPackedCreditCardID(2)));
}

// The following Refill Tests ensure that Autofill can handle the situation
// where it fills a credit card form with an expiration date like 04/2999
// and the website tries to reformat the input with whitespaces around the
// slash and then sacrifices the wrong digits in the expiration date. I.e.,
// the website replaces "04/2099" with "04 / 20". The tests ensure that this
// triggers a refill with "04 / 29".
struct RefillTestCase {
  // The value that JavaScript owned by the website sets for the expiration
  // date filed.
  std::u16string exp_date_from_js;
  // Whether we expect a refill from in this test case.
  bool triggers_refill;
  // What value we expect in the refill.
  const char* refilled_exp_date = nullptr;
};

class BrowserAutofillManagerRefillTest
    : public BrowserAutofillManagerTest,
      public testing::WithParamInterface<RefillTestCase> {};

TEST_P(BrowserAutofillManagerRefillTest,
       RefillModifiedCreditCardExpirationDates) {
  RefillTestCase test_case = GetParam();

  // Set up a CC form with name, cc number and expiration date.
  FormData form;
  form.url = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  FormFieldData field;
  test::CreateTestFormField("Name on Card", "nameoncard", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Expiration date", "exp_date", "", "text", &field);
  form.fields.push_back(field);

  // Notify BrowserAutofillManager of the form.
  FormsSeen({form});

  // Simulate filling and store the data to be filled in |first_fill_data|.
  const char guid[] = "00000000-0000-0000-0000-000000000004";
  FormData first_fill_data;
  FillAutofillFormDataAndSaveResults(form, *form.fields.begin(),
                                     MakeFrontendId({.credit_card_id = guid}),
                                     &first_fill_data);
  ASSERT_EQ(3u, first_fill_data.fields.size());
  ExpectFilledField("Name on Card", "nameoncard", "Elvis Presley", "text",
                    first_fill_data.fields[0]);
  ExpectFilledField("Card Number", "cardnumber", "4234567890123456", "text",
                    first_fill_data.fields[1]);
  ExpectFilledField("Expiration date", "exp_date", "04/2999", "text",
                    first_fill_data.fields[2]);

  FormData refilled_form;
  if (test_case.triggers_refill) {
    // Prepare intercepting the filling operation to the driver and capture
    // the re-filled form data.
    EXPECT_CALL(*autofill_driver_, FillOrPreviewForm(_, _, _, _))
        .Times(1)
        .WillOnce(DoAll(testing::SaveArg<1>(&refilled_form),
                        testing::Return(std::vector<FieldGlobalId>{})));
  } else {
    EXPECT_CALL(*autofill_driver_, FillOrPreviewForm(_, _, _, _)).Times(0);
  }
  // Simulate that JavaScript modifies the expiration date field.
  FormData form_after_js_modification = first_fill_data;
  form_after_js_modification.fields[2].value = test_case.exp_date_from_js;
  browser_autofill_manager_->OnJavaScriptChangedAutofilledValue(
      form_after_js_modification, form_after_js_modification.fields[2],
      u"04/2999");

  testing::Mock::VerifyAndClearExpectations(autofill_driver_.get());

  if (test_case.triggers_refill) {
    ASSERT_EQ(3u, refilled_form.fields.size());
    ExpectFilledField("Name on Card", "nameoncard", "Elvis Presley", "text",
                      refilled_form.fields[0]);
    EXPECT_FALSE(refilled_form.fields[0].force_override);
    ExpectFilledField("Card Number", "cardnumber", "4234567890123456", "text",
                      refilled_form.fields[1]);
    EXPECT_FALSE(refilled_form.fields[1].force_override);
    ExpectFilledField("Expiration date", "exp_date",
                      test_case.refilled_exp_date, "text",
                      refilled_form.fields[2]);
    EXPECT_TRUE(refilled_form.fields[2].force_override);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    BrowserAutofillManagerRefillTest,
    testing::Values(
        // This is the classic case: Autofill filled 04/2999, website overrode
        // 04 / 29, we need to fix this to 04 / 99.
        RefillTestCase{.exp_date_from_js = u"04 / 29",
                       .triggers_refill = true,
                       .refilled_exp_date = "04 / 99"},
        // Maybe the website replaced the separator and added whitespaces.
        RefillTestCase{.exp_date_from_js = u"04 - 29",
                       .triggers_refill = true,
                       .refilled_exp_date = "04 - 99"},
        // Maybe the website only replaced the separator.
        RefillTestCase{.exp_date_from_js = u"04-29",
                       .triggers_refill = true,
                       .refilled_exp_date = "04-99"},
        // Maybe the website was smart and dropped the correct digits.
        RefillTestCase{
            .exp_date_from_js = u"04 / 99",
            .triggers_refill = false,
        },
        // Maybe the website did not modify the values at all.
        RefillTestCase{
            .exp_date_from_js = u"04/2999",
            .triggers_refill = false,
        },
        // Maybe the website did something we don't support.
        RefillTestCase{
            .exp_date_from_js = u"April / 2999",
            .triggers_refill = false,
        },
        // Maybe the website just added some whitespaces.
        RefillTestCase{
            .exp_date_from_js = u"04 / 2999",
            .triggers_refill = false,
        },
        // Don't trigger refill on 3 digit years.
        RefillTestCase{
            .exp_date_from_js = u"04 / 299",
            .triggers_refill = false,
        }));

// Tests that analyze metrics logging in case JavaScript clears a field
// immediately after it was filled.
class BrowserAutofillManagerClearFieldTest : public BrowserAutofillManagerTest {
 public:
  void SetUp() override {
    BrowserAutofillManagerTest::SetUp();

    // Set up a CC form.
    FormData form;
    form.url = GURL("https://myform.com/form.html");
    form.action = GURL("https://myform.com/submit.html");
    FormFieldData field;
    test::CreateTestFormField("Name on Card", "nameoncard", "", "text", &field);
    form.fields.push_back(field);
    test::CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
    form.fields.push_back(field);
    test::CreateTestFormField("Expiration date", "exp_date", "", "text",
                              &field);
    form.fields.push_back(field);

    // Notify BrowserAutofillManager of the form.
    FormsSeen({form});

    // Simulate filling and store the data to be filled in `fill_data_`.
    const char guid[] = "00000000-0000-0000-0000-000000000004";
    FillAutofillFormDataAndSaveResults(form, *form.fields.begin(),
                                       MakeFrontendId({.credit_card_id = guid}),
                                       &fill_data_);
    ASSERT_EQ(3u, fill_data_.fields.size());
    ExpectFilledField("Name on Card", "nameoncard", "Elvis Presley", "text",
                      fill_data_.fields[0]);
    ExpectFilledField("Card Number", "cardnumber", "4234567890123456", "text",
                      fill_data_.fields[1]);
    ExpectFilledField("Expiration date", "exp_date", "04/2999", "text",
                      fill_data_.fields[2]);
  }

  void SimulateOverrideFieldByJavaScript(size_t field_index,
                                         const std::u16string& new_value) {
    std::u16string old_value = fill_data_.fields[field_index].value;
    fill_data_.fields[field_index].value = new_value;
    browser_autofill_manager_->OnJavaScriptChangedAutofilledValue(
        fill_data_, fill_data_.fields[field_index], old_value);
  }

  // Content of the form.
  FormData fill_data_;

  base::HistogramTester histogram_tester_;

  // Shorter alias of the Autofill.FormEvents we are interested in.
  const autofill_metrics::FormEvent kEvent = autofill_metrics::
      FORM_EVENT_AUTOFILLED_FIELD_CLEARED_BY_JAVASCRIPT_AFTER_FILL_ONCE;
};

// Ensure that we log the appropriate Autofill.FormEvents event if an autofilled
// field is cleared by JavaScript immediately after the filling.
TEST_F(BrowserAutofillManagerClearFieldTest, OneClearedField) {
  // Simulate that JavaScript clears the first field.
  SimulateOverrideFieldByJavaScript(0, u"");

  EXPECT_THAT(histogram_tester_.GetAllSamples("Autofill.FormEvents.CreditCard"),
              base::BucketsInclude(base::Bucket(kEvent, 1)));
}

// Ensure that we log a single Autofill.FormEvents event even if *two*
// autofilled fields are cleared by JavaScript immediately after the filling.
TEST_F(BrowserAutofillManagerClearFieldTest, TwoClearedFields) {
  // Simulate that JavaScript clears the first two field.
  SimulateOverrideFieldByJavaScript(0, u"");
  SimulateOverrideFieldByJavaScript(1, u"");

  EXPECT_THAT(histogram_tester_.GetAllSamples("Autofill.FormEvents.CreditCard"),
              base::BucketsInclude(base::Bucket(kEvent, 1)));
}

// Ensure that we do not log an Autofill.FormEvents event for the case that
// JavaScript modifies an autofilled field but does not clear it.
TEST_F(BrowserAutofillManagerClearFieldTest, ModifiedButDidNotClearField) {
  // Simulate that JavaScript modifies the value of the field but does not clear
  // the field.
  SimulateOverrideFieldByJavaScript(0, u"Elvis Aaron Presley");

  EXPECT_THAT(histogram_tester_.GetAllSamples("Autofill.FormEvents.CreditCard"),
              base::BucketsInclude(base::Bucket(kEvent, 0)));
}

// Ensure that we do not log an appropriate Autofill.FormEvents event if an
// autofilled field is cleared by JavaScript too long after it was filled.
TEST_F(BrowserAutofillManagerClearFieldTest, NoLoggingAfterDelay) {
  TestAutofillTickClock clock(AutofillTickClock::NowTicks());
  clock.Advance(base::Seconds(5));

  // Simulate that JavaScript clears the first field.
  SimulateOverrideFieldByJavaScript(0, u"");

  EXPECT_THAT(histogram_tester_.GetAllSamples("Autofill.FormEvents.CreditCard"),
              base::BucketsInclude(base::Bucket(kEvent, 0)));
}

class BrowserAutofillManagerVotingTest
    : public BrowserAutofillManagerTest,
      public testing::WithParamInterface<bool> {
 public:
  BrowserAutofillManagerVotingTest() {
    features_.InitWithFeatureState(features::kAutofillDelayBlurVotes,
                                   GetParam());
  }

  void SetUp() override {
    BrowserAutofillManagerTest::SetUp();

    // All uploads should be expected explicitly.
    EXPECT_CALL(*download_manager_, StartUploadRequest(_, _, _, _, _, _, _))
        .Times(0);

    form_.name = u"MyForm";
    form_.url = GURL("https://myform.com/form.html");
    form_.action = GURL("https://myform.com/submit.html");
    form_.fields.resize(2);
    test::CreateTestFormField("First Name", "firstname", "", "text",
                              "given-name", &form_.fields[0]);
    test::CreateTestFormField("Last Name", "lastname", "", "text",
                              "family-name", &form_.fields[1]);

    // Set up our form data.
    FormsSeen({form_});
  }

  void SimulateTypingFirstNameIntoFirstField() {
    form_.fields[0].value = u"Elvis";
    browser_autofill_manager_->OnTextFieldDidChange(
        form_, form_.fields[0], gfx::RectF(), AutofillTickClock::NowTicks());
  }

 protected:
  base::test::ScopedFeatureList features_;
  FormData form_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         BrowserAutofillManagerVotingTest,
                         testing::Bool());

// Ensure that a vote is submitted after a regular form submission.
TEST_P(BrowserAutofillManagerVotingTest, Submission) {
  SimulateTypingFirstNameIntoFirstField();

  std::map<std::u16string, ServerFieldTypeSet> expected_vote_types = {
      {u"firstname",
       {ServerFieldType::NAME_FIRST, ServerFieldType::CREDIT_CARD_NAME_FIRST}},
      {u"lastname", {ServerFieldType::EMPTY_TYPE}},
  };

  // Ensure that vote is submitted after form submission.
  EXPECT_CALL(
      *download_manager_,
      StartUploadRequest(AllOf(SignatureIs(CalculateFormSignature(form_)),
                               UploadedAutofillTypesAre(expected_vote_types)),
                         _, _, _, /*observed_submission=*/true, _, _))
      .Times(1);
  FormSubmitted(form_);
}

// Test that when modifying the form, a blur vote can be sent for the early
// version and a submission vote can be sent for the final version.
TEST_P(BrowserAutofillManagerVotingTest, DynamicFormSubmission) {
  // 1. Simulate typing.
  SimulateTypingFirstNameIntoFirstField();

  // 2. Simulate removing focus from the form, which triggers a blur vote.
  FormSignature first_form_signature = CalculateFormSignature(form_);
  if (!base::FeatureList::IsEnabled(features::kAutofillDelayBlurVotes)) {
    // Only if kAutofillDelayBlurVotes is disabled, we expect a blur vote being
    // sent. If kAutofillDelayBlurVotes is enabled, the next two operations
    // will override this vote.
    std::map<std::u16string, ServerFieldTypeSet> expected_vote_types = {
        {u"firstname",
         {ServerFieldType::NAME_FIRST,
          ServerFieldType::CREDIT_CARD_NAME_FIRST}},
        {u"lastname", {ServerFieldType::EMPTY_TYPE}},
    };
    EXPECT_CALL(
        *download_manager_,
        StartUploadRequest(AllOf(SignatureIs(first_form_signature),
                                 UploadedAutofillTypesAre(expected_vote_types)),
                           _, _, _, /*observed_submission=*/false, _, _))
        .Times(1);
  }
  browser_autofill_manager_->OnFocusNoLongerOnForm(true);

  // 3. Simulate typing into second field
  form_.fields[1].value = u"Presley";
  browser_autofill_manager_->OnTextFieldDidChange(
      form_, form_.fields[1], gfx::RectF(), AutofillTickClock::NowTicks());

  // 4. Simulate removing the focus from the form, which generates a second blur
  // vote which should be sent.
  std::map<std::u16string, ServerFieldTypeSet> expected_vote_types = {
      {u"firstname",
       {ServerFieldType::NAME_FIRST, ServerFieldType::CREDIT_CARD_NAME_FIRST}},
      {u"lastname",
       {ServerFieldType::NAME_LAST, ServerFieldType::CREDIT_CARD_NAME_LAST,
        ServerFieldType::NAME_LAST_SECOND}},
  };
  EXPECT_CALL(
      *download_manager_,
      StartUploadRequest(AllOf(SignatureIs(first_form_signature),
                               UploadedAutofillTypesAre(expected_vote_types)),
                         _, _, _, /*observed_submission=*/false, _, _))
      .Times(1);
  browser_autofill_manager_->OnFocusNoLongerOnForm(true);

  // 5. Grow the form by one field, which changes the form signature.
  FormFieldData field;
  test::CreateTestFormField("Zip code", "zip", "", "text", "postal-code",
                            &field);
  form_.fields.push_back(field);
  FormsSeen({form_});

  // 6. Ensure that a form submission triggers votes for the new form.
  // Adding a field should have changed the form signature.
  FormSignature second_form_signature = CalculateFormSignature(form_);
  EXPECT_NE(first_form_signature, second_form_signature);
  // Because the next field after the two names is not a credit card field,
  // field disambiguation removes the credit card name votes.
  expected_vote_types = {
      {u"firstname", {ServerFieldType::NAME_FIRST}},
      {u"lastname",
       {ServerFieldType::NAME_LAST, ServerFieldType::NAME_LAST_SECOND}},
      {u"zip", {ServerFieldType::EMPTY_TYPE}},
  };
  EXPECT_CALL(
      *download_manager_,
      StartUploadRequest(AllOf(SignatureIs(second_form_signature),
                               UploadedAutofillTypesAre(expected_vote_types)),
                         _, _, _,
                         /*observed_submission=*/true, _, _))
      .Times(1);
  FormSubmitted(form_);
}

// Ensure that a blur votes is sent after a navigation.
TEST_P(BrowserAutofillManagerVotingTest, BlurVoteOnNavigation) {
  SimulateTypingFirstNameIntoFirstField();

  // Simulate removing focus from form, which triggers a blur vote.
  std::map<std::u16string, ServerFieldTypeSet> expected_vote_types = {
      {u"firstname",
       {ServerFieldType::NAME_FIRST, ServerFieldType::CREDIT_CARD_NAME_FIRST}},
      {u"lastname", {ServerFieldType::EMPTY_TYPE}},
  };
  EXPECT_CALL(
      *download_manager_,
      StartUploadRequest(AllOf(SignatureIs(CalculateFormSignature(form_)),
                               UploadedAutofillTypesAre(expected_vote_types)),
                         _, _, _, /*observed_submission=*/false, _, _))
      .Times(1);
  browser_autofill_manager_->OnFocusNoLongerOnForm(true);

  // Simulate a navigation. If kAutofillDelayBlurVotes is enabled, this is when
  // the vote is sent.
  browser_autofill_manager_->Reset();
}

// Ensure that a submission vote blocks sending a blur vote for the same form
// signature.
TEST_P(BrowserAutofillManagerVotingTest, NoBlurVoteOnSubmission) {
  SimulateTypingFirstNameIntoFirstField();

  std::map<std::u16string, ServerFieldTypeSet> expected_vote_types = {
      {u"firstname",
       {ServerFieldType::NAME_FIRST, ServerFieldType::CREDIT_CARD_NAME_FIRST}},
      {u"lastname", {ServerFieldType::EMPTY_TYPE}},
  };

  // Simulate removing focus from form, which enqueues a blur vote.
  if (!base::FeatureList::IsEnabled(features::kAutofillDelayBlurVotes)) {
    // If AutofillDelayBlurVotes is disabled, the blur vote is actually sent.
    // Otherwise, it is overridden by the form submission vote.
    EXPECT_CALL(
        *download_manager_,
        StartUploadRequest(AllOf(SignatureIs(CalculateFormSignature(form_)),
                                 UploadedAutofillTypesAre(expected_vote_types)),
                           _, _, _, /*observed_submission=*/false, _, _))
        .Times(1);
  } else {
    // If kAutofillDelayBlurVotes is enabled, the blur vote will be ignored and
    // only the submission will be sent.
  }
  browser_autofill_manager_->OnFocusNoLongerOnForm(true);

  EXPECT_CALL(
      *download_manager_,
      StartUploadRequest(AllOf(SignatureIs(CalculateFormSignature(form_)),
                               UploadedAutofillTypesAre(expected_vote_types)),
                         _, _, _, /*observed_submission=*/true, _, _))
      .Times(1);
  FormSubmitted(form_);
}

}  // namespace autofill
