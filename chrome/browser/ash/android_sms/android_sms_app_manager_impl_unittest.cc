// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/android_sms/android_sms_app_manager_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_simple_task_runner.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/ash/android_sms/android_sms_urls.h"
#include "chrome/browser/ash/android_sms/fake_android_sms_app_setup_controller.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace android_sms {

const char kNewAppId[] = "newAppId";
const char kOldAppId[] = "oldAppId";

GURL GetAndroidMessagesURLOld(bool use_install_url = false) {
  // For this test, consider the staging server to be the "old" URL.
  return GetAndroidMessagesURL(use_install_url, PwaDomain::kStaging);
}

class TestObserver : public AndroidSmsAppManager::Observer {
 public:
  TestObserver() = default;

  TestObserver(const TestObserver&) = delete;
  TestObserver& operator=(const TestObserver&) = delete;

  ~TestObserver() override = default;

  size_t num_installed_app_url_changed_events() {
    return num_installed_app_url_changed_events_;
  }

 private:
  // AndroidSmsAppManager::Observer:
  void OnInstalledAppUrlChanged() override {
    ++num_installed_app_url_changed_events_;
  }

  size_t num_installed_app_url_changed_events_ = 0u;
};

class AndroidSmsAppManagerImplTest : public testing::Test {
 public:
  AndroidSmsAppManagerImplTest(const AndroidSmsAppManagerImplTest&) = delete;
  AndroidSmsAppManagerImplTest& operator=(const AndroidSmsAppManagerImplTest&) =
      delete;

 protected:
  class TestPwaDelegate : public AndroidSmsAppManagerImpl::PwaDelegate {
   public:
    TestPwaDelegate() = default;

    TestPwaDelegate(const TestPwaDelegate&) = delete;
    TestPwaDelegate& operator=(const TestPwaDelegate&) = delete;

    ~TestPwaDelegate() override = default;

    const std::vector<std::string>& opened_app_ids() const {
      return opened_app_ids_;
    }

    const std::vector<std::pair<std::string, std::string>>&
    transfer_item_attribute_params() {
      return transfer_item_attribute_params_;
    }

    // AndroidSmsAppManagerImpl::PwaDelegate:
    void OpenApp(Profile*, const std::string& app_id) override {
      opened_app_ids_.push_back(app_id);
    }

    bool TransferItemAttributes(
        const std::string& from_app_id,
        const std::string& to_app_id,
        app_list::AppListSyncableService* app_list_syncable_service) override {
      transfer_item_attribute_params_.emplace_back(from_app_id, to_app_id);
      return true;
    }

    bool IsAppRegistryReady(Profile* profile) override { return true; }

    void ExecuteOnAppRegistryReady(Profile* profile,
                                   base::OnceClosure task) override {
      test_task_runner_->PostTask(FROM_HERE, std::move(task));
    }

    void RunPendingTasks() { test_task_runner_->RunUntilIdle(); }

   private:
    std::vector<std::string> opened_app_ids_;
    std::vector<std::pair<std::string, std::string>>
        transfer_item_attribute_params_;
    scoped_refptr<base::TestSimpleTaskRunner> test_task_runner_ =
        base::MakeRefCounted<base::TestSimpleTaskRunner>();
  };

  AndroidSmsAppManagerImplTest() = default;
  ~AndroidSmsAppManagerImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    fake_android_sms_app_setup_controller_ =
        std::make_unique<FakeAndroidSmsAppSetupController>();

    test_pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    auto test_pwa_delegate = std::make_unique<TestPwaDelegate>();
    test_pwa_delegate_ = test_pwa_delegate.get();
    android_sms_app_manager_ = std::make_unique<AndroidSmsAppManagerImpl>(
        &profile_, fake_android_sms_app_setup_controller_.get(),
        test_pref_service_.get(), nullptr /* app_list_syncable_service */,
        std::move(test_pwa_delegate));

    test_observer_ = std::make_unique<TestObserver>();
    android_sms_app_manager_->AddObserver(test_observer_.get());
  }

  void TearDown() override {
    android_sms_app_manager_->RemoveObserver(test_observer_.get());
  }

  void CompleteAsyncInitialization() { test_pwa_delegate_->RunPendingTasks(); }

  TestPwaDelegate* test_pwa_delegate() { return test_pwa_delegate_; }

  TestObserver* test_observer() { return test_observer_.get(); }

  FakeAndroidSmsAppSetupController* fake_android_sms_app_setup_controller() {
    return fake_android_sms_app_setup_controller_.get();
  }

  AndroidSmsAppManager* android_sms_app_manager() {
    return android_sms_app_manager_.get();
  }

  sync_preferences::TestingPrefServiceSyncable* test_pref_service() {
    return test_pref_service_.get();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;

  TestingProfile profile_;
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable>
      test_pref_service_;
  std::unique_ptr<FakeAndroidSmsAppSetupController>
      fake_android_sms_app_setup_controller_;

  raw_ptr<TestPwaDelegate, DanglingUntriaged | ExperimentalAsh>
      test_pwa_delegate_;
  std::unique_ptr<TestObserver> test_observer_;

  std::unique_ptr<AndroidSmsAppManagerImpl> android_sms_app_manager_;
};

