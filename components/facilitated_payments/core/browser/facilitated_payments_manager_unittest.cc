// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/facilitated_payments_manager.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/bank_account.h"
#include "components/autofill/core/browser/test_payments_data_manager.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/facilitated_payments/core/browser/ewallet_manager.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_client.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_driver.h"
#include "components/facilitated_payments/core/browser/mock_facilitated_payments_api_client.h"
#include "components/facilitated_payments/core/browser/mock_facilitated_payments_client.h"
#include "components/facilitated_payments/core/browser/network_api/mock_facilitated_payments_network_interface.h"
#include "components/facilitated_payments/core/features/features.h"
#include "components/facilitated_payments/core/metrics/facilitated_payments_metrics.h"
#include "components/facilitated_payments/core/ui_utils/facilitated_payments_ui_utils.h"
#include "components/optimization_guide/core/mock_optimization_guide_decider.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/test/test_sync_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments::facilitated {
namespace {

// Returns a bank account enabled for Pix with fake data.
autofill::BankAccount CreatePixBankAccount(int64_t instrument_id) {
  autofill::BankAccount bank_account(
      instrument_id, u"nickname", GURL("http://www.example.com"), u"bank_name",
      u"account_number", autofill::BankAccount::AccountType::kChecking);
  return bank_account;
}

// Returns an account info that has all the details a logged in account should
// have.
CoreAccountInfo CreateLoggedInAccountInfo() {
  CoreAccountInfo account;
  account.email = "foo@bar.com";
  account.gaia = "foo-gaia-id";
  account.account_id = CoreAccountId::FromGaiaId(account.gaia);
  return account;
}

}  // namespace

class MockFacilitatedPaymentsDriver : public FacilitatedPaymentsDriver {
 public:
  explicit MockFacilitatedPaymentsDriver(
      std::unique_ptr<FacilitatedPaymentsManager> manager,
      std::unique_ptr<EwalletManager> ewallet_manager)
      : FacilitatedPaymentsDriver(std::move(manager),
                                  std::move(ewallet_manager)) {}
  ~MockFacilitatedPaymentsDriver() override = default;

  MOCK_METHOD(void,
              TriggerPixCodeDetection,
              (base::OnceCallback<void(mojom::PixCodeDetectionResult,
                                       const std::string&)>),
              (override));
};

class FacilitatedPaymentsManagerTest : public testing::Test {
 public:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  void SetUp() override {
    optimization_guide_decider_ =
        std::make_unique<optimization_guide::MockOptimizationGuideDecider>();
    driver_ = std::make_unique<MockFacilitatedPaymentsDriver>(nullptr, nullptr);
    client_ = std::make_unique<MockFacilitatedPaymentsClient>();

    manager_ = std::make_unique<FacilitatedPaymentsManager>(
        driver_.get(), client_.get(), /*api_client_creator=*/
        base::BindOnce(&MockFacilitatedPaymentsApiClient::CreateApiClient),
        optimization_guide_decider_.get());

    // Using Autofill preferences since we use autofill's infra for syncing
    // bank accounts.
    pref_service_ = autofill::test::PrefServiceForTesting();
    payments_data_manager_ =
        std::make_unique<autofill::TestPaymentsDataManager>();
    payments_data_manager_->SetPrefService(pref_service_.get());
    payments_data_manager_->SetSyncServiceForTest(&sync_service_);
    ON_CALL(*client_, GetPaymentsDataManager)
        .WillByDefault(testing::Return(payments_data_manager_.get()));
    ON_CALL(*client_, GetFacilitatedPaymentsNetworkInterface)
        .WillByDefault(testing::Return(&payments_network_interface_));
    ON_CALL(*client_, IsInLandscapeMode).WillByDefault(testing::Return(false));
  }

  void TearDown() override {
    payments_data_manager_->ClearAllServerDataForTesting();
    payments_data_manager_.reset();
  }

  void FastForwardBy(base::TimeDelta duration) {
    task_environment_.FastForwardBy(duration);
    task_environment_.RunUntilIdle();
  }

  MockFacilitatedPaymentsApiClient& GetApiClient() {
    return *static_cast<MockFacilitatedPaymentsApiClient*>(
        manager_->GetApiClient());
  }

 protected:
  std::unique_ptr<optimization_guide::MockOptimizationGuideDecider>
      optimization_guide_decider_;
  std::unique_ptr<MockFacilitatedPaymentsDriver> driver_;
  std::unique_ptr<MockFacilitatedPaymentsClient> client_;
  std::unique_ptr<FacilitatedPaymentsManager> manager_;
  std::unique_ptr<PrefService> pref_service_;
  std::unique_ptr<autofill::TestPaymentsDataManager> payments_data_manager_;
  MockFacilitatedPaymentsNetworkInterface payments_network_interface_;
  ukm::TestAutoSetUkmRecorder ukm_recorder_;

 private:
  syncer::TestSyncService sync_service_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
};

// Test that the `PIX_PAYMENT_MERCHANT_ALLOWLIST` optimization type is
// registered when RegisterPixOptimizationGuide is called.
TEST_F(FacilitatedPaymentsManagerTest, RegisterPixAllowlist) {
  EXPECT_CALL(*optimization_guide_decider_,
              RegisterOptimizationTypes(testing::ElementsAre(
                  optimization_guide::proto::PIX_MERCHANT_ORIGINS_ALLOWLIST)))
      .Times(1);

  manager_->RegisterPixAllowlist();
}

// If the facilitated payment API is not available, then the manager does not
// show the Pix payment prompt.
TEST_F(FacilitatedPaymentsManagerTest,
       NoPixPaymentPromptWhenApiClientNotAvailable) {
  payments_data_manager_->AddMaskedBankAccountForTest(
      CreatePixBankAccount(/*instrument_id=*/1));
  payments_data_manager_->AddMaskedBankAccountForTest(
      CreatePixBankAccount(/*instrument_id=*/2));

  EXPECT_CALL(*client_, ShowPixPaymentPrompt(testing::_, testing::_)).Times(0);

  manager_->OnApiAvailabilityReceived(false);
}

