// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/boca_manager.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/boca/babelorca/babel_orca_controller.h"
#include "chromeos/ash/components/boca/babelorca/babel_orca_manager.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_request_data_provider.h"
#include "chromeos/ash/components/boca/babelorca/token_manager.h"
#include "chromeos/ash/components/boca/boca_metrics_manager.h"
#include "chromeos/ash/components/boca/boca_session_manager.h"
#include "chromeos/ash/components/boca/invalidations/invalidation_service_impl.h"
#include "chromeos/ash/components/boca/session_api/session_client_impl.h"
#include "chromeos/ash/components/browser_context_helper/fake_browser_context_helper_delegate.h"
#include "components/account_id/account_id.h"
#include "components/gcm_driver/fake_gcm_driver.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "google_apis/common/request_sender.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrictMock;

namespace ash {
namespace {

constexpr char kTestEmail[] = "testemail";

class MockInstanceID : public instance_id::InstanceID {
 public:
  MockInstanceID() : InstanceID("app_id", /*gcm_driver=*/nullptr) {}
  ~MockInstanceID() override = default;
  MOCK_METHOD(void, GetID, (GetIDCallback callback), (override));
  MOCK_METHOD(void,
              GetCreationTime,
              (GetCreationTimeCallback callback),
              (override));
  MOCK_METHOD(void,
              GetToken,
              (const std::string& authorized_entity,
               const std::string& scope,
               base::TimeDelta time_to_live,
               std::set<Flags> flags,
               GetTokenCallback callback),
              (override));
  MOCK_METHOD(void,
              ValidateToken,
              (const std::string& authorized_entity,
               const std::string& scope,
               const std::string& token,
               ValidateTokenCallback callback),
              (override));

 protected:
  MOCK_METHOD(void,
              DeleteTokenImpl,
              (const std::string& authorized_entity,
               const std::string& scope,
               DeleteTokenCallback callback),
              (override));
  MOCK_METHOD(void, DeleteIDImpl, (DeleteIDCallback callback), (override));
};

class MockInstanceIDDriver : public instance_id::InstanceIDDriver {
 public:
  MockInstanceIDDriver() : InstanceIDDriver(/*gcm_driver=*/nullptr) {}
  ~MockInstanceIDDriver() override = default;
  MOCK_METHOD(instance_id::InstanceID*,
              GetInstanceID,
              (const std::string& app_id),
              (override));
  MOCK_METHOD(void, RemoveInstanceID, (const std::string& app_id), (override));
  MOCK_METHOD(bool,
              ExistsInstanceID,
              (const std::string& app_id),
              (const override));
};

class MockSessionClientImpl : public boca::SessionClientImpl {
 public:
  explicit MockSessionClientImpl(
      std::unique_ptr<google_apis::RequestSender> sender)
      : SessionClientImpl(std::move(sender)) {}
};

class BocaManagerTest : public testing::Test {
 protected:
  BocaManagerTest() = default;
  void SetUp() override {
    // This is called in the FCMHandler.
    ON_CALL(mock_instance_id_driver_,
            GetInstanceID(boca::InvalidationServiceImpl::kApplicationId))
        .WillByDefault(Return(&mock_instance_id_));
    session_client_impl_ =
        std::make_unique<StrictMock<MockSessionClientImpl>>(nullptr);
    boca_session_manager_ = std::make_unique<boca::BocaSessionManager>(
        session_client_impl_.get(), AccountId::FromUserEmail(kTestEmail),
        /*is_producer*/ false);
    invalidation_service_impl_ =
        std::make_unique<boca::InvalidationServiceImpl>(
            /*=gcm_driver*/ &fake_gcm_driver_,
            /*=instance_id_driver*/ &mock_instance_id_driver_,
            AccountId::FromUserEmail(kTestEmail), boca_session_manager_.get(),
            session_client_impl_.get());
  }

  boca::BabelOrcaManager::ControllerFactory GetBabelOrcaControllerFactory() {
    return base::BindLambdaForTesting(
        [](babelorca::TokenManager*, babelorca::TachyonRequestDataProvider*)
            -> std::unique_ptr<babelorca::BabelOrcaController> {
          return nullptr;
        });
  }

