// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/enterprise/connectors/reporting/extension_install_event_router.h"

#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"
#include "chrome/browser/enterprise/connectors/test/mock_realtime_reporting_client.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/enterprise/connectors/core/reporting_service_settings.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::ByRef;
using ::testing::Eq;
using ::testing::Return;

MATCHER_P(EqualsProto, message, "") {
  std::string expected_serialized, actual_serialized;
  message.SerializeToString(&expected_serialized);
  arg.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}

namespace enterprise_connectors {

namespace {

constexpr char kFakeExtensionId[] = "fake-extension-id";
constexpr char kFakeExtensionName[] = "Foo extension";
constexpr char kFakeExtensionDescription[] = "Does Foo";
constexpr char kFakeProfileUsername[] = "Tiamat";
constexpr char kFakeInstallAction[] = "INSTALL";
constexpr char kFakeUpdateAction[] = "UPDATE";
constexpr char kFakeUninstallAction[] = "UNINSTALL";
constexpr char kFakeExtensionVersion[] = "1";
constexpr char kFakeExtensionSource[] = "EXTERNAL";
constexpr auto kFakeExtensionSourceEnum =
    ::chrome::cros::reporting::proto::BrowserExtensionInstallEvent::
        ExtensionSource::BrowserExtensionInstallEvent_ExtensionSource_EXTERNAL;

}  // namespace

class ExtensionInstallEventRouterTest
    : public testing::Test,
      public testing::WithParamInterface<bool> {
 public:
  ExtensionInstallEventRouterTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    if (use_proto_format()) {
      feature_list_.InitAndEnableFeature(
          policy::kUploadRealtimeReportingEventsUsingProto);
    } else {
      feature_list_.InitAndDisableFeature(
          policy::kUploadRealtimeReportingEventsUsingProto);
    }

    EXPECT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile(kFakeProfileUsername);
    policy::SetDMTokenForTesting(
        policy::DMToken::CreateValidToken("fake-token"));

    RealtimeReportingClientFactory::GetInstance()->SetTestingFactory(
        profile_, base::BindRepeating(&test::MockRealtimeReportingClient::
                                          CreateMockRealtimeReportingClient));
    extensionInstallEventRouter_ =
        std::make_unique<ExtensionInstallEventRouter>(profile_);

    mockRealtimeReportingClient_ =
        static_cast<test::MockRealtimeReportingClient*>(
            RealtimeReportingClientFactory::GetForProfile(profile_));
    mockRealtimeReportingClient_->SetProfileUserNameForTesting(
        kFakeProfileUsername);

    test::SetOnSecurityEventReporting(
        profile_->GetPrefs(), /*enabled=*/true,
        /*enabled_event_names=*/std::set<std::string>(),
        /*enabled_opt_in_events=*/
        std::map<std::string, std::vector<std::string>>());
    // Set a mock cloud policy client in the router.
    client_ = std::make_unique<policy::MockCloudPolicyClient>();
    client_->SetDMToken("fake-token");
    mockRealtimeReportingClient_->SetBrowserCloudPolicyClientForTesting(
        client_.get());

    base::Value::Dict manifest;
    manifest.Set(extensions::manifest_keys::kName, kFakeExtensionName);
    manifest.Set(extensions::manifest_keys::kVersion, "1");
    manifest.Set(extensions::manifest_keys::kManifestVersion, 2);
    manifest.Set(extensions::manifest_keys::kDescription,
                 kFakeExtensionDescription);

    std::string error;
    extension_chrome_ = extensions::Extension::Create(
        base::FilePath(), extensions::mojom::ManifestLocation::kUnpacked,
        manifest, extensions::Extension::NO_FLAGS, kFakeExtensionId, &error);
  }

  void TearDown() override {
    mockRealtimeReportingClient_->SetBrowserCloudPolicyClientForTesting(
        nullptr);
  }

  bool use_proto_format() { return GetParam();}

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<TestingProfile> profile_;
  std::unique_ptr<policy::MockCloudPolicyClient> client_;