// If the facilitated payment API is available, then the manager shows the PIX
// payment prompt.
TEST_F(FacilitatedPaymentsManagerTest,
       ShowsPixPaymentPromptWhenApiClientAvailable) {
  autofill::BankAccount pix_account1 =
      CreatePixBankAccount(/*instrument_id=*/1);
  autofill::BankAccount pix_account2 =
      CreatePixBankAccount(/*instrument_id=*/2);
  payments_data_manager_->AddMaskedBankAccountForTest(pix_account1);
  payments_data_manager_->AddMaskedBankAccountForTest(pix_account2);

  EXPECT_CALL(*client_, ShowPixPaymentPrompt(testing::UnorderedElementsAreArray(
                                                 {pix_account1, pix_account2}),
                                             testing::_));

  manager_->OnApiAvailabilityReceived(true);
}

// If the user does not select a payment account on the payment prompt,
// 1. Request for risk data is not made.
// 2. Progress screen is not shown.
// 3. Histogram is not logged.
TEST_F(FacilitatedPaymentsManagerTest,
       OnPixPaymentPromptResult_FopSelectorDeclined) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*client_, ShowProgressScreen()).Times(0);
  EXPECT_CALL(*client_, LoadRiskData(testing::_)).Times(0);

  manager_->OnPixPaymentPromptResult(/*is_prompt_accepted=*/false,
                                     /*selected_instrument_id=*/0);

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.FopSelector.UserAction",
      /*sample=*/FopSelectorAction::kFopSelected,
      /*expected_bucket_count=*/0);
}

// If the user selects a payment account on the payment prompt,
// 1. Request for risk data is made.
// 2. Progress screen is shown.
// 3. Histogram is logged.
TEST_F(FacilitatedPaymentsManagerTest, OnPixPaymentPromptResult_FopSelected) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*client_, ShowProgressScreen());
  EXPECT_CALL(*client_, LoadRiskData(testing::_));

  manager_->OnPixPaymentPromptResult(/*is_prompt_accepted=*/true,
                                     /*selected_instrument_id=*/0);

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.FopSelector.UserAction",
      /*sample=*/FopSelectorAction::kFopSelected,
      /*expected_bucket_count=*/1);

  auto ukm_entries = ukm_recorder_.GetEntries(
      ukm::builders::FacilitatedPayments_Pix_FopSelectorResult::kEntryName,
      {ukm::builders::FacilitatedPayments_Pix_FopSelectorResult::kResultName});
  ASSERT_EQ(ukm_entries.size(), 1UL);
  EXPECT_EQ(ukm_entries[0].metrics.at("Result"), true);
}

// Verify risk data metrics are logged when risk data is fetched successfully.
TEST_F(FacilitatedPaymentsManagerTest, RiskDataNotEmpty_HistogramsLogged) {
  base::HistogramTester histogram_tester;

  manager_->OnRiskDataLoaded(base::TimeTicks::Now() - base::Seconds(2),
                             "seems pretty risky");

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.LoadRiskData.Success.Latency",
      /*sample=*/2000,
      /*expected_bucket_count=*/1);
}

// Verify risk data metrics are logged when risk data is empty.
TEST_F(FacilitatedPaymentsManagerTest, RiskDataEmpty_HistogramsLogged) {
  base::HistogramTester histogram_tester;

  manager_->OnRiskDataLoaded(base::TimeTicks::Now() - base::Seconds(2), "");

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.LoadRiskData.Failure.Latency",
      /*sample=*/2000,
      /*expected_bucket_count=*/1);
}

// If the risk data is empty, then the PayflowExitedReason histogram should
// be logged.
TEST_F(FacilitatedPaymentsManagerTest, PayflowExitedReason_RiskDataEmpty) {
  base::HistogramTester histogram_tester;

  manager_->OnRiskDataLoaded(base::TimeTicks::Now(), "");

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PayflowExitedReason",
      /*sample=*/PayflowExitedReason::kRiskDataNotAvailable,
      /*expected_bucket_count=*/1);
}

// If the risk data is empty, then the manager does not retrieve a client token
// from the facilitated payments API client.
TEST_F(FacilitatedPaymentsManagerTest,
       RiskDataEmpty_GetClientTokenNotCalled_ErrorScreenShown) {
  EXPECT_CALL(GetApiClient(), GetClientToken(testing::_)).Times(0);
  EXPECT_CALL(*client_, ShowErrorScreen());

  manager_->OnRiskDataLoaded(/*start_time=*/base::TimeTicks::Now(),
                             /*risk_data=*/"");
}

// If the risk data is not empty, then the manager retrieves a client token from
// the facilitated payments API client.
TEST_F(FacilitatedPaymentsManagerTest, RiskDataNotEmpty_GetClientTokenCalled) {
  EXPECT_CALL(GetApiClient(), GetClientToken(testing::_));

  manager_->OnRiskDataLoaded(/*start_time=*/base::TimeTicks::Now(),
                             /*risk_data=*/"seems pretty risky");
}

// The GetClientToken async call is made after fetching the risk data. This test
// verifies that the result and latency of the GetClientToken call is logged
// correctly.
TEST_F(FacilitatedPaymentsManagerTest, LogGetClientTokenResultAndLatency) {
  for (bool get_client_token_result : {true, false}) {
    base::HistogramTester histogram_tester;
    EXPECT_CALL(GetApiClient(), GetClientToken(testing::_));
    manager_->OnRiskDataLoaded(/*start_time=*/base::TimeTicks::Now(),
                               /*risk_data=*/"seems pretty risky");
    FastForwardBy(base::Seconds(2));

    manager_->OnGetClientToken(
        get_client_token_result ? std::vector<uint8_t>{'t', 'o', 'k', 'e', 'n'}
                                : std::vector<uint8_t>{});

    histogram_tester.ExpectUniqueSample(
        base::StrCat({"FacilitatedPayments.Pix.GetClientToken.",
                      get_client_token_result ? "Success" : "Failure",
                      ".Latency"}),
        /*sample=*/2000,
        /*expected_bucket_count=*/1);
  }
}

// If the client token is not available, then the PayflowExitedReason histogram
// should be logged.
TEST_F(FacilitatedPaymentsManagerTest,
       PayflowExitedReason_ClientTokenNotAvailable) {
  base::HistogramTester histogram_tester;

  manager_->OnGetClientToken(std::vector<uint8_t>{});

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PayflowExitedReason",
      /*sample=*/PayflowExitedReason::kClientTokenNotAvailable,
      /*expected_bucket_count=*/1);
}

