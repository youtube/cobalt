// Copyright 2023 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/browser/metrics/cobalt_metrics_service_client.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/metrics/user_metrics.h"
#include "base/test/test_simple_task_runner.h"
#include "cobalt/browser/metrics/cobalt_enabled_state_provider.h"
#include "cobalt/browser/metrics/cobalt_metrics_log_uploader.h"
#include "components/metrics/client_info.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace cobalt {
namespace browser {
namespace metrics {

void TestStoreMetricsClientInfo(const ::metrics::ClientInfo& client_info) {
  // ClientInfo is a way to get data into the metrics component, but goes unused
  // in Cobalt. Do nothing with it for now.
}

std::unique_ptr<::metrics::ClientInfo> TestLoadMetricsClientInfo() {
  // ClientInfo is a way to get data into the metrics component, but goes unused
  // in Cobalt.
  return nullptr;
}

class CobaltMetricsServiceClientTest : public ::testing::Test {
 public:
  CobaltMetricsServiceClientTest() {}
  CobaltMetricsServiceClientTest(const CobaltMetricsServiceClientTest&) =
      delete;
  CobaltMetricsServiceClientTest& operator=(
      const CobaltMetricsServiceClientTest&) = delete;

  void SetUp() override {
    task_runner_ = base::MakeRefCounted<base::TestSimpleTaskRunner>();
    // Required by MetricsServiceClient.
    base::SetRecordActionTaskRunner(task_runner_);
    enabled_state_provider_ =
        std::make_unique<CobaltEnabledStateProvider>(false, false);
    ::metrics::MetricsService::RegisterPrefs(prefs_.registry());
    metrics_state_manager_ = ::metrics::MetricsStateManager::Create(
        &prefs_, enabled_state_provider_.get(), std::wstring(),
        base::BindRepeating(&TestStoreMetricsClientInfo),
        base::BindRepeating(&TestLoadMetricsClientInfo));

    client_ = std::make_unique<CobaltMetricsServiceClient>(
        metrics_state_manager_.get(), &prefs_);
  }

  void TearDown() override {}


 protected:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  TestingPrefServiceSimple prefs_;
  std::unique_ptr<CobaltMetricsServiceClient> client_;
  std::unique_ptr<::metrics::MetricsStateManager> metrics_state_manager_;
  std::unique_ptr<CobaltEnabledStateProvider> enabled_state_provider_;
};

TEST_F(CobaltMetricsServiceClientTest, UkmServiceConstructedProperly) {
  // TODO(b/284467142): Add UKM tests when support is added.
  ASSERT_EQ(client_->GetUkmService(), nullptr);
  ASSERT_FALSE(client_->SyncStateAllowsUkm());
  ASSERT_FALSE(client_->SyncStateAllowsExtensionUkm());
}

// TODO(b/286884542): If we add system profile info, update this test.
TEST_F(CobaltMetricsServiceClientTest, TestStubbedSystemFields) {
  ASSERT_EQ(client_->GetProduct(), 0);
  ASSERT_EQ(client_->GetApplicationLocale(), "en-US");
  std::string brand_code;
  ASSERT_FALSE(client_->GetBrand(&brand_code));
  ASSERT_EQ(brand_code.size(), 0);
  ASSERT_EQ(client_->GetChannel(),
            ::metrics::SystemProfileProto::CHANNEL_UNKNOWN);
  ASSERT_EQ(client_->GetVersionString(), "1.0");
  ASSERT_EQ(client_->GetMetricsServerUrl(), "");
  ASSERT_EQ(client_->GetInsecureMetricsServerUrl(), "");
  ASSERT_FALSE(client_->IsReportingPolicyManaged());
  ASSERT_EQ(client_->GetMetricsReportingDefaultState(),
            ::metrics::EnableMetricsDefault::OPT_OUT);
  ASSERT_FALSE(client_->IsUMACellularUploadLogicEnabled());
  ASSERT_FALSE(client_->AreNotificationListenersEnabledOnAllProfiles());
  ASSERT_EQ(client_->GetAppPackageName(), "");
}

TEST_F(CobaltMetricsServiceClientTest, UploadIntervalCanBeOverriden) {
  ASSERT_EQ(client_->GetStandardUploadInterval().InSeconds(), 300);
  client_->SetUploadInterval(42);
  ASSERT_EQ(client_->GetStandardUploadInterval().InSeconds(), 42);
  client_->SetUploadInterval(UINT32_MAX);
  // UINT32_MAX is the sentinel value to revert back to the default of 5 min.
  ASSERT_EQ(client_->GetStandardUploadInterval().InSeconds(), 300);
}

TEST_F(CobaltMetricsServiceClientTest, UploadIntervalOfZeroIsIgnored) {
  ASSERT_EQ(client_->GetStandardUploadInterval().InSeconds(), 300);
  client_->SetUploadInterval(0);
  ASSERT_EQ(client_->GetStandardUploadInterval().InSeconds(), 300);
}

}  // namespace metrics
}  // namespace browser
}  // namespace cobalt
