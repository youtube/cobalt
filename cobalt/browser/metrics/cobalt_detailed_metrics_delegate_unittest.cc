// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/browser/metrics/cobalt_detailed_metrics_delegate.h"

#include <limits>

#include "base/containers/flat_map.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {

class CobaltDetailedMetricsDelegateTest : public testing::Test {
 protected:
  CobaltDetailedMetricsDelegate delegate_;
};

TEST_F(CobaltDetailedMetricsDelegateTest, CategorizesCobaltCore) {
  memory_instrumentation::SmapsMetrics m1;
  m1.pss_kb = 100;
  m1.rss_kb = 200;
  delegate_.OnSmapsEntry("/path/libcobalt.so", m1);

  memory_instrumentation::SmapsMetrics m2;
  m2.pss_kb = 30;
  m2.rss_kb = 40;
  delegate_.OnSmapsEntry("/path/libchrobalt.so", m2);

  memory_instrumentation::SmapsMetrics m3;
  m3.pss_kb = 50;
  m3.rss_kb = 60;
  delegate_.OnSmapsEntry("libcobalt.so", m3);

  base::flat_map<std::string, uint64_t> stats;
  delegate_.GetAndResetStats(&stats);
  EXPECT_EQ(stats["pss:cobalt_core"], 150u);
  EXPECT_EQ(stats["rss:cobalt_core"], 260u);
  EXPECT_EQ(stats["pss:lib_chrobalt"], 30u);
  EXPECT_EQ(stats["rss:lib_chrobalt"], 40u);
}

TEST_F(CobaltDetailedMetricsDelegateTest, CategorizesV8) {
  memory_instrumentation::SmapsMetrics m;
  m.pss_kb = 100;
  m.rss_kb = 200;
  delegate_.OnSmapsEntry("[anon:v8]", m);

  base::flat_map<std::string, uint64_t> stats;
  delegate_.GetAndResetStats(&stats);
  EXPECT_EQ(stats["pss:v8"], 100u);
  EXPECT_EQ(stats["rss:v8"], 200u);
}

TEST_F(CobaltDetailedMetricsDelegateTest, CategorizesPartitionAlloc) {
  memory_instrumentation::SmapsMetrics m;
  m.pss_kb = 150;
  m.rss_kb = 300;
  delegate_.OnSmapsEntry("[anon:partition_alloc]", m);

  base::flat_map<std::string, uint64_t> stats;
  delegate_.GetAndResetStats(&stats);
  EXPECT_EQ(stats["pss:partition_alloc"], 150u);
  EXPECT_EQ(stats["rss:partition_alloc"], 300u);
}

TEST_F(CobaltDetailedMetricsDelegateTest, CategorizesGraphics) {
  memory_instrumentation::SmapsMetrics m;
  m.pss_kb = 40;
  m.rss_kb = 80;
  delegate_.OnSmapsEntry("/dev/kgsl-3d0", m);

  base::flat_map<std::string, uint64_t> stats;
  delegate_.GetAndResetStats(&stats);
  EXPECT_EQ(stats["pss:graphics"], 40u);
  EXPECT_EQ(stats["rss:graphics"], 80u);
}

TEST_F(CobaltDetailedMetricsDelegateTest, HandlesAnonymousOther) {
  memory_instrumentation::SmapsMetrics m;
  m.pss_kb = 5;
  m.rss_kb = 10;
  delegate_.OnSmapsEntry("", m);

  base::flat_map<std::string, uint64_t> stats;
  delegate_.GetAndResetStats(&stats);
  EXPECT_EQ(stats["pss:anonymous_other"], 5u);
}

TEST_F(CobaltDetailedMetricsDelegateTest, HandlesOther) {
  memory_instrumentation::SmapsMetrics m;
  m.pss_kb = 7;
  m.rss_kb = 14;
  delegate_.OnSmapsEntry("/some/random/file", m);

  base::flat_map<std::string, uint64_t> stats;
  delegate_.GetAndResetStats(&stats);
  EXPECT_EQ(stats["pss:other"], 7u);
}

TEST_F(CobaltDetailedMetricsDelegateTest, ResetsCorrectly) {
  memory_instrumentation::SmapsMetrics m;
  m.pss_kb = 10;
  delegate_.OnSmapsEntry("libcobalt.so", m);

  base::flat_map<std::string, uint64_t> stats;
  delegate_.GetAndResetStats(&stats);
  EXPECT_EQ(stats["pss:cobalt_core"], 10u);

  delegate_.GetAndResetStats(&stats);
  EXPECT_TRUE(stats.empty());
}

}  // namespace cobalt
