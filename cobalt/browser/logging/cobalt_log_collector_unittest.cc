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

#include "cobalt/browser/logging/cobalt_log_collector.h"

#include <atomic>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace {

using LogSeverity = CobaltLogCollector::LogSeverity;

// Verifies that Cobalt incurs zero ring-buffer allocations when neither
// DevTools nor a test harness has opted into log capture, and frees buffered
// entries as soon as the last consumer stops.
TEST(CobaltLogCollectorTest, IdleZeroCostAndReleaseOnLastUnregister) {
  CobaltLogCollector collector;

  collector.RecordLogMessage(LogSeverity::kInfo, "path/to/foo.cc", 10,
                             "ignored while idle");
  CobaltLogCollector::Stats idle_stats = collector.SnapshotStats();
  EXPECT_FALSE(idle_stats.active);
  EXPECT_EQ(idle_stats.buffered_entries, 0u);
  EXPECT_EQ(idle_stats.buffered_bytes, 0u);
  EXPECT_EQ(idle_stats.latest_seq, 0u);

  {
    CobaltLogCollector::ScopedConsumer consumer_a =
        collector.RegisterConsumer();
    CobaltLogCollector::ScopedConsumer consumer_b =
        collector.RegisterConsumer();
    collector.RecordLogMessage(LogSeverity::kInfo, "path/to/foo.cc", 20,
                               "captured\n");
    CobaltLogCollector::Stats active_stats = collector.SnapshotStats();
    EXPECT_TRUE(active_stats.active);
    EXPECT_EQ(active_stats.buffered_entries, 1u);
    EXPECT_GT(active_stats.buffered_bytes, 0u);
    EXPECT_EQ(active_stats.latest_seq, 1u);

    // Dropping one consumer keeps the buffer alive for the second consumer.
    consumer_a.Reset();
    EXPECT_TRUE(collector.SnapshotStats().active);
    EXPECT_EQ(collector.SnapshotStats().buffered_entries, 1u);
  }

  // Dropping the final consumer releases buffered storage immediately while
  // preserving monotonic sequence numbers for any future session.
  CobaltLogCollector::Stats released_stats = collector.SnapshotStats();
  EXPECT_FALSE(released_stats.active);
  EXPECT_EQ(released_stats.buffered_entries, 0u);
  EXPECT_EQ(released_stats.buffered_bytes, 0u);
  EXPECT_EQ(released_stats.latest_seq, 1u);
}

// Verifies that consumers receive structured metadata (basename, line, UTC
// timestamp, stripped trailing newline) and can paginate exact-once using
// monotonic sequence cursors and severity filters.
TEST(CobaltLogCollectorTest,
     StructuredFieldsCursorPaginationAndSeverityFilter) {
  CobaltLogCollector collector;
  CobaltLogCollector::ScopedConsumer consumer = collector.RegisterConsumer();

  collector.RecordLogMessage(LogSeverity::kVerbose, "a/b/verbose.cc", 1,
                             "verbose msg\r\n");
  collector.RecordLogMessage(LogSeverity::kInfo, "a/b/info.cc", 2,
                             "info msg\n");
  collector.RecordLogMessage(LogSeverity::kWarning, "a/b/warn.cc", 3,
                             "warn msg");
  collector.RecordLogMessage(LogSeverity::kError, "a/b/err.cc", 4, "err msg");

  CobaltLogCollector::ReadResult page1 =
      collector.ReadSince(/*after_seq=*/0, /*max_entries=*/1,
                          /*max_bytes=*/64 * 1024, LogSeverity::kWarning);
  ASSERT_EQ(page1.entries.size(), 1u);
  EXPECT_TRUE(page1.truncated);
  EXPECT_EQ(page1.entries[0].seq, 3u);
  EXPECT_EQ(page1.entries[0].severity, LogSeverity::kWarning);
  EXPECT_EQ(page1.entries[0].file, "warn.cc");
  EXPECT_EQ(page1.entries[0].line, 3);
  EXPECT_EQ(page1.entries[0].message, "warn msg");
  EXPECT_GT(page1.entries[0].timestamp_ms, 0.0);
  EXPECT_EQ(page1.next_seq, 3u);

  CobaltLogCollector::ReadResult page2 =
      collector.ReadSince(page1.next_seq, /*max_entries=*/10,
                          /*max_bytes=*/64 * 1024, LogSeverity::kWarning);
  ASSERT_EQ(page2.entries.size(), 1u);
  EXPECT_FALSE(page2.truncated);
  EXPECT_EQ(page2.entries[0].seq, 4u);
  EXPECT_EQ(page2.entries[0].file, "err.cc");
  EXPECT_EQ(page2.next_seq, 4u);

  // Subsequent read with no new entries returns empty with unchanged cursor.
  CobaltLogCollector::ReadResult empty =
      collector.ReadSince(page2.next_seq, /*max_entries=*/10,
                          /*max_bytes=*/64 * 1024);
  EXPECT_TRUE(empty.entries.empty());
  EXPECT_FALSE(empty.truncated);
  EXPECT_EQ(empty.next_seq, 4u);
}