TEST_F(AndroidSmsAppManagerImplTest, TestSetUpMessages_NoPreviousApp_Fails) {
  CompleteAsyncInitialization();

  android_sms_app_manager()->SetUpAndroidSmsApp();
  fake_android_sms_app_setup_controller()->CompletePendingSetUpAppRequest(
      GetAndroidMessagesURL() /* expected_app_url */,
      GetAndroidMessagesURL(
          true /* use_install_url */) /* expected_install_url */,
      absl::nullopt /* id_for_app */);

  // Verify that no installed app exists and no observers were notified.
  EXPECT_FALSE(fake_android_sms_app_setup_controller()->GetAppMetadataAtUrl(
      GetAndroidMessagesURL(true /* use_install_url */)));
  EXPECT_FALSE(android_sms_app_manager()->GetCurrentAppUrl());
  EXPECT_EQ(0u, test_observer()->num_installed_app_url_changed_events());
}

TEST_F(AndroidSmsAppManagerImplTest,
       TestSetUpMessages_ThenTearDown_NoPreviousApp) {
  CompleteAsyncInitialization();
  const GURL install_url = GetAndroidMessagesURL(true /* use_install_url */);

  android_sms_app_manager()->SetUpAndroidSmsApp();
  fake_android_sms_app_setup_controller()->CompletePendingSetUpAppRequest(
      GetAndroidMessagesURL() /* expected_app_url */,
      install_url /* expected_install_url */, kNewAppId);

  // Verify that the app was installed and observers were notified.
  EXPECT_EQ(kNewAppId, fake_android_sms_app_setup_controller()
                           ->GetAppMetadataAtUrl(GetAndroidMessagesURL(
                               true /* use_install_url */))
                           ->pwa);
  EXPECT_TRUE(fake_android_sms_app_setup_controller()
                  ->GetAppMetadataAtUrl(install_url)
                  ->is_cookie_present);
  EXPECT_EQ(GetAndroidMessagesURL(),
            *android_sms_app_manager()->GetCurrentAppUrl());
  EXPECT_EQ(1u, test_observer()->num_installed_app_url_changed_events());

  // Now, tear down the app, which should remove the DefaultToPersist cookie.
  android_sms_app_manager()->TearDownAndroidSmsApp();
  fake_android_sms_app_setup_controller()->CompletePendingDeleteCookieRequest(
      GetAndroidMessagesURL() /* expected_app_url */,
      GetAndroidMessagesURL(
          true /* use_install_url */) /* expected_install_url */);
  EXPECT_FALSE(fake_android_sms_app_setup_controller()
                   ->GetAppMetadataAtUrl(
                       GetAndroidMessagesURL(true /* use_install_url */))
                   ->is_cookie_present);
}

TEST_F(AndroidSmsAppManagerImplTest, TestSetUpMessagesAndLaunch_NoPreviousApp) {
  CompleteAsyncInitialization();
  const GURL install_url = GetAndroidMessagesURL(true /* use_install_url */);

  android_sms_app_manager()->SetUpAndLaunchAndroidSmsApp();
  fake_android_sms_app_setup_controller()->CompletePendingSetUpAppRequest(
      GetAndroidMessagesURL() /* expected_app_url */,
      install_url /* expected_install_url */, kNewAppId);

  // Verify that the app was installed and observers were notified.
  EXPECT_EQ(kNewAppId, fake_android_sms_app_setup_controller()
                           ->GetAppMetadataAtUrl(GetAndroidMessagesURL(
                               true /* use_install_url */))
                           ->pwa);
  EXPECT_TRUE(fake_android_sms_app_setup_controller()
                  ->GetAppMetadataAtUrl(install_url)
                  ->is_cookie_present);
  EXPECT_EQ(GetAndroidMessagesURL(),
            *android_sms_app_manager()->GetCurrentAppUrl());
  EXPECT_EQ(1u, test_observer()->num_installed_app_url_changed_events());

  // The app should have been launched.
  EXPECT_EQ(kNewAppId, test_pwa_delegate()->opened_app_ids()[0]);
}