TEST_F(FacilitatedPaymentsManagerTest,
       OnGetClientToken_ClientTokenEmpty_ErrorScreenShown) {
  EXPECT_CALL(*client_, ShowErrorScreen());

  manager_->OnGetClientToken(std::vector<uint8_t>{});
}

TEST_F(FacilitatedPaymentsManagerTest, ResettingPreventsPayment) {
  manager_->initiate_payment_request_details_->risk_data_ =
      "seems pretty risky";
  manager_->initiate_payment_request_details_->client_token_ =
      std::vector<uint8_t>{'t', 'o', 'k', 'e', 'n'};
  manager_->initiate_payment_request_details_->billing_customer_number_ = 13;
  manager_->initiate_payment_request_details_->merchant_payment_page_hostname_ =
      "foo.com";
  manager_->initiate_payment_request_details_->instrument_id_ = 13;
  manager_->initiate_payment_request_details_->pix_code_ = "a valid code";

  EXPECT_TRUE(
      manager_->initiate_payment_request_details_->IsReadyForPixPayment());

  manager_->Reset();

  EXPECT_FALSE(
      manager_->initiate_payment_request_details_->IsReadyForPixPayment());
}

TEST_F(FacilitatedPaymentsManagerTest,
       CopyTrigger_UrlInAllowlist_LogPixCodeCopied) {
  base::HistogramTester histogram_tester;
  payments_data_manager_->AddMaskedBankAccountForTest(
      CreatePixBankAccount(/*instrument_id=*/1));
  GURL url("https://example.com/");
  // Mock allowlist check result.
  EXPECT_CALL(
      *optimization_guide_decider_,
      CanApplyOptimization(
          testing::Eq(url),
          testing::Eq(
              optimization_guide::proto::PIX_MERCHANT_ORIGINS_ALLOWLIST),
          testing::Matcher<optimization_guide::OptimizationMetadata*>(
              testing::Eq(nullptr))))
      .Times(1)
      .WillOnce(testing::Return(
          optimization_guide::OptimizationGuideDecision::kTrue));

  manager_->OnPixCodeCopiedToClipboard(
      url, "00020126370014br.gov.bcb.pix2515www.example.com6304EA3F",
      ukm::UkmRecorder::GetNewSourceID());

  histogram_tester.ExpectUniqueSample("FacilitatedPayments.Pix.PixCodeCopied",
                                      /*sample=*/true,
                                      /*expected_bucket_count=*/1);
  auto ukm_entries = ukm_recorder_.GetEntries(
      ukm::builders::FacilitatedPayments_PixCodeCopied::kEntryName,
      {ukm::builders::FacilitatedPayments_PixCodeCopied::kPixCodeCopiedName});
  EXPECT_EQ(ukm_entries.size(), 1UL);
  EXPECT_EQ(ukm_entries[0].metrics.at("PixCodeCopied"), true);
}

TEST_F(FacilitatedPaymentsManagerTest,
       CopyTrigger_UrlInAllowlist_PixValidationTriggered) {
  payments_data_manager_->AddMaskedBankAccountForTest(
      CreatePixBankAccount(/*instrument_id=*/1));
  GURL url("https://example.com/");
  // Mock allowlist check result.
  EXPECT_CALL(
      *optimization_guide_decider_,
      CanApplyOptimization(
          testing::Eq(url),
          testing::Eq(
              optimization_guide::proto::PIX_MERCHANT_ORIGINS_ALLOWLIST),
          testing::Matcher<optimization_guide::OptimizationMetadata*>(
              testing::Eq(nullptr))))
      .Times(1)
      .WillOnce(testing::Return(
          optimization_guide::OptimizationGuideDecision::kTrue));
  // If Pix validation is run, then IsAvailable should get called once.
  EXPECT_CALL(GetApiClient(), IsAvailable(testing::_));

  manager_->OnPixCodeCopiedToClipboard(
      url, "00020126370014br.gov.bcb.pix2515www.example.com6304EA3F",
      ukm::UkmRecorder::GetNewSourceID());

  // The DataDecoder (utility process) validates the Pix code string
  // asynchronously.
  task_environment_.RunUntilIdle();
}

TEST_F(FacilitatedPaymentsManagerTest,
       CopyTrigger_UrlNotInAllowlist_PixValidationNotTriggered) {
  payments_data_manager_->AddMaskedBankAccountForTest(
      CreatePixBankAccount(/*instrument_id=*/1));
  GURL url("https://example.com/");
  // Mock allowlist check result.
  EXPECT_CALL(
      *optimization_guide_decider_,
      CanApplyOptimization(
          testing::Eq(url),
          testing::Eq(
              optimization_guide::proto::PIX_MERCHANT_ORIGINS_ALLOWLIST),
          testing::Matcher<optimization_guide::OptimizationMetadata*>(
              testing::Eq(nullptr))))
      .Times(1)
      .WillOnce(testing::Return(
          optimization_guide::OptimizationGuideDecision::kFalse));

  // If Pix validation is not run, then IsAvailable shouldn't get called.
  EXPECT_CALL(GetApiClient(), IsAvailable(testing::_)).Times(0);

  manager_->OnPixCodeCopiedToClipboard(
      url, "00020126370014br.gov.bcb.pix2515www.example.com6304EA3F",
      ukm::UkmRecorder::GetNewSourceID());
  // The DataDecoder (utility process) validates the Pix code string
  // asynchronously.
  task_environment_.RunUntilIdle();
}

