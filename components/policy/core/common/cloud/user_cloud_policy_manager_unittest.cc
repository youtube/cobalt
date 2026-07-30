// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"

#include <memory>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/mock_user_cloud_policy_store.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/features.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema_registry.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

using testing::AnyNumber;
using testing::AtLeast;
using testing::Mock;
using testing::_;

namespace policy {
namespace {

class UserCloudPolicyManagerTest : public testing::Test {
 public:
  UserCloudPolicyManagerTest(const UserCloudPolicyManagerTest&) = delete;
  UserCloudPolicyManagerTest& operator=(const UserCloudPolicyManagerTest&) =
      delete;

 protected:
  UserCloudPolicyManagerTest()
      : store_(nullptr), extension_install_store_(nullptr) {}

  void SetUp() override {
    // Set up a policy map for testing.
    policy_map_.Set("key", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                    POLICY_SOURCE_CLOUD, base::Value("value"), nullptr);
    expected_bundle_.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string())) =
        policy_map_.Clone();
  }

  void TearDown() override {
    if (manager_) {
      manager_->RemoveObserver(&observer_);
      manager_->Shutdown();
    }
  }

  void CreateManager() {
    store_ =
        new MockUserCloudPolicyStore(dm_protocol::GetChromeUserPolicyType());
    EXPECT_CALL(*store_, Load());
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
    extension_install_store_ = new MockUserCloudPolicyStore(
        dm_protocol::kChromeExtensionInstallUserCloudPolicyType);
    // Never allowed unless explicitly initialized.
    EXPECT_CALL(*extension_install_store_, Load()).Times(0);
#endif
    const auto task_runner = task_environment_.GetMainThreadTaskRunner();
    manager_ = std::make_unique<UserCloudPolicyManager>(
        std::unique_ptr<UserCloudPolicyStore>(store_),
        std::unique_ptr<UserCloudPolicyStore>(extension_install_store_),
        base::FilePath(), std::unique_ptr<CloudExternalDataManager>(),
        task_runner, network::TestNetworkConnectionTracker::CreateGetter());
    manager_->Init(&schema_registry_);
    manager_->AddObserver(&observer_);
    Mock::VerifyAndClearExpectations(store_);
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
    Mock::VerifyAndClearExpectations(extension_install_store_);
#endif
  }

  // Needs to be the first member.
  base::test::TaskEnvironment task_environment_;

  // Convenience policy objects.
  PolicyMap policy_map_;
  PolicyBundle expected_bundle_;

  // Policy infrastructure.
  SchemaRegistry schema_registry_;
  MockConfigurationPolicyObserver observer_;
  raw_ptr<MockUserCloudPolicyStore, DanglingUntriaged> store_;  // Not owned.
  raw_ptr<MockUserCloudPolicyStore, DanglingUntriaged>
      extension_install_store_;  // Not owned.
  std::unique_ptr<UserCloudPolicyManager> manager_;
};

TEST_F(UserCloudPolicyManagerTest, DisconnectAndRemovePolicy) {
  // Load policy, make sure it goes away when DisconnectAndRemovePolicy() is
  // called.
  CreateManager();
  store_->policy_map_ = policy_map_.Clone();
  EXPECT_CALL(observer_, OnUpdatePolicy(manager_.get())).Times(2);
  store_->NotifyStoreLoaded();
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  extension_install_store_->NotifyStoreLoaded();
#endif
  EXPECT_TRUE(expected_bundle_.Equals(manager_->policies()));
  EXPECT_TRUE(manager_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
  EXPECT_CALL(*store_, Clear());
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  EXPECT_CALL(*extension_install_store_, Clear());
#endif
  manager_->DisconnectAndRemovePolicy();
  EXPECT_FALSE(manager_->core()->service());
}

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
TEST_F(UserCloudPolicyManagerTest, DeleteExtensionInstallPolicyWhenDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kEnableExtensionInstallPolicyFetching);

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath policy_dir = temp_dir.GetPath().AppendASCII("Policy");
  ASSERT_TRUE(base::CreateDirectory(policy_dir));

  base::FilePath policy_file =
      policy_dir.AppendASCII("User Extension Install Policy");
  base::FilePath key_file =
      policy_dir.AppendASCII("User Extension Install Signing Key");

  ASSERT_TRUE(base::WriteFile(policy_file, "policy_data"));
  ASSERT_TRUE(base::WriteFile(key_file, "key_data"));

  std::unique_ptr<UserCloudPolicyManager> manager =
      UserCloudPolicyManager::Create(
          temp_dir.GetPath(), &schema_registry_,
          /*force_immediate_load=*/false,
          task_environment_.GetMainThreadTaskRunner(),
          network::TestNetworkConnectionTracker::CreateGetter());

  task_environment_.RunUntilIdle();

  EXPECT_FALSE(base::PathExists(policy_file));
  EXPECT_FALSE(base::PathExists(key_file));

  manager->Shutdown();
}
#endif

}  // namespace
}  // namespace policy
