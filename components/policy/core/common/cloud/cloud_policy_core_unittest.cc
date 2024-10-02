// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/cloud_policy_core.h"

#include <memory>

#include "base/base64.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_refresh_scheduler.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class CloudPolicyCoreTest : public testing::Test,
                            public CloudPolicyCore::Observer {
 public:
  CloudPolicyCoreTest(const CloudPolicyCoreTest&) = delete;
  CloudPolicyCoreTest& operator=(const CloudPolicyCoreTest&) = delete;

 protected:
  CloudPolicyCoreTest() {
    core_ = std::make_unique<CloudPolicyCore>(
        dm_protocol::kChromeUserPolicyType, std::string(), &store_,
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        network::TestNetworkConnectionTracker::CreateGetter());
    prefs_.registry()->RegisterIntegerPref(
        policy_prefs::kUserPolicyRefreshRate,
        CloudPolicyRefreshScheduler::kDefaultRefreshDelayMs);
    core_->AddObserver(this);
  }

  ~CloudPolicyCoreTest() override {
    if (core_)
      core_->RemoveObserver(this);
  }

  void OnCoreConnected(CloudPolicyCore* core) override {
    // Make sure core is connected at callback time.
    if (core_->client())
      core_connected_callback_count_++;
    else
      bad_callback_count_++;
  }

  void OnRefreshSchedulerStarted(CloudPolicyCore* core) override {
    // Make sure refresh scheduler is started at callback time.
    if (core_->refresh_scheduler())
      refresh_scheduler_started_callback_count_++;
    else
      bad_callback_count_++;
  }

  void OnCoreDisconnecting(CloudPolicyCore* core) override {
    // Make sure core is still connected at callback time.
    if (core_->client())
      core_disconnecting_callback_count_++;
    else
      bad_callback_count_++;
  }

  void OnCoreDestruction(policy::CloudPolicyCore* core) override {
    core_destruction_callback_count_++;
    core->RemoveObserver(this);
    CloudPolicyCore::Observer::OnCoreDestruction(core);
  }

  base::test::SingleThreadTaskEnvironment task_environment_;

  TestingPrefServiceSimple prefs_;
  MockCloudPolicyStore store_;
  std::unique_ptr<CloudPolicyCore> core_;

  int core_connected_callback_count_ = 0;
  int refresh_scheduler_started_callback_count_ = 0;
  int core_disconnecting_callback_count_ = 0;
  int core_destruction_callback_count_ = 0;
  int bad_callback_count_ = 0;
};

TEST_F(CloudPolicyCoreTest, ConnectAndDisconnectAndDestroy) {
  EXPECT_TRUE(core_->store());
  EXPECT_FALSE(core_->client());
  EXPECT_FALSE(core_->service());
  EXPECT_FALSE(core_->refresh_scheduler());

  // Connect() brings up client and service.
  core_->Connect(
      std::unique_ptr<CloudPolicyClient>(new MockCloudPolicyClient()));
  EXPECT_TRUE(core_->client());
  EXPECT_TRUE(core_->service());
  EXPECT_FALSE(core_->refresh_scheduler());
  EXPECT_EQ(1, core_connected_callback_count_);
  EXPECT_EQ(0, refresh_scheduler_started_callback_count_);
  EXPECT_EQ(0, core_disconnecting_callback_count_);

  // Disconnect() goes back to no client and service.
  core_->Disconnect();
  EXPECT_FALSE(core_->client());
  EXPECT_FALSE(core_->service());
  EXPECT_FALSE(core_->refresh_scheduler());
  EXPECT_EQ(1, core_connected_callback_count_);
  EXPECT_EQ(0, refresh_scheduler_started_callback_count_);
  EXPECT_EQ(1, core_disconnecting_callback_count_);

  // Calling Disconnect() twice doesn't do bad things.
  core_->Disconnect();
  EXPECT_FALSE(core_->client());
  EXPECT_FALSE(core_->service());
  EXPECT_FALSE(core_->refresh_scheduler());
  EXPECT_EQ(1, core_connected_callback_count_);
  EXPECT_EQ(0, refresh_scheduler_started_callback_count_);
  EXPECT_EQ(1, core_disconnecting_callback_count_);
  EXPECT_EQ(0, core_destruction_callback_count_);
  EXPECT_EQ(0, bad_callback_count_);

  // Destruction callback is called.
  core_.reset();
  EXPECT_EQ(1, core_connected_callback_count_);
  EXPECT_EQ(0, refresh_scheduler_started_callback_count_);
  EXPECT_EQ(1, core_disconnecting_callback_count_);
  EXPECT_EQ(1, core_destruction_callback_count_);
  EXPECT_EQ(0, bad_callback_count_);
}

TEST_F(CloudPolicyCoreTest, RefreshScheduler) {
  EXPECT_FALSE(core_->refresh_scheduler());
  core_->Connect(
      std::unique_ptr<CloudPolicyClient>(new MockCloudPolicyClient()));
  core_->StartRefreshScheduler();
  ASSERT_TRUE(core_->refresh_scheduler());

  int default_refresh_delay =
      core_->refresh_scheduler()->GetActualRefreshDelay();

  const int kRefreshRate = 1000 * 60 * 60;
  prefs_.SetInteger(policy_prefs::kUserPolicyRefreshRate, kRefreshRate);
  core_->TrackRefreshDelayPref(&prefs_, policy_prefs::kUserPolicyRefreshRate);
  EXPECT_EQ(kRefreshRate, core_->refresh_scheduler()->GetActualRefreshDelay());

  prefs_.ClearPref(policy_prefs::kUserPolicyRefreshRate);
  EXPECT_EQ(default_refresh_delay,
            core_->refresh_scheduler()->GetActualRefreshDelay());

  EXPECT_EQ(1, core_connected_callback_count_);
  EXPECT_EQ(1, refresh_scheduler_started_callback_count_);
  EXPECT_EQ(0, core_disconnecting_callback_count_);
  EXPECT_EQ(0, bad_callback_count_);
}

TEST_F(CloudPolicyCoreTest, DmProtocolBase64Constants) {
  std::string encoded;
  base::Base64Encode(dm_protocol::kChromeMachineLevelUserCloudPolicyType,
                     &encoded);
  EXPECT_EQ(encoded, dm_protocol::kChromeMachineLevelUserCloudPolicyTypeBase64);
}

TEST_F(CloudPolicyCoreTest, DestroyWithoutConnecting) {
  core_.reset();
  EXPECT_EQ(0, core_connected_callback_count_);
  EXPECT_EQ(0, refresh_scheduler_started_callback_count_);
  EXPECT_EQ(0, core_disconnecting_callback_count_);
  EXPECT_EQ(1, core_destruction_callback_count_);
  EXPECT_EQ(0, bad_callback_count_);
}

}  // namespace policy