// Verifies that long endurance runs stay within both entry-count and byte-size
// ring caps, report exact dropped-entry counts on overflow, and truncate
// oversized messages on a valid UTF-8 boundary.
TEST(CobaltLogCollectorTest, RingOverflowDropAccountingAndUtf8Truncation) {
  CobaltLogCollector::Config config;
  config.max_entries = 3;
  config.max_bytes = 64 * 1024;
  config.max_message_bytes = 5;
  CobaltLogCollector collector(config);
  CobaltLogCollector::ScopedConsumer consumer = collector.RegisterConsumer();

  // "ab" (2 bytes) + 3-byte euro sign (0xE2 0x82 0xAC) + "z" = 6 bytes.
  // Truncating at 5 bytes keeps "ab\xE2\x82\xAC".
  collector.RecordLogMessage(LogSeverity::kInfo, "f.cc", 1, "ab\xE2\x82\xACz");
  // "abc" (3 bytes) + 3-byte euro sign = 6 bytes. Truncating at 5 bytes lands
  // inside the euro sign and must back up to "abc" (3 bytes).
  collector.RecordLogMessage(LogSeverity::kInfo, "f.cc", 2, "abc\xE2\x82\xAC");
  collector.RecordLogMessage(LogSeverity::kInfo, "f.cc", 3, "m3");
  collector.RecordLogMessage(LogSeverity::kInfo, "f.cc", 4, "m4");
  collector.RecordLogMessage(LogSeverity::kInfo, "f.cc", 5, "m5");

  CobaltLogCollector::ReadResult result =
      collector.ReadSince(/*after_seq=*/0, /*max_entries=*/10,
                          /*max_bytes=*/64 * 1024);
  EXPECT_EQ(result.dropped_count, 2u);
  ASSERT_EQ(result.entries.size(), 3u);
  EXPECT_EQ(result.entries[0].seq, 3u);
  EXPECT_EQ(result.entries[1].seq, 4u);
  EXPECT_EQ(result.entries[2].seq, 5u);

  // Verify the UTF-8 truncation boundary with a non-overflowing collector.
  CobaltLogCollector utf8_collector(config);
  CobaltLogCollector::ScopedConsumer utf8_consumer =
      utf8_collector.RegisterConsumer();
  utf8_collector.RecordLogMessage(LogSeverity::kInfo, "f.cc", 1,
                                  "abc\xE2\x82\xAC");
  CobaltLogCollector::ReadResult utf8_result =
      utf8_collector.ReadSince(/*after_seq=*/0, /*max_entries=*/10,
                               /*max_bytes=*/64 * 1024);
  ASSERT_EQ(utf8_result.entries.size(), 1u);
  EXPECT_TRUE(utf8_result.entries[0].message_truncated);
  EXPECT_EQ(utf8_result.entries[0].message, "abc");
}

// Verifies that the collector enforces its byte-size cap even when entry count
// is still below max_entries, keeping memory bounded during bursts of long
// lines.
TEST(CobaltLogCollectorTest, ByteBudgetEvictsOldestEntries) {
  CobaltLogCollector::Config config;
  config.max_entries = 100;
  // Enough room for roughly two entries with 40-byte messages.
  config.max_bytes = 2 * (sizeof(CobaltLogCollector::LogEntry) + 50);
  config.max_message_bytes = 1024;
  CobaltLogCollector collector(config);
  CobaltLogCollector::ScopedConsumer consumer = collector.RegisterConsumer();

  std::string payload(40, 'x');
  for (int i = 0; i < 6; ++i) {
    collector.RecordLogMessage(LogSeverity::kInfo, "b.cc", i, payload);
  }

  CobaltLogCollector::Stats stats = collector.SnapshotStats();
  EXPECT_LE(stats.buffered_bytes, config.max_bytes);
  EXPECT_LT(stats.buffered_entries, 6u);
  EXPECT_GT(stats.dropped_count, 0u);
}

