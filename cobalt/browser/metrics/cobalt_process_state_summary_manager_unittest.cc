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

#include "cobalt/browser/metrics/cobalt_process_state_summary_manager.h"

#include <cstdint>
#include <optional>
#include <vector>

#include "base/hash/hash.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace {

TEST(CobaltProcessStateSummaryManagerTest, SnapshotSerializationRoundtrip) {
  ProcessStateSnapshot original;
  original.pid = 12345;
  original.peak_rss_kb = 256000;
  original.peak_pmf_kb = 128000;
  original.peak_v8_code_kb = 64000;
  original.uptime_sec = 3600;
  original.startup_milestones = (1ULL << 0) | (1ULL << 5) | (1ULL << 17);
  original.highest_milestone = 17;
  original.last_trim_level = 15;
  original.flags = kFlagForeground | kFlagMediaPlaying |
                   kFlagStartupGuardArmed | kFlagStartupGuardTriggeredKill;

  std::vector<uint8_t> payload =
      CobaltProcessStateSummaryManager::SerializeSnapshot(original);
  EXPECT_EQ(payload.size(), kProcessStateSummaryPayloadSize);

  std::optional<ProcessStateSnapshot> deserialized =
      CobaltProcessStateSummaryManager::DeserializeSnapshot(payload);
  ASSERT_TRUE(deserialized.has_value());
  EXPECT_EQ(*deserialized, original);
  EXPECT_TRUE(deserialized->is_startup_guard_armed());
  EXPECT_TRUE(deserialized->was_killed_by_startup_guard());
  EXPECT_EQ(deserialized->uptime_minutes(), 60u);
}

TEST(CobaltProcessStateSummaryManagerTest, RejectsTruncatedAndCorruptPayloads) {
  ProcessStateSnapshot valid;
  valid.pid = 54321;
  valid.peak_rss_kb = 102400;
  valid.peak_pmf_kb = 102400;
  valid.peak_v8_code_kb = 10240;
  valid.uptime_sec = 600;
  valid.startup_milestones = 1ULL << 5;
  valid.highest_milestone = 5;

  std::vector<uint8_t> valid_payload =
      CobaltProcessStateSummaryManager::SerializeSnapshot(valid);
  ASSERT_EQ(valid_payload.size(), kProcessStateSummaryPayloadSize);

  // Truncated payload.
  std::vector<uint8_t> truncated(valid_payload.begin(),
                                 valid_payload.end() - 1);
  EXPECT_FALSE(CobaltProcessStateSummaryManager::DeserializeSnapshot(truncated)
                   .has_value());

  // Oversized payload.
  std::vector<uint8_t> oversized = valid_payload;
  oversized.push_back(0);
  EXPECT_FALSE(CobaltProcessStateSummaryManager::DeserializeSnapshot(oversized)
                   .has_value());

  // Corrupted magic.
  std::vector<uint8_t> bad_magic = valid_payload;
  bad_magic[0] = 0xAA;
  EXPECT_FALSE(CobaltProcessStateSummaryManager::DeserializeSnapshot(bad_magic)
                   .has_value());

  // Corrupted version.
  std::vector<uint8_t> bad_version = valid_payload;
  bad_version[1] = 99;
  EXPECT_FALSE(
      CobaltProcessStateSummaryManager::DeserializeSnapshot(bad_version)
          .has_value());

  // Corrupted payload checksum mismatch.
  std::vector<uint8_t> bad_checksum = valid_payload;
  bad_checksum[8] ^= 0xFF;  // Corrupt RSS
  EXPECT_FALSE(
      CobaltProcessStateSummaryManager::DeserializeSnapshot(bad_checksum)
          .has_value());

  // Implausible memory (> 64GB).
  ProcessStateSnapshot bad_mem = valid;
  bad_mem.peak_rss_kb = 70 * 1024 * 1024;
  std::vector<uint8_t> bad_mem_payload =
      CobaltProcessStateSummaryManager::SerializeSnapshot(bad_mem);
  EXPECT_FALSE(
      CobaltProcessStateSummaryManager::DeserializeSnapshot(bad_mem_payload)
          .has_value());

  // Implausible uptime (> 180 days).
  ProcessStateSnapshot bad_uptime = valid;
  bad_uptime.uptime_sec = 200 * 86400;
  std::vector<uint8_t> bad_uptime_payload =
      CobaltProcessStateSummaryManager::SerializeSnapshot(bad_uptime);
  EXPECT_FALSE(
      CobaltProcessStateSummaryManager::DeserializeSnapshot(bad_uptime_payload)
          .has_value());

  // Invalid trim level (> 100).
  ProcessStateSnapshot bad_trim = valid;
  bad_trim.last_trim_level = 101;
  std::vector<uint8_t> bad_trim_payload =
      CobaltProcessStateSummaryManager::SerializeSnapshot(bad_trim);
  EXPECT_FALSE(
      CobaltProcessStateSummaryManager::DeserializeSnapshot(bad_trim_payload)
          .has_value());

  // Invalid milestone (>= 64).
  ProcessStateSnapshot bad_milestone = valid;
  bad_milestone.highest_milestone = 64;
  std::vector<uint8_t> bad_milestone_payload =
      CobaltProcessStateSummaryManager::SerializeSnapshot(bad_milestone);
  EXPECT_FALSE(CobaltProcessStateSummaryManager::DeserializeSnapshot(
                   bad_milestone_payload)
                   .has_value());

  // Unknown flag bits (e.g. 0x80).
  ProcessStateSnapshot bad_flags = valid;
  bad_flags.flags = 0x80;
  std::vector<uint8_t> bad_flags_payload =
      CobaltProcessStateSummaryManager::SerializeSnapshot(bad_flags);
  EXPECT_FALSE(
      CobaltProcessStateSummaryManager::DeserializeSnapshot(bad_flags_payload)
          .has_value());

  // Uninitialized / trivial 0xFFFFFFFF memory values.
  ProcessStateSnapshot bad_uninit_rss = valid;
  bad_uninit_rss.peak_rss_kb = 0xFFFFFFFF;
  std::vector<uint8_t> bad_uninit_rss_payload =
      CobaltProcessStateSummaryManager::SerializeSnapshot(bad_uninit_rss);
  EXPECT_FALSE(CobaltProcessStateSummaryManager::DeserializeSnapshot(
                   bad_uninit_rss_payload)
                   .has_value());
}

