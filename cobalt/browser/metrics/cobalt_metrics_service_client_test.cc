// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {


class CobaltMetricsServiceClientTest : public ::testing::Test {
 public:
  CobaltMetricsServiceClientTest() = default;
 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

// Verifies that...
TEST_F(CobaltMetricsServiceClientTest, ConstructDestruct) {

  auto metrics = std::make_unique<CobaltMetricsServiceClient>();
  EXPECT_TRUE(metrics->GetMetricsService());
}

}  // namespace cobalt
