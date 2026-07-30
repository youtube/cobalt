// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/intro/sign_in_promo_handler.h"

#include <memory>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/management_identity.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/ui/webui/intro/intro_ui.h"
#include "chrome/browser/ui/webui/intro/sign_in_promo.mojom.h"
#include "chrome/grit/generated_resources.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_store.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

using ::testing::_;

class MockPage : public intro::mojom::SignInPromoPage {
 public:
  MockPage() = default;
  ~MockPage() override = default;

  mojo::PendingRemote<intro::mojom::SignInPromoPage> BindAndGetRemote() {
    if (receiver_.is_bound()) {
      receiver_.reset();
    }
    return receiver_.BindNewPipeAndPassRemote();
  }

  void FlushForTesting() { receiver_.FlushForTesting(); }

  MOCK_METHOD(void, OnResetButtons, (), (override));

 private:
  mojo::Receiver<intro::mojom::SignInPromoPage> receiver_{this};
};

class SignInPromoHandlerTest : public testing::Test {
 public:
  SignInPromoHandlerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override { CreateHandler(/*is_device_managed=*/false); }

  void CreateHandler(bool is_device_managed) {
    handler_remote_.reset();
    handler_ = std::make_unique<SignInPromoHandler>(
        mock_signin_choice_callback_.Get(), is_device_managed,
        mock_page_.BindAndGetRemote(),
        handler_remote_.BindNewPipeAndPassReceiver());
  }

  base::MockRepeatingCallback<void(IntroChoice)>&
  mock_signin_choice_callback() {
    return mock_signin_choice_callback_;
  }
  MockPage& mock_page() { return mock_page_; }
  SignInPromoHandler& handler() { return *handler_; }
  content::BrowserTaskEnvironment& task_environment() {
    return task_environment_;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::MockRepeatingCallback<void(IntroChoice)> mock_signin_choice_callback_;
  testing::StrictMock<MockPage> mock_page_;
  mojo::Remote<intro::mojom::SignInPromoPageHandler> handler_remote_;
  std::unique_ptr<SignInPromoHandler> handler_;
};

TEST_F(SignInPromoHandlerTest, ContinueWithAccountForwarded) {
  EXPECT_CALL(mock_signin_choice_callback(),
              Run(IntroChoice::kContinueWithAccount));
  handler().ContinueWithAccount();
}

TEST_F(SignInPromoHandlerTest, ContinueWithoutAccountForwarded) {
  EXPECT_CALL(mock_signin_choice_callback(),
              Run(IntroChoice::kContinueWithoutAccount));
  handler().ContinueWithoutAccount();
}

TEST_F(SignInPromoHandlerTest, ResetIntroButtonsNotifiesPage) {
  base::test::TestFuture<void> future;
  EXPECT_CALL(mock_page(), OnResetButtons())
      .WillOnce(base::test::RunOnceClosure(future.GetCallback()));
  handler().ResetIntroButtons();
  EXPECT_TRUE(future.Wait());
}

TEST_F(SignInPromoHandlerTest, GetManagedDeviceDisclaimerNonManagedDevice) {
  base::test::TestFuture<std::string> future;
  handler().GetManagedDeviceDisclaimer(
      future.GetCallback<const std::string&>());
  EXPECT_EQ(future.Get(), "");
}

class FakeMachineLevelUserCloudPolicyStore
    : public policy::MachineLevelUserCloudPolicyStore {
 public:
  FakeMachineLevelUserCloudPolicyStore()
      : policy::MachineLevelUserCloudPolicyStore(
            policy::DMToken::CreateValidToken("dummy-token"),
            "dummy-client-id",
            base::FilePath(),
            base::FilePath(),
            base::FilePath(),
            base::FilePath(),
            policy::dm_protocol::kChromeMachineLevelUserCloudPolicyType,
            /*background_task_runner=*/nullptr) {}

  ~FakeMachineLevelUserCloudPolicyStore() override = default;

  // Elevate the protected observer trigger methods in the base class to public
  // so that they can be called directly inside our test bodies.
  using policy::CloudPolicyStore::NotifyStoreError;
  using policy::CloudPolicyStore::NotifyStoreLoaded;
};

class SignInPromoHandlerManagedTest : public SignInPromoHandlerTest {
 public:
  SignInPromoHandlerManagedTest() = default;
  ~SignInPromoHandlerManagedTest() override = default;