TEST(CobaltProcessStateSummaryManagerTest, V1LegacyPayloadDeserialization) {
  std::vector<uint8_t> v1_payload(kProcessStateSummaryV1PayloadSize, 0);
  v1_payload[0] = kProcessStateSummaryMagic;
  v1_payload[1] = 1;   // Version 1
  v1_payload[2] = 10;  // last_trim_level
  v1_payload[3] = kFlagForeground | kFlagMediaPlaying;

  // PID = 9999 (0x270F)
  v1_payload[4] = 0x0F;
  v1_payload[5] = 0x27;

  // Peak RSS = 50000 (0xC350)
  v1_payload[8] = 0x50;
  v1_payload[9] = 0xC3;

  // Peak PMF = 40000 (0x9C40)
  v1_payload[12] = 0x40;
  v1_payload[13] = 0x9C;

  // Peak V8 = 10000 (0x2710)
  v1_payload[16] = 0x10;
  v1_payload[17] = 0x27;

  // Uptime = 300 (0x012C)
  v1_payload[20] = 0x2C;
  v1_payload[21] = 0x01;

  // Compute 32-bit PersistentHash checksum over bytes [0..23].
  uint32_t checksum = base::PersistentHash(base::span(v1_payload).first(24u));
  v1_payload[24] = static_cast<uint8_t>(checksum & 0xFF);
  v1_payload[25] = static_cast<uint8_t>((checksum >> 8) & 0xFF);
  v1_payload[26] = static_cast<uint8_t>((checksum >> 16) & 0xFF);
  v1_payload[27] = static_cast<uint8_t>((checksum >> 24) & 0xFF);

  std::optional<ProcessStateSnapshot> deserialized =
      CobaltProcessStateSummaryManager::DeserializeSnapshot(v1_payload);
  ASSERT_TRUE(deserialized.has_value());
  EXPECT_EQ(deserialized->pid, 9999u);
  EXPECT_EQ(deserialized->last_trim_level, 10);
  EXPECT_EQ(deserialized->flags, kFlagForeground | kFlagMediaPlaying);
  EXPECT_EQ(deserialized->peak_rss_kb, 50000u);
  EXPECT_EQ(deserialized->peak_pmf_kb, 40000u);
  EXPECT_EQ(deserialized->peak_v8_code_kb, 10000u);
  EXPECT_EQ(deserialized->uptime_sec, 300u);
  EXPECT_EQ(deserialized->startup_milestones, 0u);
  EXPECT_EQ(deserialized->highest_milestone, 0u);
}

TEST(CobaltProcessStateSummaryManagerTest, ExitReasonToHistogramSuffixMapping) {
  EXPECT_EQ(ExitReasonToHistogramSuffix(0), "Anr");
  EXPECT_EQ(ExitReasonToHistogramSuffix(1), "Crash");
  EXPECT_EQ(ExitReasonToHistogramSuffix(2), "CrashNative");
  EXPECT_EQ(ExitReasonToHistogramSuffix(3), "DependencyDied");
  EXPECT_EQ(ExitReasonToHistogramSuffix(4), "ExcessiveResourceUsage");
  EXPECT_EQ(ExitReasonToHistogramSuffix(5), "ExitSelf");
  EXPECT_EQ(ExitReasonToHistogramSuffix(6), "InitializationFailure");
  EXPECT_EQ(ExitReasonToHistogramSuffix(7), "LowMemory");
  EXPECT_EQ(ExitReasonToHistogramSuffix(8), "Other");
  EXPECT_EQ(ExitReasonToHistogramSuffix(9), "PermissionChange");
  EXPECT_EQ(ExitReasonToHistogramSuffix(10), "Signaled");
  EXPECT_EQ(ExitReasonToHistogramSuffix(11), "Unknown");
  EXPECT_EQ(ExitReasonToHistogramSuffix(12), "UserRequested");
  EXPECT_EQ(ExitReasonToHistogramSuffix(13), "UserStopped");
  EXPECT_EQ(ExitReasonToHistogramSuffix(14), "ApiFailed");
  EXPECT_EQ(ExitReasonToHistogramSuffix(15), "Freezer");
  EXPECT_EQ(ExitReasonToHistogramSuffix(16), "PackageStateChange");
  EXPECT_EQ(ExitReasonToHistogramSuffix(17), "PackageUpdated");
  EXPECT_EQ(ExitReasonToHistogramSuffix(999), "Other");
}

}  // namespace
}  // namespace cobalt
