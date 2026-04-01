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

#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {

class CobaltDetailedMetricsDelegateTest : public testing::Test {
 protected:
  CobaltDetailedMetricsDelegate delegate_;
};

TEST_F(CobaltDetailedMetricsDelegateTest, CategorizesCobaltCore) {
  delegate_.OnSmapsLine(
      "00400000-00421000 r-xp 00000000 fc:01 1234  /path/libcobalt.so");
  delegate_.OnSmapsLine("Pss:                  100 kB");
  delegate_.OnSmapsLine("Rss:                  200 kB");

  delegate_.OnSmapsLine(
      "00500000-00521000 r-xp 00000000 fc:01 1235  CobaltCore");
  delegate_.OnSmapsLine("Pss:                   50 kB");
  delegate_.OnSmapsLine("Rss:                   60 kB");

  auto stats = delegate_.GetAndResetStats();
  EXPECT_EQ(stats["pss:cobalt_core"], 150u);
  EXPECT_EQ(stats["rss:cobalt_core"], 260u);
}

TEST_F(CobaltDetailedMetricsDelegateTest, CategorizesV8) {
  delegate_.OnSmapsLine("00580000-005a1000 rw-p 00000000 00:00 0 [anon:v8]");
  delegate_.OnSmapsLine("Pss:                   100 kB");
  delegate_.OnSmapsLine("Rss:                   200 kB");

  auto stats = delegate_.GetAndResetStats();
  EXPECT_EQ(stats["pss:v8"], 100u);
  EXPECT_EQ(stats["rss:v8"], 200u);
}

TEST_F(CobaltDetailedMetricsDelegateTest, CategorizesPartitionAlloc) {
  delegate_.OnSmapsLine(
      "005b0000-005d1000 rw-p 00000000 00:00 0 [anon:partition_alloc]");
  delegate_.OnSmapsLine("Pss:                   150 kB");
  delegate_.OnSmapsLine("Rss:                   300 kB");

  auto stats = delegate_.GetAndResetStats();
  EXPECT_EQ(stats["pss:partition_alloc"], 150u);
  EXPECT_EQ(stats["rss:partition_alloc"], 300u);
}

TEST_F(CobaltDetailedMetricsDelegateTest, CategorizesMalloc) {
  delegate_.OnSmapsLine("00600000-00621000 rw-p 00000000 00:00 0 [anon:scudo]");
  delegate_.OnSmapsLine("Pss:                   10 kB");
  delegate_.OnSmapsLine("Rss:                   20 kB");

  delegate_.OnSmapsLine("00700000-00721000 rw-p 00000000 00:00 0 [heap]");
  delegate_.OnSmapsLine("Pss:                   5 kB");
  delegate_.OnSmapsLine("Rss:                   10 kB");

  auto stats = delegate_.GetAndResetStats();
  EXPECT_EQ(stats["pss:malloc"], 15u);
  EXPECT_EQ(stats["rss:malloc"], 30u);
}

TEST_F(CobaltDetailedMetricsDelegateTest, CategorizesAndroidRuntime) {
  delegate_.OnSmapsLine(
      "00800000-00821000 r--p 00000000 fc:01 1236  /path/boot.art");
  delegate_.OnSmapsLine("Pss:                   20 kB");
  delegate_.OnSmapsLine("Rss:                   40 kB");

  delegate_.OnSmapsLine(
      "00900000-00921000 r-xp 00000000 fc:01 1237  /path/base.odex");
  delegate_.OnSmapsLine("Pss:                   30 kB");
  delegate_.OnSmapsLine("Rss:                   50 kB");

  auto stats = delegate_.GetAndResetStats();
  EXPECT_EQ(stats["pss:android_runtime"], 50u);
  EXPECT_EQ(stats["rss:android_runtime"], 90u);
}

TEST_F(CobaltDetailedMetricsDelegateTest, CategorizesGraphics) {
  delegate_.OnSmapsLine(
      "00a00000-00a21000 rw-p 00000000 00:00 0 /dev/kgsl-3d0");
  delegate_.OnSmapsLine("Pss:                   40 kB");
  delegate_.OnSmapsLine("Rss:                   80 kB");

  auto stats = delegate_.GetAndResetStats();
  EXPECT_EQ(stats["pss:graphics"], 40u);
  EXPECT_EQ(stats["rss:graphics"], 80u);
}

TEST_F(CobaltDetailedMetricsDelegateTest, HandlesBrittleFormats) {
  // Test irregular spacing
  delegate_.OnSmapsLine("00b00000-00b21000 rw-p 00000000 00:00 0 [stack]");
  delegate_.OnSmapsLine("Pss: 12 kB");
  delegate_.OnSmapsLine("Rss:   24    kB");

  auto stats = delegate_.GetAndResetStats();
  EXPECT_EQ(stats["pss:stacks"], 12u);
  EXPECT_EQ(stats["rss:stacks"], 24u);
}

TEST_F(CobaltDetailedMetricsDelegateTest, RespectsPrecedence) {
  // libcobalt.so should be cobalt_core even if it has .so extension
  // (code_other)
  delegate_.OnSmapsLine(
      "00c00000-00c21000 r-xp 00000000 fc:01 1238  /path/libcobalt.so");
  delegate_.OnSmapsLine("Pss:                   10 kB");

  // some other .so should be code_other
  delegate_.OnSmapsLine(
      "00d00000-00d21000 r-xp 00000000 fc:01 1239  /path/libother.so");
  delegate_.OnSmapsLine("Pss:                   20 kB");

  auto stats = delegate_.GetAndResetStats();
  EXPECT_EQ(stats["pss:cobalt_core"], 10u);
  EXPECT_EQ(stats["pss:code_other"], 20u);
}

TEST_F(CobaltDetailedMetricsDelegateTest, HandlesUnknownAndOther) {
  // Anonymous mapping not starting with scudo or dalvik
  delegate_.OnSmapsLine(
      "00e00000-00e21000 rw-p 00000000 00:00 0 [anon:something]");
  delegate_.OnSmapsLine("Pss:                   5 kB");

  // Something completely different
  delegate_.OnSmapsLine("00f00000-00f21000 rw-p 00000000 00:00 0 /some/file");
  delegate_.OnSmapsLine("Pss:                   7 kB");

  auto stats = delegate_.GetAndResetStats();
  EXPECT_EQ(stats["pss:anonymous_other"], 5u);
  EXPECT_EQ(stats["pss:other"], 7u);
}

TEST_F(CobaltDetailedMetricsDelegateTest, ResetsCorrectly) {
  delegate_.OnSmapsLine(
      "01000000-01021000 r-xp 00000000 fc:01 1240  CobaltCore");
  delegate_.OnSmapsLine("Pss:                   10 kB");

  delegate_.GetAndResetStats();

  auto stats = delegate_.GetAndResetStats();
  EXPECT_EQ(stats["pss:cobalt_core"], 0u);
}

}  // namespace cobalt
