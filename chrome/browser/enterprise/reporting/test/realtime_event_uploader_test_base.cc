// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/test/realtime_event_uploader_test_base.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/mock_cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/mock_user_cloud_policy_store.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "services/network/test/test_network_connection_tracker.h"

namespace enterprise_reporting {

MockRealtimeReportingClient::MockRealtimeReportingClient(
    content::BrowserContext* context)
    : enterprise_connectors::RealtimeReportingClient(context) {}

MockRealtimeReportingClient::~MockRealtimeReportingClient() = default;

RealtimeEventUploaderTestBase::RealtimeEventUploaderTestBase() = default;

RealtimeEventUploaderTestBase::~RealtimeEventUploaderTestBase() = default;

void RealtimeEventUploaderTestBase::SetUp() {
  profile_manager_ = std::make_unique<TestingProfileManager>(
      TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(profile_manager_->SetUp());

  fake_browser_dm_token_storage_ =
      std::make_unique<policy::FakeBrowserDMTokenStorage>();
  policy::BrowserDMTokenStorage::SetForTesting(
      fake_browser_dm_token_storage_.get());
}

void RealtimeEventUploaderTestBase::TearDown() {
  TestingBrowserProcess::GetGlobal()
      ->browser_policy_connector()
      ->SetDeviceAffiliatedIdsForTesting({});
  policy::BrowserDMTokenStorage::SetForTesting(nullptr);
}

void RealtimeEventUploaderTestBase::SetBrowserManaged(
    bool is_managed,
    const std::string& dm_token) {
  if (is_managed) {
    fake_browser_dm_token_storage_->SetDMToken(dm_token);
    fake_browser_dm_token_storage_->SetClientId("browser_client_id");
  }
}

TestingProfile* RealtimeEventUploaderTestBase::CreateProfile(
    const std::string& name,
    bool is_managed,
    bool is_affiliated,
    bool create_reporting_client) {
  TestingProfile::Builder builder;

  if (is_managed) {
    auto store = std::make_unique<policy::MockUserCloudPolicyStore>(
        policy::dm_protocol::GetChromeUserPolicyType());
    auto policy_data = std::make_unique<enterprise_management::PolicyData>();
    policy_data->set_request_token("user_dm_token_" + name);
    store->set_policy_data_for_testing(std::move(policy_data));

    auto manager = std::make_unique<policy::UserCloudPolicyManager>(
        std::move(store),
        /*extension_install_store=*/nullptr, base::FilePath(),
        std::make_unique<
            testing::NiceMock<policy::MockCloudExternalDataManager>>(),
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        base::BindRepeating([]() -> network::NetworkConnectionTracker* {
          return network::TestNetworkConnectionTracker::GetInstance();
        }));
    builder.SetUserCloudPolicyManager(std::move(manager));
  }

  builder.SetProfileName(name);

  if (create_reporting_client) {
    builder.AddTestingFactory(
        enterprise_connectors::RealtimeReportingClientFactory::GetInstance(),
        base::BindRepeating([](content::BrowserContext* context)
                                -> std::unique_ptr<KeyedService> {
          return std::make_unique<MockRealtimeReportingClient>(context);
        }));
  }

  // Register the profile with the manager to ensure it's tracked and destroyed.
  // This avoids manual memory management while satisfying cross-platform needs.
  std::unique_ptr<TestingProfile> profile_ptr = builder.Build();
  TestingProfile* profile = profile_ptr.get();
  profile_manager_->profile_manager()->RegisterTestingProfile(
      std::move(profile_ptr), /*add_to_storage=*/true);

  if (is_affiliated) {
    profile->GetProfilePolicyConnector()->SetUserAffiliationIdsForTesting(
        {"affiliation_id"});
    TestingBrowserProcess::GetGlobal()
        ->browser_policy_connector()
        ->SetDeviceAffiliatedIdsForTesting({"affiliation_id"});
  }

  return profile;
}

MockRealtimeReportingClient* RealtimeEventUploaderTestBase::GetMockClient(
    Profile* profile) {
  return static_cast<MockRealtimeReportingClient*>(
      enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(
          profile));
}

}  // namespace enterprise_reporting