  // BocaSessionManager require task_env for mojom binding.
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<ash::FakeBrowserContextHelperDelegate>
      fake_browser_context_helper_delegate_ =
          std::make_unique<ash::FakeBrowserContextHelperDelegate>();
  ash::BrowserContextHelper helper{
      std::move(fake_browser_context_helper_delegate_)};
  std::unique_ptr<StrictMock<MockSessionClientImpl>> session_client_impl_;
  std::unique_ptr<boca::BocaSessionManager> boca_session_manager_;
  gcm::FakeGCMDriver fake_gcm_driver_;
  NiceMock<MockInstanceIDDriver> mock_instance_id_driver_;
  NiceMock<MockInstanceID> mock_instance_id_;
  std::unique_ptr<boca::InvalidationServiceImpl> invalidation_service_impl_;
  signin::IdentityTestEnvironment identity_test_env_;
  network::TestURLLoaderFactory url_loader_factory_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

class BocaManagerProducerTest : public BocaManagerTest {
 protected:
  BocaManagerProducerTest() = default;
  void SetUp() override {
    BocaManagerTest::SetUp();
    scoped_feature_list_.InitWithFeatures({ash::features::kBoca},
                                          /*disabled_features=*/{});
    boca_manager_ = std::make_unique<BocaManager>(
        std::make_unique<boca::OnTaskSessionManager>(
            /*system_web_app_manager=*/nullptr, /*extensions_manager=*/nullptr),
        std::move(session_client_impl_), std::move(boca_session_manager_),
        std::move(invalidation_service_impl_),
        std::make_unique<boca::BabelOrcaManager>(
            identity_test_env_.identity_manager(),
            url_loader_factory_.GetSafeWeakWrapper(),
            GetBabelOrcaControllerFactory()),
        std::make_unique<boca::BocaMetricsManager>(/*is_producer*/ true));
  }
  std::unique_ptr<BocaManager> boca_manager_;
};

TEST_F(BocaManagerProducerTest, VerifyOnTaskObserverNotAddedForProducer) {
  ASSERT_FALSE(boca_manager_->GetBocaSessionManager()->observers().HasObserver(
      boca_manager_->GetOnTaskSessionManagerForTesting()));
}

TEST_F(BocaManagerProducerTest, VerifyBabelOrcaObserverHasAddedForProducer) {
  ASSERT_TRUE(boca_manager_->GetBocaSessionManager()->observers().HasObserver(
      boca_manager_->GetBabelOrcaManagerForTesting()));
}

TEST_F(BocaManagerProducerTest, VerifyDependenciesTearDownProperly) {
  boca_manager_->Shutdown();
  ASSERT_EQ(nullptr, invalidation_service_impl_);
  ASSERT_TRUE(boca_manager_->GetBocaSessionManager()->observers().empty());
}

TEST_F(BocaManagerProducerTest, VerifyBocaMetricsManagerWasAddedForProducer) {
  ASSERT_TRUE(boca_manager_->GetBocaSessionManager()->observers().HasObserver(
      boca_manager_->GetBocaMetricsManagerForTesting()));
}

class BocaManagerConsumerTest : public BocaManagerTest {
 protected:
  BocaManagerConsumerTest() = default;
  void SetUp() override {
    BocaManagerTest::SetUp();
    scoped_feature_list_.InitWithFeatures(
        /* enabled_features */ {ash::features::kBoca,
                                ash::features::kBocaConsumer},
        /* disabled_features */ {});

    boca_manager_ = std::make_unique<BocaManager>(
        std::make_unique<boca::OnTaskSessionManager>(
            /*system_web_app_manager=*/nullptr, /*extensions_manager=*/nullptr),
        std::move(session_client_impl_), std::move(boca_session_manager_),
        std::move(invalidation_service_impl_),
        std::make_unique<boca::BabelOrcaManager>(
            identity_test_env_.identity_manager(),
            url_loader_factory_.GetSafeWeakWrapper(),
            GetBabelOrcaControllerFactory()),
        std::make_unique<boca::BocaMetricsManager>(/*is_producer*/ false));
  }
  std::unique_ptr<BocaManager> boca_manager_;
};

TEST_F(BocaManagerConsumerTest, VerifyOnTaskObserverHasAddedForConsumer) {
  ASSERT_TRUE(boca_manager_->GetBocaSessionManager()->observers().HasObserver(
      boca_manager_->GetOnTaskSessionManagerForTesting()));
}

TEST_F(BocaManagerConsumerTest, VerifyBabelOrcaObserverHasAddedForConsumer) {
  ASSERT_TRUE(boca_manager_->GetBocaSessionManager()->observers().HasObserver(
      boca_manager_->GetBabelOrcaManagerForTesting()));
}

TEST_F(BocaManagerConsumerTest, VerifyBocaMetricsManagerWasAddedForConsumer) {
  ASSERT_TRUE(boca_manager_->GetBocaSessionManager()->observers().HasObserver(
      boca_manager_->GetBocaMetricsManagerForTesting()));
}

TEST_F(BocaManagerConsumerTest, VerifyDependenciesTearDownProperly) {
  boca_manager_->Shutdown();
  ASSERT_EQ(nullptr, invalidation_service_impl_);
  ASSERT_TRUE(boca_manager_->GetBocaSessionManager()->observers().empty());
}

}  // namespace
}  // namespace ash