TEST_F(FacilitatedPaymentsManagerTest,
       TestPayFlowCanBeTriggeredOnlyOncePerPageLoad) {
  payments_data_manager_->AddMaskedBankAccountForTest(
      CreatePixBankAccount(/*instrument_id=*/1));
  GURL url("https://example.com/");
  // Mock allowlist check result.
  EXPECT_CALL(*optimization_guide_decider_,
              CanApplyOptimization(
                  testing::Eq(url), testing::_,
                  testing::Matcher<optimization_guide::OptimizationMetadata*>(
                      testing::Eq(nullptr))))
      .WillOnce(testing::Return(
          optimization_guide::OptimizationGuideDecision::kTrue));

  // Even if there are multiple copy events, the payflow should be initiated
  // only once. This can be verified with a single IsAvailable call.
  EXPECT_CALL(GetApiClient(), IsAvailable(testing::_));

  std::string pix_code =
      "00020126370014br.gov.bcb.pix2515www.example.com6304EA3F";
  manager_->OnPixCodeCopiedToClipboard(url, pix_code,
                                       ukm::UkmRecorder::GetNewSourceID());
  manager_->OnPixCodeCopiedToClipboard(url, pix_code,
                                       ukm::UkmRecorder::GetNewSourceID());
  // The DataDecoder (utility process) validates the Pix code string
  // asynchronously.
  task_environment_.RunUntilIdle();
}

// The manager checks for API availability after validating the Pix code.
TEST_F(FacilitatedPaymentsManagerTest,
       ApiClientTriggeredAfterPixCodeValidation) {
  payments_data_manager_->AddMaskedBankAccountForTest(
      CreatePixBankAccount(/*instrument_id=*/1));

  EXPECT_CALL(GetApiClient(), IsAvailable(testing::_));

  manager_->OnPixCodeValidated(/*pix_code=*/std::string(),
                               base::TimeTicks::Now(),
                               /*is_pix_code_valid=*/true);
}

// If the Pix code validation in the utility process has returned `false`, then
// the manager does not check the API for availability.
TEST_F(FacilitatedPaymentsManagerTest,
       PixCodeValidationFailed_NoApiClientTriggered) {
  payments_data_manager_->AddMaskedBankAccountForTest(
      CreatePixBankAccount(/*instrument_id=*/1));

  EXPECT_CALL(GetApiClient(), IsAvailable(testing::_)).Times(0);

  manager_->OnPixCodeValidated(/*pix_code=*/std::string(),
                               base::TimeTicks::Now(),
                               /*is_pix_code_valid=*/false);
}

// If the Pix code validation in the utility process has returned `false`, then
// the PayflowExitedReason histogram should be logged.
TEST_F(FacilitatedPaymentsManagerTest, PayflowExitedReason_InvalidCode) {
  base::HistogramTester histogram_tester;

  manager_->OnPixCodeValidated(/*pix_code=*/std::string(),
                               base::TimeTicks::Now(),
                               /*is_pix_code_valid=*/false);

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PayflowExitedReason",
      /*sample=*/PayflowExitedReason::kInvalidCode,
      /*expected_bucket_count=*/1);
}

// If the user has opted out of the Pix flow, the PayflowExitedReason
// histogram should be logged.
TEST_F(FacilitatedPaymentsManagerTest, PayflowExitedReason_UserOptedOut) {
  base::HistogramTester histogram_tester;
  payments_data_manager_->AddMaskedBankAccountForTest(
      CreatePixBankAccount(/*instrument_id=*/1));
  autofill::prefs::SetFacilitatedPaymentsPix(pref_service_.get(), false);

  manager_->OnPixCodeValidated(/*pix_code=*/std::string(),
                               base::TimeTicks::Now(),
                               /*is_pix_code_valid=*/true);

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PayflowExitedReason",
      /*sample=*/PayflowExitedReason::kUserOptedOut,
      /*expected_bucket_count=*/1);
}

// If the user doesn't have any linked Pix account, the PayflowExitedReason
// histogram should be logged.
TEST_F(FacilitatedPaymentsManagerTest, PayflowExitedReason_NoLinkedAccount) {
  base::HistogramTester histogram_tester;

  manager_->OnPixCodeValidated(/*pix_code=*/std::string(),
                               base::TimeTicks::Now(),
                               /*is_pix_code_valid=*/true);

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PayflowExitedReason",
      /*sample=*/PayflowExitedReason::kNoLinkedAccount,
      /*expected_bucket_count=*/1);
}

// If the validation utility process has disconnected (e.g., due to a crash in
// the validation code), then the manager does not check the API for
// availability.
TEST_F(FacilitatedPaymentsManagerTest,
       PixCodeValidatorTerminatedUnexpectedly_NoApiClientTriggered) {
  payments_data_manager_->AddMaskedBankAccountForTest(
      CreatePixBankAccount(/*instrument_id=*/1));

  EXPECT_CALL(GetApiClient(), IsAvailable(testing::_)).Times(0);

  manager_->OnPixCodeValidated(
      /*pix_code=*/std::string(), base::TimeTicks::Now(),
      /*is_pix_code_valid=*/
      base::unexpected("Data Decoder terminated unexpectedly"));
}

// If the validation utility process has disconnected (e.g., due to a crash in
// the validation code), then the PayflowExitedReason histogram should be
// logged.
TEST_F(FacilitatedPaymentsManagerTest,
       PayflowExitedReason_CodeValidatorFailed) {
  base::HistogramTester histogram_tester;

  manager_->OnPixCodeValidated(
      /*pix_code=*/std::string(), base::TimeTicks::Now(),
      /*is_pix_code_valid=*/
      base::unexpected("Data Decoder terminated unexpectedly"));

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PayflowExitedReason",
      /*sample=*/PayflowExitedReason::kCodeValidatorFailed,
      /*expected_bucket_count=*/1);
}

// If the Pix payment user pref is turned off, the manager does not check
// whether the facilitated payment API is available.
TEST_F(FacilitatedPaymentsManagerTest, PixPrefTurnedOff_NoApiClientTriggered) {
  payments_data_manager_->AddMaskedBankAccountForTest(
      CreatePixBankAccount(/*instrument_id=*/1));
  // Turn off Pix pref.
  autofill::prefs::SetFacilitatedPaymentsPix(pref_service_.get(), false);

  EXPECT_CALL(GetApiClient(), IsAvailable(testing::_)).Times(0);

  manager_->OnPixCodeValidated(/*pix_code=*/std::string(),
                               base::TimeTicks::Now(),
                               /*is_pix_code_valid=*/true);
}

// If the user doesn't have any linked Pix accounts, the manager does not check
// whether the facilitated payment API is available.
TEST_F(FacilitatedPaymentsManagerTest, NoPixAccounts_NoApiClientTriggered) {
  EXPECT_CALL(GetApiClient(), IsAvailable(testing::_)).Times(0);

  manager_->OnPixCodeValidated(/*pix_code=*/std::string(),
                               base::TimeTicks::Now(),
                               /*is_pix_code_valid=*/true);
}