// Verifies that high-frequency bursts coalesce into a single delayed UI-thread
// notification callback and remain safe if a local collector is destroyed
// before a queued notification runs.
TEST(CobaltLogCollectorTest, CoalescedPushNotificationsAndSafeTeardown) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  CobaltLogCollector::Config config;
  config.notify_delay = base::Milliseconds(50);

  int callback_count = 0;
  {
    CobaltLogCollector collector(config);
    CobaltLogCollector::ScopedConsumer consumer = collector.RegisterConsumer();
    collector.SetNotifyCallback(
        base::SequencedTaskRunner::GetCurrentDefault(),
        base::BindLambdaForTesting([&]() { ++callback_count; }));

    for (int i = 0; i < 20; ++i) {
      collector.RecordLogMessage(LogSeverity::kInfo, "burst.cc", i, "burst");
    }

    EXPECT_EQ(callback_count, 0);
    task_environment.FastForwardBy(base::Milliseconds(50));
    EXPECT_EQ(callback_count, 1);

    // Queue another notification and destroy the collector before it fires to
    // prove DispatchNotification does not dereference a freed instance.
    collector.RecordLogMessage(LogSeverity::kInfo, "burst.cc", 99, "late");
  }

  task_environment.FastForwardBy(base::Milliseconds(100));
  EXPECT_EQ(callback_count, 1);
}

// Verifies that logging from many concurrent non-sequenced ThreadPool worker
// threads succeeds without DCHECKing on SequencedTaskRunner::GetCurrentDefault
// (regression coverage for b/347723488).
TEST(CobaltLogCollectorTest, ConcurrentWorkerThreadsSafeWithoutSequence) {
  base::test::TaskEnvironment task_environment;

  CobaltLogCollector::Config config;
  config.notify_delay = base::Milliseconds(1);
  CobaltLogCollector collector(config);
  CobaltLogCollector::ScopedConsumer consumer = collector.RegisterConsumer();

  std::atomic<int> notifications{0};
  base::RunLoop run_loop;
  collector.SetNotifyCallback(
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindLambdaForTesting([&]() {
        if (notifications.fetch_add(1, std::memory_order_relaxed) == 0) {
          run_loop.Quit();
        }
      }));

  constexpr int kNumWorkers = 8;
  constexpr int kLogsPerWorker = 25;
  std::atomic<int> remaining{kNumWorkers};
  base::WaitableEvent all_workers_done;

  for (int worker = 0; worker < kNumWorkers; ++worker) {
    base::ThreadPool::PostTask(
        FROM_HERE, base::BindLambdaForTesting([&, worker]() {
          for (int i = 0; i < kLogsPerWorker; ++i) {
            collector.RecordLogMessage(LogSeverity::kInfo, "worker.cc", i,
                                       "w" + base::NumberToString(worker) +
                                           "-" + base::NumberToString(i));
          }
          if (remaining.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            all_workers_done.Signal();
          }
        }));
  }

  all_workers_done.Wait();
  run_loop.Run();

  CobaltLogCollector::ReadResult result =
      collector.ReadSince(/*after_seq=*/0, /*max_entries=*/500,
                          /*max_bytes=*/512 * 1024);
  EXPECT_EQ(result.entries.size(),
            static_cast<size_t>(kNumWorkers * kLogsPerWorker));
  EXPECT_EQ(result.dropped_count, 0u);
  for (size_t i = 1; i < result.entries.size(); ++i) {
    EXPECT_GT(result.entries[i].seq, result.entries[i - 1].seq);
  }
}

std::atomic<int> g_chained_handler_calls{0};
bool g_trigger_reentrant_log = false;

bool TestPreviousLogHandler(int severity,
                            const char* file,
                            int line,
                            size_t message_start,
                            const std::string& str) {
  g_chained_handler_calls.fetch_add(1, std::memory_order_relaxed);
  if (g_trigger_reentrant_log) {
    g_trigger_reentrant_log = false;
    CobaltLogCollector::GetInstance()->RecordLogMessage(
        LogSeverity::kInfo, "reentrant.cc", 1, "dropped-reentrant-log");
  }
  return false;
}

