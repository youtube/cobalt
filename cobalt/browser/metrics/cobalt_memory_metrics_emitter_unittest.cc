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

#include "cobalt/browser/metrics/cobalt_memory_metrics_emitter.h"

#include <array>
#include <cstddef>
#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "testing/gtest/include/gtest/gtest.h"

// The parser and everything it feeds are compiled out where VA space telemetry
// is not collected, so there is nothing to test on those platforms.
#if BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_32_BITS)

namespace cobalt {
namespace {

using Emitter = CobaltMemoryMetricsEmitter;

// Builds a /proc/self/maps line whose path is long enough to overflow the
// parser's internal line buffer, such that the split lands immediately before
// `tail`, which is then parsed as if it began a fresh line.
//
// NOTE: kParserLineBudget is deliberately tied to kMaxLineLength - 1 in
// CalculateVirtualAddressSpaceMetricsInternal(). If that buffer size changes,
// update this.
constexpr size_t kParserLineBudget = 1023;

std::string LineTruncatedBefore(const std::string& tail) {
  const std::string head = "00400000-00450000 r-xp 00000000 08:02 173521 /bin/";
  return head + std::string(kParserLineBudget - head.size(), 'x') + tail + "\n";
}

// A well-formed VMA that follows the 0x00400000-0x00450000 mapping used by the
// truncation cases, leaving a 16 MB gap after it.
constexpr char kNextVmaAfter16MbGap[] =
    "01450000-01550000 rw-p 00000000 00:00 0      [anon:heap]\n";

TEST(CobaltVirtualAddressSpaceMetricsTest, ComputesGapsAndRatio) {
  // Three VMAs with known gaps:
  //   VMA 1: 0x00400000-0x00450000
  //   [gap 1: 0x00450000 -> 0x01450000 = 16 MB]
  //   VMA 2: 0x01450000-0x01550000
  //   [gap 2: 0x01550000 -> 0x05550000 = 64 MB]
  //   VMA 3: 0x05550000-0x05650000
  // Total unmapped = 80 MB, largest gap = 64 MB.
  // Fragmentation = 1.0 - (64 / 80) = 20%.
  auto metrics = Emitter::CalculateVirtualAddressSpaceMetricsForTesting(
      "00400000-00450000 r-xp 00000000 08:02 173521 /bin/app\n"
      "01450000-01550000 rw-p 00000000 00:00 0      [anon:heap]\n"
      "05550000-05650000 rw-p 00000000 00:00 0      [stack]\n");

  ASSERT_TRUE(metrics.has_value());
  EXPECT_EQ(3u, metrics->vma_count);
  EXPECT_EQ(64u, metrics->largest_free_gap_mb);
  EXPECT_EQ(80u, metrics->total_unmapped_va_mb);
  EXPECT_EQ(20, metrics->fragmentation_ratio_pct);
}

TEST(CobaltVirtualAddressSpaceMetricsTest, ReportsContiguousSpaceAsExhausted) {
  // Back-to-back mappings leave no unmapped space at all. There is nothing left
  // to allocate from, which is reported at the top of the scale rather than 0.
  auto metrics = Emitter::CalculateVirtualAddressSpaceMetricsForTesting(
      "00400000-00450000 r-xp 00000000 08:02 173521 /bin/app\n"
      "00450000-00550000 rw-p 00000000 00:00 0      [anon:heap]\n");

  ASSERT_TRUE(metrics.has_value());
  EXPECT_EQ(2u, metrics->vma_count);
  EXPECT_EQ(0u, metrics->largest_free_gap_mb);
  EXPECT_EQ(0u, metrics->total_unmapped_va_mb);
  EXPECT_EQ(100, metrics->fragmentation_ratio_pct);
}

TEST(CobaltVirtualAddressSpaceMetricsTest, ReturnsNulloptWithoutUsableInput) {
  // Nothing parsable means we do not know what the address space looks like.
  // That must not be reported as an all-zero (and therefore alarming) sample.
  const auto kTestCases = std::to_array<const char*>({
      "",
      "\n",
      "not a maps file\nneither is this\n",
      // Hex digits, but never a "<hex>-<hex>" range.
      "deadbeef rw-p 00000000 00:00 0\n",
  });

  for (size_t i = 0; i < std::size(kTestCases); ++i) {
    SCOPED_TRACE(
        base::StringPrintf("kTestCases[%zu] = \"%s\"", i, kTestCases[i]));
    EXPECT_FALSE(
        Emitter::CalculateVirtualAddressSpaceMetricsForTesting(kTestCases[i])
            .has_value());
  }
}

// The kernel emits VMAs in strictly ascending, non-overlapping order, so any
// record violating that is garbage -- most often the tail of a truncated long
// path that happens to look like a range.
//
// Every case below wraps the same real mapping pair (0x00400000-0x00450000
// then 0x01450000-0x01550000, a 16 MB gap) around a bogus record, so a
// regression shows up as a gap larger than 16 MB.
TEST(CobaltVirtualAddressSpaceMetricsTest, IgnoresBogusRecords) {
  struct TestCase {
    const char* name;
    std::string maps;
  };

  const TestCase kTestCases[] = {
      {"backwards range",
       "00400000-00450000 r-xp 00000000 08:02 173521 /bin/app\n"
       "0000001a-0000002b rw-p 00000000 00:00 0      [bogus]\n" +
           std::string(kNextVmaAfter16MbGap)},
      // Deliberately sits *above* the previous mapping's end so that it clears
      // the ordering check; only the end-before-start check can reject it.
      {"end before start, in ascending order",
       "00400000-00450000 r-xp 00000000 08:02 173521 /bin/app\n"
       "00500000-00460000 rw-p 00000000 00:00 0      [bogus]\n" +
           std::string(kNextVmaAfter16MbGap)},
      // sscanf's %x accepts a leading sign, so a hyphenated fragment of a
      // truncated path can convert to a huge wrapped value.
      {"signed range from truncated path",
       LineTruncatedBefore("-2b-3c") + std::string(kNextVmaAfter16MbGap)},
      {"hex-like tail from truncated path",
       LineTruncatedBefore("1a-2b") + std::string(kNextVmaAfter16MbGap)},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.name);

    auto metrics =
        Emitter::CalculateVirtualAddressSpaceMetricsForTesting(test_case.maps);
    ASSERT_TRUE(metrics.has_value());
    // Only the two well-ordered VMAs count.
    EXPECT_EQ(2u, metrics->vma_count);
    // Measured from 0x00450000, not from the bogus record's end.
    EXPECT_EQ(16u, metrics->largest_free_gap_mb);
    EXPECT_EQ(16u, metrics->total_unmapped_va_mb);
  }
}

// A long path that does not produce a range-shaped tail should simply be
// ignored past the address field, leaving the metrics untouched.
TEST(CobaltVirtualAddressSpaceMetricsTest, ToleratesLongPathsWithoutHexTails) {
  auto metrics = Emitter::CalculateVirtualAddressSpaceMetricsForTesting(
      LineTruncatedBefore("ordinary/path/component") +
      std::string(kNextVmaAfter16MbGap));

  ASSERT_TRUE(metrics.has_value());
  EXPECT_EQ(2u, metrics->vma_count);
  EXPECT_EQ(16u, metrics->largest_free_gap_mb);
}

// The kernel's gate VMA sits above the user/kernel split, so the space beneath
// it is not allocatable. Counting it would inflate the free-space total, and on
// a 3GB/1GB split kernel that region is ~1GB and would become the largest gap.
TEST(CobaltVirtualAddressSpaceMetricsTest, IgnoresGateVma) {
  constexpr char kUserSpace[] =
      "00400000-00450000 r-xp 00000000 08:02 173521 /bin/app\n"
      "01450000-01550000 rw-p 00000000 00:00 0      [anon:heap]\n";
  constexpr char kGateVma[] =
      "ffff0000-ffff1000 r-xp 00000000 00:00 0      [vectors]\n";

  auto baseline =
      Emitter::CalculateVirtualAddressSpaceMetricsForTesting(kUserSpace);
  auto with_gate = Emitter::CalculateVirtualAddressSpaceMetricsForTesting(
      std::string(kUserSpace) + kGateVma);

  ASSERT_TRUE(baseline.has_value());
  ASSERT_TRUE(with_gate.has_value());
  EXPECT_EQ(baseline->vma_count, with_gate->vma_count);
  EXPECT_EQ(baseline->largest_free_gap_mb, with_gate->largest_free_gap_mb);
  EXPECT_EQ(baseline->total_unmapped_va_mb, with_gate->total_unmapped_va_mb);
}

// The gate VMA is emitted after seq_file finishes walking the VMA tree, so
// records past it cannot be trusted. This record is ordered above the last
// real mapping, so the ordering guard would accept it; only stopping at the
// gate keeps it out.
TEST(CobaltVirtualAddressSpaceMetricsTest, StopsAtGateVma) {
  auto metrics = Emitter::CalculateVirtualAddressSpaceMetricsForTesting(
      "00400000-00450000 r-xp 00000000 08:02 173521 /bin/app\n"
      "01450000-01550000 rw-p 00000000 00:00 0      [anon:heap]\n"
      "ffff0000-ffff1000 r-xp 00000000 00:00 0      [vectors]\n"
      "02550000-02650000 rw-p 00000000 00:00 0      [anon:replayed]\n");

  ASSERT_TRUE(metrics.has_value());
  EXPECT_EQ(2u, metrics->vma_count);
  EXPECT_EQ(16u, metrics->largest_free_gap_mb);
  EXPECT_EQ(16u, metrics->total_unmapped_va_mb);
}

// Sanity check against the real address space. The absolute numbers depend on
// the host, so only invariants are asserted.
TEST(CobaltVirtualAddressSpaceMetricsTest, ParsesRealProcSelfMaps) {
  std::string maps;
  ASSERT_TRUE(base::ReadFileToString(base::FilePath("/proc/self/maps"), &maps));

  auto metrics = Emitter::CalculateVirtualAddressSpaceMetricsForTesting(maps);
  ASSERT_TRUE(metrics.has_value());
  EXPECT_GT(metrics->vma_count, 0u);
  EXPECT_GE(metrics->total_unmapped_va_mb, metrics->largest_free_gap_mb);
  EXPECT_GE(metrics->fragmentation_ratio_pct, 0);
  EXPECT_LE(metrics->fragmentation_ratio_pct, 100);
}

}  // namespace
}  // namespace cobalt

#endif  // BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_32_BITS)