// If payments data manager is unavailable, the manager does not check
// whether the facilitated payment API is available.
TEST_F(FacilitatedPaymentsManagerTest,
       NoPaymentsDataManager_NoApiClientTriggered) {
  payments_data_manager_->AddMaskedBankAccountForTest(
      CreatePixBankAccount(/*instrument_id=*/1));
  ON_CALL(*client_, GetPaymentsDataManager)
      .WillByDefault(testing::Return(nullptr));

  EXPECT_CALL(GetApiClient(), IsAvailable(testing::_)).Times(0);

  manager_->OnPixCodeValidated(/*pix_code=*/std::string(),
                               base::TimeTicks::Now(),
                               /*is_pix_code_valid=*/true);
}

// Test that SendInitiatePaymentRequest initiates payment using the
// FacilitatedPaymentsNetworkInterface.
TEST_F(FacilitatedPaymentsManagerTest, SendInitiatePaymentRequest) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(payments_network_interface_,
              InitiatePayment(testing::_, testing::_, testing::_));

  manager_->SendInitiatePaymentRequest();

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.InitiatePayment.Attempt",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
}

// Test that if the response from
// `FacilitatedPaymentsNetworkInterface::InitiatePayment` call has failure
// result, purchase action is not invoked. Instead, an error message is shown.
TEST_F(FacilitatedPaymentsManagerTest,
       OnInitiatePaymentResponseReceived_FailureResponse) {
  base::HistogramTester histogram_tester;
  manager_->SendInitiatePaymentRequest();
  ON_CALL(*client_, GetCoreAccountInfo)
      .WillByDefault(testing::Return(CreateLoggedInAccountInfo()));

  EXPECT_CALL(*client_, ShowErrorScreen());
  EXPECT_CALL(GetApiClient(), InvokePurchaseAction).Times(0);

  auto response_details =
      std::make_unique<FacilitatedPaymentsInitiatePaymentResponseDetails>();
  response_details->action_token_ =
      std::vector<uint8_t>{'t', 'o', 'k', 'e', 'n'};
  FastForwardBy(base::Seconds(2));
  manager_->OnInitiatePaymentResponseReceived(
      autofill::payments::PaymentsAutofillClient::PaymentsRpcResult::
          kPermanentFailure,
      std::move(response_details));

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PayflowExitedReason",
      /*sample=*/PayflowExitedReason::kInitiatePaymentFailed,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.InitiatePayment.Failure.Latency",
      /*sample=*/2000,
      /*expected_bucket_count=*/1);
}

// Test that if the response from
// `FacilitatedPaymentsNetworkInterface::InitiatePayment` has empty action
// token, purchase action is not invoked. Instead, an error message is shown.
TEST_F(FacilitatedPaymentsManagerTest,
       OnInitiatePaymentResponseReceived_NoActionToken_ErrorScreenShown) {
  base::HistogramTester histogram_tester;
  manager_->SendInitiatePaymentRequest();
  ON_CALL(*client_, GetCoreAccountInfo)
      .WillByDefault(testing::Return(CreateLoggedInAccountInfo()));

  EXPECT_CALL(*client_, ShowErrorScreen());
  EXPECT_CALL(GetApiClient(), InvokePurchaseAction).Times(0);

  FastForwardBy(base::Seconds(2));
  auto response_details =
      std::make_unique<FacilitatedPaymentsInitiatePaymentResponseDetails>();
  manager_->OnInitiatePaymentResponseReceived(
      autofill::payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
      std::move(response_details));

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.InitiatePayment.Success.Latency",
      /*sample=*/2000,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PayflowExitedReason",
      /*sample=*/PayflowExitedReason::kActionTokenNotAvailable,
      /*expected_bucket_count=*/1);
}

// Test that if the core account is std::nullopt, purchase action is not
// invoked. Instead, an error message is shown.
TEST_F(FacilitatedPaymentsManagerTest,
       OnInitiatePaymentResponseReceived_NoCoreAccountInfo_ErrorScreenShown) {
  base::HistogramTester histogram_tester;
  manager_->SendInitiatePaymentRequest();
  ON_CALL(*client_, GetCoreAccountInfo)
      .WillByDefault(testing::Return(std::nullopt));

  EXPECT_CALL(*client_, ShowErrorScreen());
  EXPECT_CALL(GetApiClient(), InvokePurchaseAction).Times(0);

  FastForwardBy(base::Seconds(2));
  auto response_details =
      std::make_unique<FacilitatedPaymentsInitiatePaymentResponseDetails>();
  response_details->action_token_ =
      std::vector<uint8_t>{'t', 'o', 'k', 'e', 'n'};
  manager_->OnInitiatePaymentResponseReceived(
      autofill::payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
      std::move(response_details));

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.InitiatePayment.Success.Latency",
      /*sample=*/2000,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PayflowExitedReason",
      /*sample=*/PayflowExitedReason::kUserLoggedOut,
      /*expected_bucket_count=*/1);
}

// Test that if the user is logged out, purchase action is not invoked. Instead,
// an error message is shown.
TEST_F(FacilitatedPaymentsManagerTest,
       OnInitiatePaymentResponseReceived_LoggedOutProfile_ErrorScreenShown) {
  base::HistogramTester histogram_tester;
  manager_->SendInitiatePaymentRequest();
  ON_CALL(*client_, GetCoreAccountInfo)
      .WillByDefault(testing::Return(CoreAccountInfo()));

  EXPECT_CALL(*client_, ShowErrorScreen());
  EXPECT_CALL(GetApiClient(), InvokePurchaseAction).Times(0);

  FastForwardBy(base::Seconds(2));
  auto response_details =
      std::make_unique<FacilitatedPaymentsInitiatePaymentResponseDetails>();
  response_details->action_token_ =
      std::vector<uint8_t>{'t', 'o', 'k', 'e', 'n'};
  manager_->OnInitiatePaymentResponseReceived(
      autofill::payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
      std::move(response_details));

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.InitiatePayment.Success.Latency",
      /*sample=*/2000,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PayflowExitedReason",
      /*sample=*/PayflowExitedReason::kUserLoggedOut,
      /*expected_bucket_count=*/1);
}

