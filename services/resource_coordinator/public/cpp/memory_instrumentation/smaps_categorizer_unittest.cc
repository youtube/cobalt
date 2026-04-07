// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/public/cpp/memory_instrumentation/smaps_categorizer.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace memory_instrumentation {

class TestDelegate : public SmapsCategorizer::Delegate {
 public:
  mojom::CobaltMemoryCategory GetCategory(
      const std::string& mapped_file) const override {
    if (mapped_file.find("libchrobalt.so") != std::string::npos) {
      return mojom::CobaltMemoryCategory::kLibChrobalt;
    }
    if (mapped_file.find("[stack]") != std::string::npos) {
      return mojom::CobaltMemoryCategory::kStacks;
    }
    if (mapped_file.find("partition_alloc") != std::string::npos) {
      return mojom::CobaltMemoryCategory::kPartitionAlloc;
    }
    return mojom::CobaltMemoryCategory::kUnknown;
  }
};

TEST(SmapsCategorizerTest, ParseSmapsContent) {
  std::string content =
      "7fb8322000-7fb8323000 r--p 00000000 00:00 0 [stack]\n"
      "Size:                  4 kB\n"
      "Pss:                   4 kB\n"
      "7fb8323000-7fb8324000 r-xp 00000000 00:00 0 libchrobalt.so\n"
      "Size:                  8 kB\n"
      "Pss:                   8 kB\n"
      "7fb8324000-7fb8325000 rw-p 00000000 00:00 0 \n"
      "Size:                  4 kB\n"
      "Pss:                   2 kB\n";

  TestDelegate delegate;
  mojom::DetailedMemoryStats stats =
      SmapsCategorizer::ParseSmapsContent(content, delegate);

  EXPECT_EQ(stats.categories_kb[mojom::CobaltMemoryCategory::kStacks], 4u);
  EXPECT_EQ(stats.categories_kb[mojom::CobaltMemoryCategory::kLibChrobalt], 8u);
  EXPECT_EQ(stats.categories_kb[mojom::CobaltMemoryCategory::kUnknown], 2u);
}

TEST(SmapsCategorizerTest, ParseSmapsMetricContent) {
  std::string content =
      "00400000-7ffc9a759000 [rollup]\n"
      "Rss:                 100 kB\n"
      "Pss:                  80 kB\n"
      "Pss_Dirty:            40 kB\n";

  EXPECT_EQ(SmapsCategorizer::ParseSmapsMetricContent(content, "Rss:"), 100u);
  EXPECT_EQ(SmapsCategorizer::ParseSmapsMetricContent(content, "Pss:"), 80u);
  EXPECT_EQ(SmapsCategorizer::ParseSmapsMetricContent(content, "Pss_Dirty:"),
            40u);
  EXPECT_EQ(SmapsCategorizer::ParseSmapsMetricContent(content, "NonExistent:"),
            0u);
}

TEST(SmapsCategorizerTest, MalformedInput) {
  TestDelegate delegate;

  // Empty content
  EXPECT_TRUE(
      SmapsCategorizer::ParseSmapsContent("", delegate).categories_kb.empty());

  // No headers, just stats
  std::string content = "Pss: 100 kB\n";
  auto stats = SmapsCategorizer::ParseSmapsContent(content, delegate);
  EXPECT_EQ(stats.categories_kb[mojom::CobaltMemoryCategory::kUnknown], 100u);

  // Invalid Pss value
  content =
      "7fb8322000-7fb8323000 r--p 00000000 00:00 0 [stack]\nPss: abc kB\n";
  stats = SmapsCategorizer::ParseSmapsContent(content, delegate);
  EXPECT_EQ(stats.categories_kb[mojom::CobaltMemoryCategory::kStacks], 0u);

  // Missing 'kB' suffix
  content = "7fb8322000-7fb8323000 r--p 00000000 00:00 0 [stack]\nPss: 123\n";
  stats = SmapsCategorizer::ParseSmapsContent(content, delegate);
  EXPECT_EQ(stats.categories_kb[mojom::CobaltMemoryCategory::kStacks], 123u);
}

TEST(SmapsCategorizerTest, MalformedHeaders) {
  TestDelegate delegate;

  // Header with missing columns (less than 5)
  std::string content =
      "7fb8322000-7fb8323000 r--p\n"
      "Pss: 10 kB\n";
  auto stats = SmapsCategorizer::ParseSmapsContent(content, delegate);
  // Should NOT be recognized as a header, but Pss should be attributed to
  // the current category (Unknown at start).
  EXPECT_EQ(stats.categories_kb[mojom::CobaltMemoryCategory::kUnknown], 10u);

  // Header without hyphen in address range
  content =
      "7fb8322000 r--p 00000000 00:00 0 [stack]\n"
      "Pss: 20 kB\n";
  stats = SmapsCategorizer::ParseSmapsContent(content, delegate);
  EXPECT_EQ(stats.categories_kb[mojom::CobaltMemoryCategory::kUnknown], 20u);

  // Header with only 5 columns (no pathname)
  content =
      "7fb8322000-7fb8323000 r--p 00000000 00:00 0\n"
      "Pss: 30 kB\n";
  stats = SmapsCategorizer::ParseSmapsContent(content, delegate);
  EXPECT_EQ(stats.categories_kb[mojom::CobaltMemoryCategory::kUnknown], 30u);
}

TEST(SmapsCategorizerTest, MultipleMappingsSameCategory) {
  std::string content =
      "7fb8322000-7fb8323000 r--p 00000000 00:00 0 partition_alloc\n"
      "Pss:                   10 kB\n"
      "7fb8323000-7fb8324000 rw-p 00000000 00:00 0 partition_alloc\n"
      "Pss:                   20 kB\n";

  TestDelegate delegate;
  mojom::DetailedMemoryStats stats =
      SmapsCategorizer::ParseSmapsContent(content, delegate);

  EXPECT_EQ(stats.categories_kb[mojom::CobaltMemoryCategory::kPartitionAlloc],
            30u);
}

}  // namespace memory_instrumentation
