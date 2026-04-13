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
  delegate_.OnSmapsBuffer(
      "00400000-00421000 r-xp 00000000 fc:01 1234  /path/libcobalt.so");
  delegate_.OnSmapsBuffer("Pss:                  100 kB");
  delegate_.OnSmapsBuffer("Rss:                  200 kB");

  delegate_.OnSmapsBuffer(
      "00430000-00450000 r-xp 00000000 fc:01 1236  /path/libchrobalt.so");
  delegate_.OnSmapsBuffer("Pss:                   30 kB");
  delegate_.OnSmapsBuffer("Rss:                   40 kB");

  delegate_.OnSmapsBuffer(
      "00500000-00521000 r-xp 00000000 fc:01 1235  CobaltCore");
  delegate_.OnSmapsBuffer("Pss:                   50 kB");
  delegate_.OnSmapsBuffer("Rss:                   60 kB");

  auto stats = delegate_.GetAndResetStats();
  EXPECT_EQ(stats.categories_kb["pss:cobalt_core"], 150u);
  EXPECT_EQ(stats.categories_kb["rss:cobalt_core"], 260u);
  EXPECT_EQ(stats.categories_kb["pss:lib_chrobalt"], 30u);
  EXPECT_EQ(stats.categories_kb["rss:lib_chrobalt"], 40u);
}

TEST_F(CobaltDetailedMetricsDelegateTest, CategorizesV8) {
  delegate_.OnSmapsBuffer("00580000-005a1000 rw-p 00000000 00:00 0 [anon:v8]");
  delegate_.OnSmapsBuffer("Pss:                   100 kB");
  delegate_.OnSmapsBuffer("Rss:                   200 kB");

  auto stats = delegate_.GetAndResetStats();
  EXPECT_EQ(stats.categories_kb["pss:v8"], 100u);
  EXPECT_EQ(stats.categories_kb["rss:v8"], 200u);
}

TEST_F(CobaltDetailedMetricsDelegateTest, CategorizesPartitionAlloc) {
  delegate_.OnSmapsBuffer(
      "005b0000-005d1000 rw-p 00000000 00:00 0 [anon:partition_alloc]");
  delegate_.OnSmapsBuffer("Pss:                   150 kB");
  delegate_.OnSmapsBuffer("Rss:                   300 kB");

  auto stats = delegate_.GetAndResetStats();
  EXPECT_EQ(stats.categories_kb["pss:partition_alloc"], 150u);
  EXPECT_EQ(stats.categories_kb["rss:partition_alloc"], 300u);
}

TEST_F(CobaltDetailedMetricsDelegateTest, CategorizesMalloc) {
  delegate_.OnSmapsBuffer(
      "00600000-00621000 rw-p 00000000 00:00 0 [anon:scudo]");
  delegate_.OnSmapsBuffer("Pss:                   10 kB");
  delegate_.OnSmapsBuffer("Rss:                   20 kB");

  delegate_.OnSmapsBuffer("00700000-00721000 rw-p 00000000 00:00 0 [heap]");
  delegate_.OnSmapsBuffer("Pss:                   5 kB");
  delegate_.OnSmapsBuffer("Rss:                   10 kB");

  auto stats = delegate_.GetAndResetStats();
  EXPECT_EQ(stats.categories_kb["pss:malloc"], 15u);
  EXPECT_EQ(stats.categories_kb["rss:malloc"], 30u);
}

TEST_F(CobaltDetailedMetricsDelegateTest, CategorizesAndroidRuntime) {
  delegate_.OnSmapsBuffer(
      "00800000-00821000 r--p 00000000 fc:01 1236  /path/boot.art");
  delegate_.OnSmapsBuffer("Pss:                   20 kB");
  delegate_.OnSmapsBuffer("Rss:                   40 kB");

  delegate_.OnSmapsBuffer(
      "00900000-00921000 r-xp 00000000 fc:01 1237  /path/base.odex");
  delegate_.OnSmapsBuffer("Pss:                   30 kB");
  delegate_.OnSmapsBuffer("Rss:                   50 kB");

  auto stats = delegate_.GetAndResetStats();
  EXPECT_EQ(stats.categories_kb["pss:android_runtime"], 50u);
  EXPECT_EQ(stats.categories_kb["rss:android_runtime"], 90u);
}

TEST_F(CobaltDetailedMetricsDelegateTest, CategorizesGraphics) {
  delegate_.OnSmapsBuffer(
      "00a00000-00a21000 rw-p 00000000 00:00 0 /dev/kgsl-3d0");
  delegate_.OnSmapsBuffer("Pss:                   40 kB");
  delegate_.OnSmapsBuffer("Rss:                   80 kB");

  auto stats = delegate_.GetAndResetStats();
  EXPECT_EQ(stats.categories_kb["pss:graphics"], 40u);
  EXPECT_EQ(stats.categories_kb["rss:graphics"], 80u);
}

TEST_F(CobaltDetailedMetricsDelegateTest, HandlesBrittleFormats) {
  // Test irregular spacing
  delegate_.OnSmapsBuffer("00b00000-00b21000 rw-p 00000000 00:00 0 [stack]");
  delegate_.OnSmapsBuffer("Pss: 12 kB");
  delegate_.OnSmapsBuffer("Rss:   24    kB");

  auto stats = delegate_.GetAndResetStats();
  EXPECT_EQ(stats.categories_kb["pss:stacks"], 12u);
  EXPECT_EQ(stats.categories_kb["rss:stacks"], 24u);
}

TEST_F(CobaltDetailedMetricsDelegateTest, RespectsPrecedence) {
  // libcobalt.so should be cobalt_core even if it has .so extension
  // (code_other)
  delegate_.OnSmapsBuffer(
      "00c00000-00c21000 r-xp 00000000 fc:01 1238  /path/libcobalt.so");
  delegate_.OnSmapsBuffer("Pss:                   10 kB");

  // some other .so should be code_other
  delegate_.OnSmapsBuffer(
      "00d00000-00d21000 r-xp 00000000 fc:01 1239  /path/libother.so");
  delegate_.OnSmapsBuffer("Pss:                   20 kB");

  auto stats = delegate_.GetAndResetStats();
  EXPECT_EQ(stats.categories_kb["pss:cobalt_core"], 10u);
  EXPECT_EQ(stats.categories_kb["pss:code_other"], 20u);
}

TEST_F(CobaltDetailedMetricsDelegateTest, HandlesUnknownAndOther) {
  // Anonymous mapping not starting with scudo or dalvik
  delegate_.OnSmapsBuffer(
      "00e00000-00e21000 rw-p 00000000 00:00 0 [anon:something]");
  delegate_.OnSmapsBuffer("Pss:                   5 kB");

  // Something completely different
  delegate_.OnSmapsBuffer("00f00000-00f21000 rw-p 00000000 00:00 0 /some/file");
  delegate_.OnSmapsBuffer("Pss:                   7 kB");

  auto stats = delegate_.GetAndResetStats();
  EXPECT_EQ(stats.categories_kb["pss:anonymous_other"], 5u);
  EXPECT_EQ(stats.categories_kb["pss:other"], 7u);
}

TEST_F(CobaltDetailedMetricsDelegateTest, ResetsCorrectly) {
  delegate_.OnSmapsBuffer(
      "01000000-01021000 r-xp 00000000 fc:01 1240  CobaltCore");
  delegate_.OnSmapsBuffer("Pss:                   10 kB");

  delegate_.GetAndResetStats();

  auto stats = delegate_.GetAndResetStats();
  EXPECT_EQ(stats.categories_kb["pss:cobalt_core"], 0u);
}

}  // namespace cobalt