// Test that the puchase action is invoked after receiving a success response
// from the `FacilitatedPaymentsNetworkInterface::InitiatePayment` call.
TEST_F(FacilitatedPaymentsManagerTest,
       OnInitiatePaymentResponseReceived_InvokePurchaseActionTriggered) {
  base::HistogramTester histogram_tester;
  manager_->SendInitiatePaymentRequest();
  ON_CALL(*client_, GetCoreAccountInfo)
      .WillByDefault(testing::Return(CreateLoggedInAccountInfo()));

  EXPECT_CALL(GetApiClient(), InvokePurchaseAction);

  FastForwardBy(base::Seconds(2));
  auto response_details =
      std::make_unique<FacilitatedPaymentsInitiatePaymentResponseDetails>();
  response_details->action_token_ =
      std::vector<uint8_t>{'t', 'o', 'k', 'e', 'n'};
  manager_->OnInitiatePaymentResponseReceived(
      autofill::payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
      std::move(response_details));

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.InitiatePayment.Success.Latency",
      /*sample=*/2000,
      /*expected_bucket_count=*/1);
}

// The `IsAvailable` async call is made after a valid Pix code has been
// detected. This test verifies that the result and latency are logged after the
// async call is completed.
TEST_F(FacilitatedPaymentsManagerTest,
       LogApiAvailabilityCheckResultAndLatency) {
  base::HistogramTester histogram_tester;
  payments_data_manager_->AddMaskedBankAccountForTest(
      CreatePixBankAccount(/*instrument_id=*/1));
  EXPECT_CALL(GetApiClient(), IsAvailable(testing::_));
  manager_->OnPixCodeValidated(/*pix_code=*/std::string(),
                               base::TimeTicks::Now(),
                               /*is_pix_code_valid=*/true);
  FastForwardBy(base::Seconds(2));

  manager_->OnApiAvailabilityReceived(true);

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.IsApiAvailable.Success.Latency",
      /*sample=*/2000,
      /*expected_bucket_count=*/1);
}

// The `IsAvailable` async call is made after a valid Pix code has been
// detected. This test verifies that if the api available result is false, the
// PayflowExitedReason histogram is logged.
TEST_F(FacilitatedPaymentsManagerTest,
       PayflowExitedReason_ApiClientNotAvailable) {
  base::HistogramTester histogram_tester;

  manager_->OnApiAvailabilityReceived(false);

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PayflowExitedReason",
      /*sample=*/PayflowExitedReason::kApiClientNotAvailable,
      /*expected_bucket_count=*/1);
}

// Test that when a purchase action result is received, the UI prompt is
// dismissed.
TEST_F(FacilitatedPaymentsManagerTest,
       OnPurchaseActionResult_UiPromptDismissed) {
  // `DismissPrompt` is called once whenever a purchase action result is
  // received, and again when the test fixture destroys the `manager_`.
  EXPECT_CALL(*client_, DismissPrompt()).Times(3);

  for (PurchaseActionResult result : {PurchaseActionResult::kResultOk,
                                      PurchaseActionResult::kResultCanceled}) {
    manager_->OnPurchaseActionResult(result);
  }
}

// Test that when an InitiatePurchaseAction request is sent, the attempt is
// logged.
TEST_F(FacilitatedPaymentsManagerTest, LogInitiatePurchaseActionAttempt) {
  base::HistogramTester histogram_tester;
  ON_CALL(*client_, GetCoreAccountInfo)
      .WillByDefault(testing::Return(CreateLoggedInAccountInfo()));
  EXPECT_CALL(GetApiClient(), InvokePurchaseAction);
  auto response_details =
      std::make_unique<FacilitatedPaymentsInitiatePaymentResponseDetails>();
  response_details->action_token_ =
      std::vector<uint8_t>{'t', 'o', 'k', 'e', 'n'};
  manager_->OnInitiatePaymentResponseReceived(
      autofill::payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
      std::move(response_details));

  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.InitiatePurchaseAction.Attempt",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
}

// Test that when an InitiatePurchaseAction response is received, the result and
// latency of the invoke purchase action is logged.
TEST_F(FacilitatedPaymentsManagerTest,
       LogInitiatePurchaseActionResultAndLatency) {
  size_t index = 0;
  for (PurchaseActionResult result :
       {PurchaseActionResult::kResultOk, PurchaseActionResult::kCouldNotInvoke,
        PurchaseActionResult::kResultCanceled}) {
    base::HistogramTester histogram_tester;
    ON_CALL(*client_, GetCoreAccountInfo)
        .WillByDefault(testing::Return(CreateLoggedInAccountInfo()));
    EXPECT_CALL(GetApiClient(), InvokePurchaseAction);
    auto response_details =
        std::make_unique<FacilitatedPaymentsInitiatePaymentResponseDetails>();
    response_details->action_token_ =
        std::vector<uint8_t>{'t', 'o', 'k', 'e', 'n'};
    manager_->OnInitiatePaymentResponseReceived(
        autofill::payments::PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
        std::move(response_details));

    FastForwardBy(base::Seconds(2));
    manager_->OnPurchaseActionResult(result);

    histogram_tester.ExpectBucketCount(
        base::StrCat({"FacilitatedPayments.Pix.InitiatePurchaseAction.",
                      manager_->GetInitiatePurchaseActionResultString(result),
                      ".Latency"}),
        /*sample=*/2000,
        /*expected_count=*/1);
    auto ukm_entries = ukm_recorder_.GetEntries(
        ukm::builders::FacilitatedPayments_Pix_InitiatePurchaseActionResult::
            kEntryName,
        {ukm::builders::FacilitatedPayments_Pix_InitiatePurchaseActionResult::
             kResultName});
    ASSERT_EQ(ukm_entries.size(), index + 1);
    EXPECT_EQ(ukm_entries[index++].metrics.at("Result"),
              static_cast<uint8_t>(result));
  }
}