// Verifies that installing the global base::logging hook captures LOG() output,
// chains cleanly to any pre-existing handler without infinite recursion across
// repeated InstallHooks() calls, and drops re-entrant logs without deadlock.
TEST(CobaltLogCollectorTest, GlobalBaseLoggingHookChainingAndIdempotency) {
  logging::LogMessageHandlerFunction saved_handler =
      logging::GetLogMessageHandler();
  g_chained_handler_calls.store(0, std::memory_order_relaxed);
  g_trigger_reentrant_log = true;
  logging::SetLogMessageHandler(&TestPreviousLogHandler);

  CobaltLogCollector* collector = CobaltLogCollector::GetInstance();
  collector->UninstallHooksForTesting();
  collector->InstallHooks();
  // Repeated calls must remain idempotent and never chain to self.
  collector->InstallHooks();

  const uint64_t start_seq = collector->SnapshotStats().latest_seq;
  {
    CobaltLogCollector::ScopedConsumer consumer = collector->RegisterConsumer();
    LOG(WARNING) << "cobalt-collector-global-hook-test";

    CobaltLogCollector::ReadResult result =
        collector->ReadSince(start_seq, /*max_entries=*/50,
                             /*max_bytes=*/64 * 1024);
    bool found_warning = false;
    bool found_reentrant = false;
    for (const CobaltLogCollector::LogEntry& entry : result.entries) {
      if (entry.message == "cobalt-collector-global-hook-test" &&
          entry.severity == LogSeverity::kWarning) {
        found_warning = true;
      }
      if (entry.message == "dropped-reentrant-log") {
        found_reentrant = true;
      }
    }
    EXPECT_TRUE(found_warning);
    EXPECT_FALSE(found_reentrant);
  }

  EXPECT_FALSE(g_trigger_reentrant_log);
  EXPECT_GE(g_chained_handler_calls.load(std::memory_order_relaxed), 1);

  collector->UninstallHooksForTesting();
  EXPECT_EQ(logging::GetLogMessageHandler(), &TestPreviousLogHandler);
  logging::SetLogMessageHandler(saved_handler);
}

std::atomic<int> g_chained_sb_handler_calls{0};
bool g_trigger_reentrant_sb_log = false;

bool TestPreviousStarboardLogHandler(SbLogPriority priority,
                                     const char* file,
                                     int line,
                                     size_t message_start,
                                     const std::string& str) {
  g_chained_sb_handler_calls.fetch_add(1, std::memory_order_relaxed);
  if (g_trigger_reentrant_sb_log) {
    g_trigger_reentrant_sb_log = false;
    CobaltLogCollector::GetInstance()->RecordLogMessage(
        LogSeverity::kInfo, "reentrant_sb.cc", 1, "dropped-reentrant-sb-log");
  }
  return false;
}

// Verifies that installing hooks also captures SB_LOG() messages emitted inside
// libcobalt.so, chains cleanly to any prior Starboard handler, and drops
// same-thread re-entrant logs without deadlock.
TEST(CobaltLogCollectorTest, GlobalStarboardLogHookCapturesSbLogAndChains) {
  starboard::LogMessageHandlerFunction saved_sb_handler =
      starboard::GetLogMessageHandler();
  g_chained_sb_handler_calls.store(0, std::memory_order_relaxed);
  g_trigger_reentrant_sb_log = true;
  starboard::SetLogMessageHandler(&TestPreviousStarboardLogHandler);

  CobaltLogCollector* collector = CobaltLogCollector::GetInstance();
  collector->UninstallHooksForTesting();
  collector->InstallHooks();
  collector->InstallHooks();

  const uint64_t start_seq = collector->SnapshotStats().latest_seq;
  {
    CobaltLogCollector::ScopedConsumer consumer = collector->RegisterConsumer();
    SB_LOG(ERROR) << "cobalt-collector-sblog-hook-test";

    CobaltLogCollector::ReadResult result =
        collector->ReadSince(start_seq, /*max_entries=*/50,
                             /*max_bytes=*/64 * 1024);
    bool found_error = false;
    bool found_reentrant = false;
    for (const CobaltLogCollector::LogEntry& entry : result.entries) {
      if (entry.message == "cobalt-collector-sblog-hook-test" &&
          entry.severity == LogSeverity::kError) {
        found_error = true;
      }
      if (entry.message == "dropped-reentrant-sb-log") {
        found_reentrant = true;
      }
    }
    EXPECT_TRUE(found_error);
    EXPECT_FALSE(found_reentrant);
  }

  EXPECT_FALSE(g_trigger_reentrant_sb_log);
  EXPECT_GE(g_chained_sb_handler_calls.load(std::memory_order_relaxed), 1);

  collector->UninstallHooksForTesting();
  EXPECT_EQ(starboard::GetLogMessageHandler(),
            &TestPreviousStarboardLogHandler);
  starboard::SetLogMessageHandler(saved_sb_handler);
}

}  // namespace
}  // namespace cobalt
