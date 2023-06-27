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

#include "cobalt/browser/metrics/cobalt_metrics_services_manager_client.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/metrics/user_metrics.h"
#include "base/test/test_simple_task_runner.h"
#include "components/metrics/metrics_service_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace browser {
namespace metrics {


class CobaltMetricsServicesManagerClientTest : public ::testing::Test {
 public:
  void SetUp() override {
    task_runner_ = base::MakeRefCounted<base::TestSimpleTaskRunner>();
    // Required by MetricsServiceClient.
    base::SetRecordActionTaskRunner(task_runner_);
    client_ = std::make_unique<CobaltMetricsServicesManagerClient>();
  }

  void TearDown() override {}


 protected:
  std::unique_ptr<CobaltMetricsServicesManagerClient> client_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
};

TEST_F(CobaltMetricsServicesManagerClientTest,
       EnabledStateProviderStateIsPreserved) {
  auto provider = client_->GetEnabledStateProvider();
  provider->SetConsentGiven(true);
  provider->SetReportingEnabled(true);
  EXPECT_TRUE(client_->IsMetricsConsentGiven());
  EXPECT_TRUE(client_->IsMetricsReportingEnabled());

  provider->SetConsentGiven(false);
  provider->SetReportingEnabled(false);
  EXPECT_FALSE(client_->IsMetricsConsentGiven());
  EXPECT_FALSE(client_->IsMetricsReportingEnabled());
}

TEST_F(CobaltMetricsServicesManagerClientTest, ForceEnabledStateIsCorrect) {
  // TODO(b/286091096): Add tests for force enabling on the command-line.
  EXPECT_FALSE(client_->IsMetricsReportingForceEnabled());
}

TEST_F(CobaltMetricsServicesManagerClientTest,
       MetricsServiceClientAndStateManagerAreConstructedProperly) {
  auto metrics_client = client_->CreateMetricsServiceClient();
  ASSERT_NE(metrics_client, nullptr);

  auto provider = client_->GetEnabledStateProvider();
  provider->SetConsentGiven(true);
  provider->SetReportingEnabled(true);

  ASSERT_TRUE(
      client_->GetMetricsStateManagerForTesting()->IsMetricsReportingEnabled());

  provider->SetConsentGiven(false);
  provider->SetReportingEnabled(false);

  ASSERT_FALSE(
      client_->GetMetricsStateManagerForTesting()->IsMetricsReportingEnabled());
}

}  // namespace metrics
}  // namespace browser
}  // namespace cobalt