// Verify that the API client is initialized lazily, so it does not take up
// space in memory unless it's being used.
TEST_F(FacilitatedPaymentsManagerTest, ApiClientInitializedLazily) {
  payments_data_manager_->AddMaskedBankAccountForTest(
      CreatePixBankAccount(/*instrument_id=*/1));

  EXPECT_EQ(nullptr, manager_->api_client_.get());

  manager_->OnPixCodeValidated(/*pix_code=*/std::string(),
                               base::TimeTicks::Now(),
                               /*is_pix_code_valid=*/true);

  EXPECT_NE(nullptr, manager_->api_client_.get());
}

// Verify that a failure to lazily initialize the API client is not fatal.
TEST_F(FacilitatedPaymentsManagerTest,
       HandlesFailureToLazilyInitializeApiClient) {
  payments_data_manager_->AddMaskedBankAccountForTest(
      CreatePixBankAccount(/*instrument_id=*/1));
  manager_->api_client_creator_.Reset();

  EXPECT_EQ(nullptr, manager_->api_client_.get());

  manager_->OnPixCodeValidated(/*pix_code=*/std::string(),
                               base::TimeTicks::Now(),
                               /*is_pix_code_valid=*/true);

  EXPECT_EQ(nullptr, manager_->api_client_.get());
}

// Test class for devices being used in the landscape mode.
class FacilitatedPaymentsManagerTestInLandscapeMode
    : public FacilitatedPaymentsManagerTest,
      public testing::WithParamInterface<bool> {
 public:
  FacilitatedPaymentsManagerTestInLandscapeMode() {
    scoped_feature_list_.InitWithFeatureState(kEnablePixPaymentsInLandscapeMode,
                                              GetParam());
  }

  void SetUp() override {
    FacilitatedPaymentsManagerTest::SetUp();
    ON_CALL(*client_, IsInLandscapeMode).WillByDefault(testing::Return(true));
  }

  bool IsPaymentEnabledInLandscapeMode() { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         FacilitatedPaymentsManagerTestInLandscapeMode,
                         testing::Bool());

TEST_P(FacilitatedPaymentsManagerTestInLandscapeMode,
       PixPayflowBlockedWhenFlagDisabled) {
  payments_data_manager_->AddMaskedBankAccountForTest(
      CreatePixBankAccount(/*instrument_id=*/1));

  // In landscape mode, checking the API client's availability (which is part of
  // Pix payflow) is only done if the `EnablePixPaymentsInLandscapeMode` flag is
  // enabled.
  EXPECT_CALL(GetApiClient(), IsAvailable(testing::_))
      .Times(IsPaymentEnabledInLandscapeMode() ? 1 : 0);

  manager_->OnPixCodeValidated(/*pix_code=*/std::string(),
                               base::TimeTicks::Now(),
                               /*is_pix_code_valid=*/true);
}

TEST_P(FacilitatedPaymentsManagerTestInLandscapeMode,
       PayflowExitedReason_LandscapeScreenOrientation) {
  base::HistogramTester histogram_tester;
  payments_data_manager_->AddMaskedBankAccountForTest(
      CreatePixBankAccount(/*instrument_id=*/1));

  manager_->OnPixCodeValidated(/*pix_code=*/std::string(),
                               base::TimeTicks::Now(),
                               /*is_pix_code_valid=*/true);

  // In landscape mode, if the `EnablePixPaymentsInLandscapeMode` flag is
  // disabled, Pix payment is not offered, and a histogram should be logged.
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PayflowExitedReason",
      /*sample=*/PayflowExitedReason::kLandscapeScreenOrientation,
      /*expected_bucket_count=*/IsPaymentEnabledInLandscapeMode() ? 0 : 1);
}

TEST_F(FacilitatedPaymentsManagerTest, ShowPixPaymentPrompt) {
  // Verify the default UI state.
  EXPECT_EQ(manager_->ui_state_, UiState::kHidden);

  // Verify that when the feature wants to show the payment prompt, it asks the
  // client.
  EXPECT_CALL(*client_, ShowPixPaymentPrompt(testing::_, testing::_));

  const std::vector<autofill::BankAccount> bank_accounts = {
      autofill::test::CreatePixBankAccount(100L)};
  manager_->ShowPixPaymentPrompt(std::move(bank_accounts), base::DoNothing());

  // Verify that the UI state is updated.
  EXPECT_EQ(manager_->ui_state_, UiState::kFopSelector);
}

TEST_F(FacilitatedPaymentsManagerTest, ShowProgressScreen) {
  // Verify the default UI state.
  EXPECT_EQ(manager_->ui_state_, UiState::kHidden);

  // Verify that when the feature wants to show the progress screen, it asks the
  // client.
  EXPECT_CALL(*client_, ShowProgressScreen);

  manager_->ShowProgressScreen();

  // Verify that the UI state is updated.
  EXPECT_EQ(manager_->ui_state_, UiState::kProgressScreen);
}

TEST_F(FacilitatedPaymentsManagerTest, ShowErrorScreen) {
  // Verify the default UI state.
  EXPECT_EQ(manager_->ui_state_, UiState::kHidden);

  // Verify that when the feature wants to show the error screen, it asks the
  // client.
  EXPECT_CALL(*client_, ShowErrorScreen);

  manager_->ShowErrorScreen();

  // Verify that the UI state is updated.
  EXPECT_EQ(manager_->ui_state_, UiState::kErrorScreen);
}

TEST_F(FacilitatedPaymentsManagerTest, DismissPrompt) {
  // Verify that when the feature wants to dismiss the UI screen, it asks the
  // client. The second call is from test teardown.
  EXPECT_CALL(*client_, DismissPrompt).Times(2);

  manager_->DismissPrompt();

  // Verify that the UI state is updated.
  EXPECT_EQ(manager_->ui_state_, UiState::kHidden);
}