TEST_F(AndroidSmsAppManagerImplTest,
       TestSetUpMessages_PreviousAppExists_Fails) {
  // Before completing initialization, install the old app.
  fake_android_sms_app_setup_controller()->SetAppAtUrl(
      GetAndroidMessagesURLOld(true /* use_install_url */), kOldAppId);
  CompleteAsyncInitialization();

  // This should trigger the new app to be installed; fail this installation.
  // This simulates a situation which could occur if the user signs in with the
  // flag enabled but is offline and thus unable to install the new PWA.
  fake_android_sms_app_setup_controller()->CompletePendingSetUpAppRequest(
      GetAndroidMessagesURL() /* expected_app_url */,
      GetAndroidMessagesURL(
          true /* use_install_url */) /* expected_install_url */,
      absl::nullopt /* id_for_app */);

  // Verify that the new app was not installed and no observers were notified.
  EXPECT_FALSE(fake_android_sms_app_setup_controller()->GetAppMetadataAtUrl(
      GetAndroidMessagesURL(true /* use_install_url */)));
  EXPECT_EQ(0u, test_observer()->num_installed_app_url_changed_events());

  // The old app should still be true.
  EXPECT_EQ(GetAndroidMessagesURLOld(),
            *android_sms_app_manager()->GetCurrentAppUrl());
  EXPECT_EQ(kOldAppId, fake_android_sms_app_setup_controller()
                           ->GetAppMetadataAtUrl(GetAndroidMessagesURLOld(
                               true /* use_install_url */))
                           ->pwa);
  EXPECT_TRUE(fake_android_sms_app_setup_controller()
                  ->GetAppMetadataAtUrl(
                      GetAndroidMessagesURLOld(true /* use_install_url */))
                  ->is_cookie_present);
}

TEST_F(AndroidSmsAppManagerImplTest,
       TestSetUpMessages_ThenTearDown_PreviousAppExists) {
  // Before completing initialization, install the old app.
  fake_android_sms_app_setup_controller()->SetAppAtUrl(
      GetAndroidMessagesURLOld(true /* use_install_url */), kOldAppId);
  CompleteAsyncInitialization();

  // This should trigger the new app to be installed.
  const GURL install_url = GetAndroidMessagesURL(true /* use_install_url */);
  fake_android_sms_app_setup_controller()->CompletePendingSetUpAppRequest(
      GetAndroidMessagesURL() /* expected_app_url */,
      install_url /* expected_install_url */, kNewAppId /* id_for_app */);

  // Verify that the app was installed and attributes were transferred. By this
  // point, observers should not have been notified yet since the old app was
  // not yet installed.
  EXPECT_EQ(kNewAppId, fake_android_sms_app_setup_controller()
                           ->GetAppMetadataAtUrl(GetAndroidMessagesURL(
                               true /* use_install_url */))
                           ->pwa);
  EXPECT_TRUE(fake_android_sms_app_setup_controller()
                  ->GetAppMetadataAtUrl(
                      GetAndroidMessagesURL(true /* use_install_url */))
                  ->is_cookie_present);
  EXPECT_EQ(GetAndroidMessagesURL(),
            *android_sms_app_manager()->GetCurrentAppUrl());
  EXPECT_EQ(std::make_pair(std::string(kOldAppId), std::string(kNewAppId)),
            test_pwa_delegate()->transfer_item_attribute_params()[0]);
  EXPECT_EQ(0u, test_observer()->num_installed_app_url_changed_events());

  // Now, complete uninstallation of the old app; this should trigger observers
  // to be notified.
  fake_android_sms_app_setup_controller()->CompleteRemoveAppRequest(
      GetAndroidMessagesURLOld() /* expected_app_url */,
      GetAndroidMessagesURLOld(
          true /* use_install_url */) /* expected_install_url */,
      GetAndroidMessagesURL() /* expected_migrated_to_app_url */,
      true /* success */);
  EXPECT_EQ(1u, test_observer()->num_installed_app_url_changed_events());
}

TEST_F(AndroidSmsAppManagerImplTest, TestGetCurrentAppUrl) {
  // Enable staging flag and install both prod and staging apps.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kUseMessagesStagingUrl);
  fake_android_sms_app_setup_controller()->SetAppAtUrl(
      GetAndroidMessagesURL(true /* use_install_url */, PwaDomain::kProdGoogle),
      kOldAppId);
  fake_android_sms_app_setup_controller()->SetAppAtUrl(
      GetAndroidMessagesURL(true /* use_install_url */, PwaDomain::kStaging),
      kNewAppId);
  CompleteAsyncInitialization();

  // When both apps are installed GetCurrentAppUrl should always
  // return the preferred app.
  EXPECT_EQ(
      GetAndroidMessagesURL(false /* use_install_url */, PwaDomain::kStaging),
      android_sms_app_manager()->GetCurrentAppUrl());
}

}  // namespace android_sms
}  // namespace ash