  void SetUp() override {
    auto store = std::make_unique<FakeMachineLevelUserCloudPolicyStore>();
    store_ptr_ = store.get();

    manager_ = std::make_unique<policy::MachineLevelUserCloudPolicyManager>(
        std::move(store),
        /*extension_install_store=*/nullptr,
        /*external_data_manager=*/nullptr,
        /*policy_dir=*/base::FilePath(),
        scoped_refptr<base::SequencedTaskRunner>(),
        network::NetworkConnectionTrackerGetter());

    g_browser_process->browser_policy_connector()
        ->SetMachineLevelUserCloudPolicyManagerForTesting(manager_.get());

    CreateHandler(/*is_device_managed=*/true);
  }

  void TearDown() override {
    handler_.reset();
    g_browser_process->browser_policy_connector()
        ->SetMachineLevelUserCloudPolicyManagerForTesting(nullptr);
    store_ptr_ = nullptr;
  }

  FakeMachineLevelUserCloudPolicyStore& store() {
    return CHECK_DEREF(store_ptr_);
  }

 private:
  raw_ptr<FakeMachineLevelUserCloudPolicyStore> store_ptr_;
  std::unique_ptr<policy::MachineLevelUserCloudPolicyManager> manager_;
};

TEST_F(SignInPromoHandlerManagedTest, StoreLoadedBeforeQuery) {
  store().NotifyStoreLoaded();
  ASSERT_TRUE(store().is_initialized());

  base::test::TestFuture<std::string> future;
  handler().GetManagedDeviceDisclaimer(
      future.GetCallback<const std::string&>());

  EXPECT_EQ(future.Get(),
            l10n_util::GetStringUTF8(IDS_FRE_MANAGED_DESCRIPTION));
}

TEST_F(SignInPromoHandlerManagedTest, StoreAlreadyInitialized) {
  store().NotifyStoreLoaded();
  ASSERT_TRUE(store().is_initialized());

  // Recreate the handler after the store is initialized.
  CreateHandler(/*is_device_managed=*/true);

  base::test::TestFuture<std::string> future;
  handler().GetManagedDeviceDisclaimer(
      future.GetCallback<const std::string&>());

  EXPECT_EQ(future.Get(),
            l10n_util::GetStringUTF8(IDS_FRE_MANAGED_DESCRIPTION));
}

TEST_F(SignInPromoHandlerManagedTest, StoreLoadedAsynchronously) {
  base::test::TestFuture<std::string> future;
  handler().GetManagedDeviceDisclaimer(
      future.GetCallback<const std::string&>());

  // The store has not been loaded yet, so the callback shouldn't be called yet.
  EXPECT_FALSE(future.IsReady());

  store().NotifyStoreLoaded();

  EXPECT_EQ(future.Get(),
            l10n_util::GetStringUTF8(IDS_FRE_MANAGED_DESCRIPTION));
}

TEST_F(SignInPromoHandlerManagedTest, StoreErrorAsynchronously) {
  base::test::TestFuture<std::string> future;
  handler().GetManagedDeviceDisclaimer(
      future.GetCallback<const std::string&>());

  EXPECT_FALSE(future.IsReady());

  store().NotifyStoreError();

  EXPECT_EQ(future.Get(),
            l10n_util::GetStringUTF8(IDS_FRE_MANAGED_DESCRIPTION));
}

TEST_F(SignInPromoHandlerManagedTest, StoreLoadTimeout) {
  base::test::TestFuture<std::string> future;
  handler().GetManagedDeviceDisclaimer(
      future.GetCallback<const std::string&>());

  EXPECT_FALSE(future.IsReady());

  // Fast forward by 2.5 seconds, which is the timeout duration.
  task_environment().FastForwardBy(base::Seconds(2.5));

  EXPECT_EQ(future.Get(),
            l10n_util::GetStringUTF8(IDS_FRE_MANAGED_DESCRIPTION));
}

TEST_F(SignInPromoHandlerManagedTest, StoreLoadedWithDeviceManager) {
  ScopedDeviceManagerForTesting scoped_manager("google.com");

  base::test::TestFuture<std::string> future;
  handler().GetManagedDeviceDisclaimer(
      future.GetCallback<const std::string&>());

  store().NotifyStoreLoaded();

  EXPECT_EQ(future.Get(), l10n_util::GetStringFUTF8(
                              IDS_FRE_MANAGED_BY_DESCRIPTION, u"google.com"));
}

}  // namespace