// Test that when the Pix FOP selector is shown, related Pix metrics are logged.
TEST_F(FacilitatedPaymentsManagerTest, PixFopSelectorShown_HistogramsLogged) {
  base::HistogramTester histogram_tester;

  // Simulate Pix code being copied. The latency is computed from this point.
  manager_->OnPixCodeCopiedToClipboard(GURL("https://example.com/"),
                                       std::string(),
                                       ukm::UkmRecorder::GetNewSourceID());
  // Fully mocked time, does not advance by itself.
  FastForwardBy(base::Seconds(2));
  // Simulate that the FOP selector was shown successfully.
  std::vector<autofill::BankAccount> bank_accounts = {
      autofill::test::CreatePixBankAccount(100L)};
  manager_->ShowPixPaymentPrompt(std::move(bank_accounts), base::DoNothing());
  manager_->OnUiEvent(UiEvent::kNewScreenShown);

  // Verify that when the Pix FOP selector is shown, related metrics are
  // logged.
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.FopSelectorShown.LatencyAfterCopy",
      /*sample=*/2000,
      /*expected_bucket_count=*/1);
  auto ukm_entries = ukm_recorder_.GetEntries(
      ukm::builders::FacilitatedPayments_Pix_FopSelectorShown::kEntryName,
      {ukm::builders::FacilitatedPayments_Pix_FopSelectorShown::kShownName});
  EXPECT_EQ(ukm_entries.size(), 1UL);
  EXPECT_EQ(ukm_entries[0].metrics.at("Shown"), true);
}

class FacilitatedPaymentsManagerTestForUiScreens
    : public FacilitatedPaymentsManagerTest,
      public testing::WithParamInterface<UiState> {
 public:
  void SetUp() override {
    FacilitatedPaymentsManagerTest::SetUp();

    // Default state.
    EXPECT_EQ(manager_->ui_state_, UiState::kHidden);

    switch (GetParam()) {
      case UiState::kFopSelector: {
        const std::vector<autofill::BankAccount> bank_accounts = {
            autofill::test::CreatePixBankAccount(100L)};
        manager_->ShowPixPaymentPrompt(std::move(bank_accounts),
                                       base::DoNothing());
        break;
      }
      case UiState::kProgressScreen:
        manager_->ShowProgressScreen();
        break;
      case UiState::kErrorScreen:
        manager_->ShowErrorScreen();
        break;
      case UiState::kHidden:
        NOTREACHED();
    }
  }

  UiState ui_state() { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(FacilitatedPaymentsManagerTest,
                         FacilitatedPaymentsManagerTestForUiScreens,
                         testing::Values(UiState::kFopSelector,
                                         UiState::kProgressScreen,
                                         UiState::kErrorScreen));

// Test that when a new screen is shown, UI state reflects the current UI being
// shown.
TEST_P(FacilitatedPaymentsManagerTestForUiScreens, NewScreenShown) {
  base::HistogramTester histogram_tester;

  // Simulate new screen was shown successfully.
  manager_->OnUiEvent(UiEvent::kNewScreenShown);

  // Verify feature has updated the UI state.
  EXPECT_EQ(manager_->ui_state_, ui_state());
  // Verify that the histogram is logged.
  histogram_tester.ExpectUniqueSample("FacilitatedPayments.Pix.UiScreenShown",
                                      /*sample=*/ui_state(),
                                      /*expected_bucket_count=*/1);
  if (ui_state() == UiState::kFopSelector) {
    auto ukm_entries = ukm_recorder_.GetEntries(
        ukm::builders::FacilitatedPayments_Pix_FopSelectorShown::kEntryName,
        {ukm::builders::FacilitatedPayments_Pix_FopSelectorShown::kShownName});
    EXPECT_EQ(ukm_entries.size(), 1UL);
    EXPECT_EQ(ukm_entries[0].metrics.at("Shown"), true);
  }
}

// Test that when a new screen could not be shown, UI state is updated.
TEST_P(FacilitatedPaymentsManagerTestForUiScreens, NewScreenCouldNotBeShown) {
  base::HistogramTester histogram_tester;

  // Simulate new screen could not be shown.
  manager_->OnUiEvent(UiEvent::kScreenClosedNotByUser);

  // Verify that the UI state is hidden.
  EXPECT_EQ(manager_->ui_state_, UiState::kHidden);
  // Verify that the payflow exited histogram is logged.
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PayflowExitedReason",
      /*sample=*/PayflowExitedReason::kFopSelectorClosedNotByUser,
      /*expected_bucket_count=*/ui_state() == UiState::kFopSelector ? 1 : 0);
}

// Test that when the UI screen is closed, but it was not due to a user action,
// the feature updates the UI state.
TEST_P(FacilitatedPaymentsManagerTestForUiScreens, ScreenClosedNotByUser) {
  base::HistogramTester histogram_tester;

  // Simulate new screen was shown successfully.
  manager_->OnUiEvent(UiEvent::kNewScreenShown);
  // Simulate UI screen was closed, but it was not due to a user action.
  manager_->OnUiEvent(UiEvent::kScreenClosedNotByUser);

  // Verify that the UI state is hidden.
  EXPECT_EQ(manager_->ui_state_, UiState::kHidden);
  // Verify that the payflow exited histogram is logged.
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PayflowExitedReason",
      /*sample=*/PayflowExitedReason::kFopSelectorClosedNotByUser,
      /*expected_bucket_count=*/ui_state() == UiState::kFopSelector ? 1 : 0);
}

// Test that when the UI screen is closed by the user, the feature updates the
// UI state.
TEST_P(FacilitatedPaymentsManagerTestForUiScreens, ScreenClosedByUser) {
  base::HistogramTester histogram_tester;

  // Simulate new screen was shown successfully.
  manager_->OnUiEvent(UiEvent::kNewScreenShown);
  // Simulate UI screen was closed by the user.
  manager_->OnUiEvent(UiEvent::kScreenClosedByUser);

  // Verify that the UI state is hidden.
  EXPECT_EQ(manager_->ui_state_, UiState::kHidden);
  // Verify that the payflow exited histogram is logged.
  histogram_tester.ExpectUniqueSample(
      "FacilitatedPayments.Pix.PayflowExitedReason",
      /*sample=*/PayflowExitedReason::kFopSelectorClosedByUser,
      /*expected_bucket_count=*/ui_state() == UiState::kFopSelector ? 1 : 0);
  if (ui_state() == UiState::kFopSelector) {
    auto ukm_entries = ukm_recorder_.GetEntries(
        ukm::builders::FacilitatedPayments_Pix_FopSelectorResult::kEntryName,
        {ukm::builders::FacilitatedPayments_Pix_FopSelectorResult::
             kResultName});
    ASSERT_EQ(ukm_entries.size(), 1UL);
    EXPECT_EQ(ukm_entries[0].metrics.at("Result"), false);
  }
}

}  // namespace payments::facilitated
