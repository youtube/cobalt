// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/account_status_check_fetcher.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/policy/core/common/cloud/mock_device_management_service.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {
namespace {

using ::testing::ElementsAre;
namespace em = ::enterprise_management;

class AccountStatusCheckFetcherUnitTest : public testing::Test {
 public:
  AccountStatusCheckFetcherUnitTest() = default;
  ~AccountStatusCheckFetcherUnitTest() override = default;

  void SetUpAccountStatusCheckFetcher(const std::string& email) {
    service_ =
        std::make_unique<FakeDeviceManagementService>(&job_creation_handler_);
    service_->ScheduleInitialization(0);
    base::RunLoop().RunUntilIdle();
    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &url_loader_factory_);
    fetcher_ = std::make_unique<AccountStatusCheckFetcher>(
        email, service_.get(), shared_url_loader_factory_);
  }

  void SetReply(const em::DeviceManagementResponse& response) {
    EXPECT_CALL(job_creation_handler_, OnJobCreation)
        .WillOnce(DoAll(service_->SendJobOKAsync(response)))
        .RetiresOnSaturation();
  }

  void SetFailReply(int net_error, int response_code) {
    EXPECT_CALL(job_creation_handler_, OnJobCreation)
        .WillOnce(
            DoAll(service_->SendJobResponseAsync(net_error, response_code)))
        .RetiresOnSaturation();
  }

  void OnFetchCompleted(base::OnceClosure finish_callback,
                        bool fetch_succeeded,
                        AccountStatusCheckFetcher::AccountStatus status) {
    fetch_succeeded_ = fetch_succeeded;
    status_ = status;
    std::move(finish_callback).Run();
  }

  void SyncFetch() {
    base::RunLoop run_loop;
    base::OnceClosure callback = run_loop.QuitClosure();
    fetcher_->Fetch(
        base::BindOnce(&AccountStatusCheckFetcherUnitTest::OnFetchCompleted,
                       base::Unretained(this), std::move(callback)));
    run_loop.Run();
  }

  em::DeviceManagementResponse CreateResponse(
      em::CheckUserAccountResponse::UserAccountType type,
      bool is_domain_verified) {
    em::DeviceManagementResponse response;
    response.mutable_check_user_account_response()->set_user_account_type(type);
    response.mutable_check_user_account_response()->set_domain_verified(
        is_domain_verified);
    return response;
  }

  em::DeviceManagementResponse dummy_response_;
  std::unique_ptr<AccountStatusCheckFetcher> fetcher_;
  base::HistogramTester histogram_tester_;

 protected:
  bool fetch_succeeded_ = false;
  AccountStatusCheckFetcher::AccountStatus status_ =
      AccountStatusCheckFetcher::AccountStatus::kUnknown;

 private:
  content::BrowserTaskEnvironment task_environment_;
  network::TestURLLoaderFactory url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  testing::StrictMock<MockJobCreationHandler> job_creation_handler_;
  std::unique_ptr<FakeDeviceManagementService> service_;
};

TEST_F(AccountStatusCheckFetcherUnitTest, NetworkFailure) {
  SetUpAccountStatusCheckFetcher("fooboo@foobooorg.com");
  SetFailReply(net::OK, DeviceManagementService::kServiceUnavailable);
  SyncFetch();
  EXPECT_FALSE(fetch_succeeded_);
}

TEST_F(AccountStatusCheckFetcherUnitTest, Dasher) {
  SetUpAccountStatusCheckFetcher("fooboo@foobooorg.com");
  SetReply(CreateResponse(em::CheckUserAccountResponse::DASHER,
                          /*is_domain_verified=*/true));
  SyncFetch();
  EXPECT_TRUE(fetch_succeeded_);
  const AccountStatusCheckFetcher::AccountStatus expected_status =
      AccountStatusCheckFetcher::AccountStatus::kDasher;
  EXPECT_EQ(status_, expected_status);
  EXPECT_THAT(
      histogram_tester_.GetAllSamples("Enterprise.AccountStatusCheckResult"),
      ElementsAre(base::Bucket(static_cast<int>(expected_status), 1)));
}

TEST_F(AccountStatusCheckFetcherUnitTest, ConsumerWithConsumerDomain) {
  SetUpAccountStatusCheckFetcher("fooboo@gmail.com");
  SetReply(CreateResponse(em::CheckUserAccountResponse::CONSUMER,
                          /*is_domain_verified=*/true));
  SyncFetch();
  EXPECT_TRUE(fetch_succeeded_);
  const AccountStatusCheckFetcher::AccountStatus expected_status =
      AccountStatusCheckFetcher::AccountStatus::kConsumerWithConsumerDomain;
  EXPECT_EQ(status_, expected_status);
  EXPECT_THAT(
      histogram_tester_.GetAllSamples("Enterprise.AccountStatusCheckResult"),
      ElementsAre(base::Bucket(static_cast<int>(expected_status), 1)));
}

TEST_F(AccountStatusCheckFetcherUnitTest, ConsumerWithBusinessDomain) {
  SetUpAccountStatusCheckFetcher("fooboo@testbusinessdomain.com");
  SetReply(CreateResponse(em::CheckUserAccountResponse::CONSUMER,
                          /*is_domain_verified=*/true));
  SyncFetch();
  EXPECT_TRUE(fetch_succeeded_);
  const AccountStatusCheckFetcher::AccountStatus expected_status =
      AccountStatusCheckFetcher::AccountStatus::kConsumerWithBusinessDomain;
  EXPECT_EQ(status_, expected_status);
  EXPECT_THAT(
      histogram_tester_.GetAllSamples("Enterprise.AccountStatusCheckResult"),
      ElementsAre(base::Bucket(static_cast<int>(expected_status), 1)));
}

TEST_F(AccountStatusCheckFetcherUnitTest, OrganisationalAccountVerified) {
  SetUpAccountStatusCheckFetcher("fooboo@testbusinessdomain.com");
  SetReply(CreateResponse(em::CheckUserAccountResponse::NOT_EXIST,
                          /*is_domain_verified=*/true));
  SyncFetch();
  EXPECT_TRUE(fetch_succeeded_);
  const AccountStatusCheckFetcher::AccountStatus expected_status =
      AccountStatusCheckFetcher::AccountStatus::kOrganisationalAccountVerified;
  EXPECT_EQ(status_, expected_status);
  EXPECT_THAT(
      histogram_tester_.GetAllSamples("Enterprise.AccountStatusCheckResult"),
      ElementsAre(base::Bucket(static_cast<int>(expected_status), 1)));
}

TEST_F(AccountStatusCheckFetcherUnitTest, OrganisationalAccountUnverified) {
  SetUpAccountStatusCheckFetcher("fooboo@testbusinessdomain.com");
  SetReply(CreateResponse(em::CheckUserAccountResponse::NOT_EXIST,
                          /*is_domain_verified=*/false));
  SyncFetch();
  EXPECT_TRUE(fetch_succeeded_);
  const AccountStatusCheckFetcher::AccountStatus expected_status =
      AccountStatusCheckFetcher::AccountStatus::
          kOrganisationalAccountUnverified;
  EXPECT_EQ(status_, expected_status);
  EXPECT_THAT(
      histogram_tester_.GetAllSamples("Enterprise.AccountStatusCheckResult"),
      ElementsAre(base::Bucket(static_cast<int>(expected_status), 1)));
}

}  // namespace
}  // namespace policy
