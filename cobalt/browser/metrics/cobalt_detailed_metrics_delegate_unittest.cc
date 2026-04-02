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
  delegate_.OnSmapsHeader(
      "00400000-00421000 r-xp 00000000 fc:01 1234  /path/libcobalt.so");
  delegate_.OnSmapsCounter("Pss", 100);
  delegate_.OnSmapsCounter("Rss", 200);

  delegate_.OnSmapsHeader(
      "00500000-00521000 r-xp 00000000 fc:01 1235  CobaltCore");
  delegate_.OnSmapsCounter("Pss", 50);
  delegate_.OnSmapsCounter("Rss", 60);

  auto stats = delegate_.GetAndResetStats();
  EXPECT_EQ(stats["pss:cobalt_core"], 150u);
  EXPECT_EQ(stats["rss:cobalt_core"], 260u);
}

TEST_F(CobaltDetailedMetricsDelegateTest, CategorizesV8) {
  delegate_.OnSmapsHeader("00580000-005a1000 rw-p 00000000 00:00 0 [anon:v8]");
  delegate_.OnSmapsCounter("Pss", 100);
  delegate_.OnSmapsCounter("Rss", 200);

  auto stats = delegate_.GetAndResetStats();
  EXPECT_EQ(stats["pss:v8"], 100u);
  EXPECT_EQ(stats["rss:v8"], 200u);
}

TEST_F(CobaltDetailedMetricsDelegateTest, CategorizesPartitionAlloc) {
  delegate_.OnSmapsHeader(
      "005b0000-005d1000 rw-p 00000000 00:00 0 [anon:partition_alloc]");
  delegate_.OnSmapsCounter("Pss", 150);
  delegate_.OnSmapsCounter("Rss", 300);

  auto stats = delegate_.GetAndResetStats();
  EXPECT_EQ(stats["pss:partition_alloc"], 150u);
  EXPECT_EQ(stats["rss:partition_alloc"], 300u);
}

TEST_F(CobaltDetailedMetricsDelegateTest, CategorizesMalloc) {
  delegate_.OnSmapsHeader(
      "00600000-00621000 rw-p 00000000 00:00 0 [anon:scudo]");
  delegate_.OnSmapsCounter("Pss", 10);
  delegate_.OnSmapsCounter("Rss", 20);

  delegate_.OnSmapsHeader("00700000-00721000 rw-p 00000000 00:00 0 [heap]");
  delegate_.OnSmapsCounter("Pss", 5);
  delegate_.OnSmapsCounter("Rss", 10);

  auto stats = delegate_.GetAndResetStats();
  EXPECT_EQ(stats["pss:malloc"], 15u);
  EXPECT_EQ(stats["rss:malloc"], 30u);
}

TEST_F(CobaltDetailedMetricsDelegateTest, CategorizesAndroidRuntime) {
  delegate_.OnSmapsHeader(
      "00800000-00821000 r--p 00000000 fc:01 1236  /path/boot.art");
  delegate_.OnSmapsCounter("Pss", 20);
  delegate_.OnSmapsCounter("Rss", 40);

  delegate_.OnSmapsHeader(
      "00900000-00921000 r-xp 00000000 fc:01 1237  /path/base.odex");
  delegate_.OnSmapsCounter("Pss", 30);
  delegate_.OnSmapsCounter("Rss", 50);

  auto stats = delegate_.GetAndResetStats();
  EXPECT_EQ(stats["pss:android_runtime"], 50u);
  EXPECT_EQ(stats["rss:android_runtime"], 90u);
}

TEST_F(CobaltDetailedMetricsDelegateTest, CategorizesGraphics) {
  delegate_.OnSmapsHeader(
      "00a00000-00a21000 rw-p 00000000 00:00 0 /dev/kgsl-3d0");
  delegate_.OnSmapsCounter("Pss", 40);
  delegate_.OnSmapsCounter("Rss", 80);

  auto stats = delegate_.GetAndResetStats();
  EXPECT_EQ(stats["pss:graphics"], 40u);
  EXPECT_EQ(stats["rss:graphics"], 80u);
}

TEST_F(CobaltDetailedMetricsDelegateTest, HandlesBrittleFormats) {
  // Test irregular spacing (no longer an issue as categorizer handles it)
  delegate_.OnSmapsHeader("00b00000-00b21000 rw-p 00000000 00:00 0 [stack]");
  delegate_.OnSmapsCounter("Pss", 12);
  delegate_.OnSmapsCounter("Rss", 24);

  auto stats = delegate_.GetAndResetStats();
  EXPECT_EQ(stats["pss:stacks"], 12u);
  EXPECT_EQ(stats["rss:stacks"], 24u);
}

TEST_F(CobaltDetailedMetricsDelegateTest, RespectsPrecedence) {
  // libcobalt.so should be cobalt_core even if it has .so extension
  // (code_other)
  delegate_.OnSmapsHeader(
      "00c00000-00c21000 r-xp 00000000 fc:01 1238  /path/libcobalt.so");
  delegate_.OnSmapsCounter("Pss", 10);

  // some other .so should be code_other
  delegate_.OnSmapsHeader(
      "00d00000-00d21000 r-xp 00000000 fc:01 1239  /path/libother.so");
  delegate_.OnSmapsCounter("Pss", 20);

  auto stats = delegate_.GetAndResetStats();
  EXPECT_EQ(stats["pss:cobalt_core"], 10u);
  EXPECT_EQ(stats["pss:code_other"], 20u);
}

TEST_F(CobaltDetailedMetricsDelegateTest, HandlesUnknownAndOther) {
  // Anonymous mapping not starting with scudo or dalvik
  delegate_.OnSmapsHeader(
      "00e00000-00e21000 rw-p 00000000 00:00 0 [anon:something]");
  delegate_.OnSmapsCounter("Pss", 5);

  // Something completely different
  delegate_.OnSmapsHeader("00f00000-00f21000 rw-p 00000000 00:00 0 /some/file");
  delegate_.OnSmapsCounter("Pss", 7);

  auto stats = delegate_.GetAndResetStats();
  EXPECT_EQ(stats["pss:anonymous_other"], 5u);
  EXPECT_EQ(stats["pss:other"], 7u);
}

TEST_F(CobaltDetailedMetricsDelegateTest, ResetsCorrectly) {
  delegate_.OnSmapsHeader(
      "01000000-01021000 r-xp 00000000 fc:01 1240  CobaltCore");
  delegate_.OnSmapsCounter("Pss", 10);

  delegate_.GetAndResetStats();

  auto stats = delegate_.GetAndResetStats();
  EXPECT_EQ(stats["pss:cobalt_core"], 0u);
}

}  // namespace cobalt