  scoped_refptr<extensions::Extension> extension_chrome_;
  raw_ptr<test::MockRealtimeReportingClient> mockRealtimeReportingClient_;
  std::unique_ptr<ExtensionInstallEventRouter> extensionInstallEventRouter_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(ExtensionInstallEventRouterTest, CheckInstallEventReported) {
  ::chrome::cros::reporting::proto::Event expectedEventProto;
  base::Value::Dict expectedEvent;

  if (use_proto_format()) {
    auto* extension_event =
        expectedEventProto.mutable_browser_extension_install_event();

    extension_event->set_id(kFakeExtensionId);
    extension_event->set_name(kFakeExtensionName);
    extension_event->set_description(kFakeExtensionDescription);
    extension_event->set_extension_action_type(
        ::chrome::cros::reporting::proto::BrowserExtensionInstallEvent::
            ExtensionAction::
                BrowserExtensionInstallEvent_ExtensionAction_INSTALL);
    extension_event->set_extension_version(kFakeExtensionVersion);
    extension_event->set_extension_source(kFakeExtensionSourceEnum);

    EXPECT_CALL(*mockRealtimeReportingClient_,
                ReportEvent(EqualsProto(expectedEventProto), _))
        .Times(1);
  } else {
    expectedEvent.Set("id", kFakeExtensionId);
    expectedEvent.Set("name", kFakeExtensionName);
    expectedEvent.Set("description", kFakeExtensionDescription);
    expectedEvent.Set("extension_action_type", kFakeInstallAction);
    expectedEvent.Set("extension_version", kFakeExtensionVersion);
    expectedEvent.Set("extension_source", kFakeExtensionSource);

    EXPECT_CALL(*mockRealtimeReportingClient_,
                ReportRealtimeEvent(kExtensionInstallEvent, _,
                                    Eq(ByRef(expectedEvent))))
        .Times(1);
  }
  extensionInstallEventRouter_->OnExtensionInstalled(
      nullptr, extension_chrome_.get(), false);
}

TEST_P(ExtensionInstallEventRouterTest, CheckUpdateEventReported) {
  ::chrome::cros::reporting::proto::Event expectedEventProto;
  base::Value::Dict expectedEvent;

  if (use_proto_format()) {
    auto* extension_event =
        expectedEventProto.mutable_browser_extension_install_event();

    extension_event->set_id(kFakeExtensionId);
    extension_event->set_name(kFakeExtensionName);
    extension_event->set_description(kFakeExtensionDescription);
    extension_event->set_extension_action_type(
        ::chrome::cros::reporting::proto::BrowserExtensionInstallEvent::
            ExtensionAction::
                BrowserExtensionInstallEvent_ExtensionAction_UPDATE);
    extension_event->set_extension_version(kFakeExtensionVersion);
    extension_event->set_extension_source(kFakeExtensionSourceEnum);

    EXPECT_CALL(*mockRealtimeReportingClient_,
                ReportEvent(EqualsProto(expectedEventProto), _))
        .Times(1);
  } else {
    expectedEvent.Set("id", kFakeExtensionId);
    expectedEvent.Set("name", kFakeExtensionName);
    expectedEvent.Set("description", kFakeExtensionDescription);
    expectedEvent.Set("extension_action_type", kFakeUpdateAction);
    expectedEvent.Set("extension_version", kFakeExtensionVersion);
    expectedEvent.Set("extension_source", kFakeExtensionSource);

    EXPECT_CALL(*mockRealtimeReportingClient_,
                ReportRealtimeEvent(kExtensionInstallEvent, _,
                                    Eq(ByRef(expectedEvent))))
        .Times(1);
  }

  extensionInstallEventRouter_->OnExtensionInstalled(
      nullptr, extension_chrome_.get(), true);
}

TEST_P(ExtensionInstallEventRouterTest, CheckUninstallEventReported) {
  ::chrome::cros::reporting::proto::Event expectedEventProto;
  base::Value::Dict expectedEvent;

  if (use_proto_format()) {
    auto* extension_event =
        expectedEventProto.mutable_browser_extension_install_event();

    extension_event->set_id(kFakeExtensionId);
    extension_event->set_name(kFakeExtensionName);
    extension_event->set_description(kFakeExtensionDescription);
    extension_event->set_extension_action_type(
        ::chrome::cros::reporting::proto::BrowserExtensionInstallEvent::
            ExtensionAction::
                BrowserExtensionInstallEvent_ExtensionAction_UNINSTALL);
    extension_event->set_extension_version(kFakeExtensionVersion);
    extension_event->set_extension_source(kFakeExtensionSourceEnum);

    EXPECT_CALL(*mockRealtimeReportingClient_,
                ReportEvent(EqualsProto(expectedEventProto), _))
        .Times(1);
  } else {
    expectedEvent.Set("id", kFakeExtensionId);
    expectedEvent.Set("name", kFakeExtensionName);
    expectedEvent.Set("description", kFakeExtensionDescription);
    expectedEvent.Set("extension_action_type", kFakeUninstallAction);
    expectedEvent.Set("extension_version", kFakeExtensionVersion);
    expectedEvent.Set("extension_source", kFakeExtensionSource);

    EXPECT_CALL(*mockRealtimeReportingClient_,
                ReportRealtimeEvent(kExtensionInstallEvent, _,
                                    Eq(ByRef(expectedEvent))))
        .Times(1);
  }
  extensionInstallEventRouter_->OnExtensionUninstalled(
      nullptr, extension_chrome_.get(),
      extensions::UNINSTALL_REASON_FOR_TESTING);
}

INSTANTIATE_TEST_SUITE_P(, ExtensionInstallEventRouterTest, ::testing::Bool());

}  // namespace enterprise_connectors
