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
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "starboard/common/log.h"
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

  // Verify UTF-8 truncation boundaries on a non-overflowing collector:
  // 1) "ab" (2 bytes) + 3-byte euro sign + "z" = 6 bytes -> keeps
  // "ab\xE2\x82\xAC". 2) "abc" (3 bytes) + 3-byte euro sign = 6 bytes -> backs
  // up to "abc". 3) "ab" + orphan continuation bytes -> backs up to "ab".
  // 4) "a" (1 byte) + 4-byte code point + "z" = 6 bytes -> keeps
  // "a\xF0\x9F\x98\x80". 5) "ab" (2 bytes) + 4-byte code point = 6 bytes ->
  // backs up to "ab".
  CobaltLogCollector::Config utf8_config = config;
  utf8_config.max_entries = 10;
  CobaltLogCollector utf8_collector(utf8_config);
  CobaltLogCollector::ScopedConsumer utf8_consumer =
      utf8_collector.RegisterConsumer();
  utf8_collector.RecordLogMessage(LogSeverity::kInfo, "f.cc", 1,
                                  "ab\xE2\x82\xACz");
  utf8_collector.RecordLogMessage(LogSeverity::kInfo, "f.cc", 2,
                                  "abc\xE2\x82\xAC");
  utf8_collector.RecordLogMessage(LogSeverity::kInfo, "f.cc", 3,
                                  "ab\x80\x80\x80\x80");
  utf8_collector.RecordLogMessage(LogSeverity::kInfo, "f.cc", 4,
                                  "a\xF0\x9F\x98\x80z");
  utf8_collector.RecordLogMessage(LogSeverity::kInfo, "f.cc", 5,
                                  "ab\xF0\x9F\x98\x80");
  CobaltLogCollector::ReadResult utf8_result =
      utf8_collector.ReadSince(/*after_seq=*/0, /*max_entries=*/10,
                               /*max_bytes=*/64 * 1024);
  ASSERT_EQ(utf8_result.entries.size(), 5u);
  EXPECT_TRUE(utf8_result.entries[0].message_truncated);
  EXPECT_EQ(utf8_result.entries[0].message, "ab\xE2\x82\xAC");
  EXPECT_TRUE(utf8_result.entries[1].message_truncated);
  EXPECT_EQ(utf8_result.entries[1].message, "abc");
  EXPECT_TRUE(utf8_result.entries[2].message_truncated);
  EXPECT_EQ(utf8_result.entries[2].message, "ab");
  EXPECT_TRUE(utf8_result.entries[3].message_truncated);
  EXPECT_EQ(utf8_result.entries[3].message, "a\xF0\x9F\x98\x80");
  EXPECT_TRUE(utf8_result.entries[4].message_truncated);
  EXPECT_EQ(utf8_result.entries[4].message, "ab");
}

// Verifies that byte-budget pagination splits batches cleanly, still delivers
// an oversized single entry so readers never stall, supports zero limits for
// unbounded reads, and drops all entries when ring max_bytes is configured to
// zero.
TEST(CobaltLogCollectorTest, ReadSinceByteCapsOversizedEntryAndZeroConfig) {
  CobaltLogCollector::Config zero_bytes_config;
  zero_bytes_config.max_entries = 10;
  zero_bytes_config.max_bytes = 0;
  CobaltLogCollector zero_collector(zero_bytes_config);
  CobaltLogCollector::ScopedConsumer zero_consumer =
      zero_collector.RegisterConsumer();
  zero_collector.RecordLogMessage(LogSeverity::kInfo, "z.cc", 1, "ignored");
  EXPECT_EQ(zero_collector.SnapshotStats().buffered_entries, 0u);
  EXPECT_EQ(zero_collector.SnapshotStats().buffered_bytes, 0u);

  CobaltLogCollector collector;
  CobaltLogCollector::ScopedConsumer consumer = collector.RegisterConsumer();
  const std::string payload(80, 'a');
  collector.RecordLogMessage(LogSeverity::kInfo, "p.cc", 1, payload);
  collector.RecordLogMessage(LogSeverity::kInfo, "p.cc", 2, payload);
  collector.RecordLogMessage(LogSeverity::kInfo, "p.cc", 3, payload);

  // A max_bytes smaller than a single entry must still yield 1 entry so the
  // consumer makes forward progress instead of spinning on seq 0.
  CobaltLogCollector::ReadResult oversized_page =
      collector.ReadSince(/*after_seq=*/0, /*max_entries=*/10, /*max_bytes=*/1);
  ASSERT_EQ(oversized_page.entries.size(), 1u);
  EXPECT_EQ(oversized_page.entries[0].seq, 1u);
  EXPECT_EQ(oversized_page.next_seq, 1u);
  EXPECT_TRUE(oversized_page.truncated);

  // A byte budget that fits two entries splits three entries across two pages.
  const size_t approx_entry_bytes =
      sizeof(CobaltLogCollector::LogEntry) + payload.size() + 10;
  CobaltLogCollector::ReadResult byte_page1 =
      collector.ReadSince(/*after_seq=*/0, /*max_entries=*/10,
                          /*max_bytes=*/2 * approx_entry_bytes);
  ASSERT_EQ(byte_page1.entries.size(), 2u);
  EXPECT_EQ(byte_page1.entries[0].seq, 1u);
  EXPECT_EQ(byte_page1.entries[1].seq, 2u);
  EXPECT_EQ(byte_page1.next_seq, 2u);
  EXPECT_TRUE(byte_page1.truncated);

  CobaltLogCollector::ReadResult byte_page2 =
      collector.ReadSince(byte_page1.next_seq, /*max_entries=*/10,
                          /*max_bytes=*/2 * approx_entry_bytes);
  ASSERT_EQ(byte_page2.entries.size(), 1u);
  EXPECT_EQ(byte_page2.entries[0].seq, 3u);
  EXPECT_EQ(byte_page2.next_seq, 3u);
  EXPECT_FALSE(byte_page2.truncated);

  // Passing 0 for max_entries and max_bytes disables read caps and returns the
  // remaining buffered entries in one batch.
  CobaltLogCollector::ReadResult unbounded =
      collector.ReadSince(oversized_page.next_seq, /*max_entries=*/0,
                          /*max_bytes=*/0);
  ASSERT_EQ(unbounded.entries.size(), 2u);
  EXPECT_EQ(unbounded.entries[0].seq, 2u);
  EXPECT_EQ(unbounded.entries[1].seq, 3u);
  EXPECT_EQ(unbounded.next_seq, 3u);
  EXPECT_FALSE(unbounded.truncated);

  // Future cursor past latest_seq returns empty without rewinding next_seq.
  CobaltLogCollector::ReadResult future =
      collector.ReadSince(/*after_seq=*/999, /*max_entries=*/10,
                          /*max_bytes=*/64 * 1024);
  EXPECT_TRUE(future.entries.empty());
  EXPECT_EQ(future.next_seq, 999u);
  EXPECT_FALSE(future.truncated);
}

// Verifies ScopedConsumer move semantics keep collection active across
// ownership transfers without double-unregistering, and validates Chromium and
// Starboard severity mappings.
TEST(CobaltLogCollectorTest, ScopedConsumerMoveSemanticsAndSeverityMappings) {
  CobaltLogCollector collector;
  {
    CobaltLogCollector::ScopedConsumer original = collector.RegisterConsumer();
    EXPECT_TRUE(static_cast<bool>(original));

    CobaltLogCollector::ScopedConsumer moved_ctor(std::move(original));
    EXPECT_FALSE(static_cast<bool>(original));
    EXPECT_TRUE(static_cast<bool>(moved_ctor));
    EXPECT_TRUE(collector.SnapshotStats().active);

    CobaltLogCollector::ScopedConsumer moved_assign =
        collector.RegisterConsumer();
    moved_assign = std::move(moved_ctor);
    EXPECT_FALSE(static_cast<bool>(moved_ctor));
    EXPECT_TRUE(static_cast<bool>(moved_assign));
    EXPECT_TRUE(collector.SnapshotStats().active);
  }
  EXPECT_FALSE(collector.SnapshotStats().active);

  EXPECT_EQ(CobaltLogCollector::SeverityFromBaseLogLevel(-1),
            LogSeverity::kVerbose);
  EXPECT_EQ(CobaltLogCollector::SeverityFromBaseLogLevel(logging::LOGGING_INFO),
            LogSeverity::kInfo);
  EXPECT_EQ(
      CobaltLogCollector::SeverityFromBaseLogLevel(logging::LOGGING_WARNING),
      LogSeverity::kWarning);
  EXPECT_EQ(
      CobaltLogCollector::SeverityFromBaseLogLevel(logging::LOGGING_ERROR),
      LogSeverity::kError);
  EXPECT_EQ(
      CobaltLogCollector::SeverityFromBaseLogLevel(logging::LOGGING_FATAL),
      LogSeverity::kFatal);

  EXPECT_EQ(CobaltLogCollector::SeverityFromStarboardLogPriority(
                kSbLogPriorityUnknown),
            LogSeverity::kInfo);
  EXPECT_EQ(
      CobaltLogCollector::SeverityFromStarboardLogPriority(kSbLogPriorityInfo),
      LogSeverity::kInfo);
  EXPECT_EQ(CobaltLogCollector::SeverityFromStarboardLogPriority(
                kSbLogPriorityWarning),
            LogSeverity::kWarning);
  EXPECT_EQ(
      CobaltLogCollector::SeverityFromStarboardLogPriority(kSbLogPriorityError),
      LogSeverity::kError);
  EXPECT_EQ(
      CobaltLogCollector::SeverityFromStarboardLogPriority(kSbLogPriorityFatal),
      LogSeverity::kFatal);
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

// Verifies that replacing the notification listener or dropping the final
// consumer cancels any in-flight delayed notification so a stale task never
// invokes a newly registered callback on the old task runner.
TEST(CobaltLogCollectorTest, StaleNotificationDoesNotInvokeReplacedCallback) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  CobaltLogCollector::Config config;
  config.notify_delay = base::Milliseconds(50);
  CobaltLogCollector collector(config);
  CobaltLogCollector::ScopedConsumer consumer = collector.RegisterConsumer();

  scoped_refptr<base::SequencedTaskRunner> runner_a =
      base::SequencedTaskRunner::GetCurrentDefault();
  scoped_refptr<base::SequencedTaskRunner> runner_b =
      base::ThreadPool::CreateSequencedTaskRunner({});

  int cb_a_calls = 0;
  std::atomic<int> cb_b_calls{0};
  std::atomic<bool> cb_b_ran_on_runner_b{true};
  base::WaitableEvent cb_b_fired;

  collector.SetNotifyCallback(
      runner_a, base::BindLambdaForTesting([&]() { ++cb_a_calls; }));
  collector.RecordLogMessage(LogSeverity::kInfo, "stale.cc", 1, "first");

  // Replace with (runner_b, cb_b) before the delayed task on runner_a runs.
  collector.SetNotifyCallback(
      runner_b, base::BindLambdaForTesting([&]() {
        if (!runner_b->RunsTasksInCurrentSequence()) {
          cb_b_ran_on_runner_b.store(false, std::memory_order_relaxed);
        }
        cb_b_calls.fetch_add(1, std::memory_order_relaxed);
        cb_b_fired.Signal();
      }));

  // Let runner_a's stale delayed task fire: neither callback may execute.
  task_environment.FastForwardBy(base::Milliseconds(50));
  EXPECT_EQ(cb_a_calls, 0);
  EXPECT_EQ(cb_b_calls.load(std::memory_order_relaxed), 0);
  EXPECT_TRUE(cb_b_ran_on_runner_b.load(std::memory_order_relaxed));

  // New logs schedule a fresh notification on runner_b and run there.
  collector.RecordLogMessage(LogSeverity::kInfo, "stale.cc", 2, "second");
  task_environment.FastForwardBy(base::Milliseconds(50));
  cb_b_fired.Wait();
  EXPECT_EQ(cb_b_calls.load(std::memory_order_relaxed), 1);
  EXPECT_TRUE(cb_b_ran_on_runner_b.load(std::memory_order_relaxed));

  // Dropping the final consumer also invalidates any queued notification.
  int after_unregister_calls = 0;
  collector.SetNotifyCallback(runner_a, base::BindLambdaForTesting([&]() {
                                ++after_unregister_calls;
                              }));
  collector.RecordLogMessage(LogSeverity::kInfo, "stale.cc", 3, "third");
  consumer.Reset();
  task_environment.FastForwardBy(base::Milliseconds(50));
  EXPECT_EQ(after_unregister_calls, 0);
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

// Verifies that replacing or clearing the notification callback destroys the
// previous callback outside the collector lock so a bound object's destructor
// can log or query collector stats without self-deadlocking.
TEST(CobaltLogCollectorTest, SetNotifyCallbackDestroysOldCallbackOutsideLock) {
  base::test::TaskEnvironment task_environment;
  CobaltLogCollector collector;
  CobaltLogCollector::ScopedConsumer consumer = collector.RegisterConsumer();

  bool destructor_ran = false;
  auto guard = std::make_shared<base::ScopedClosureRunner>(
      base::BindLambdaForTesting([&]() {
        destructor_ran = true;
        collector.RecordLogMessage(LogSeverity::kInfo, "dtor.cc", 1,
                                   "from-callback-dtor");
        EXPECT_TRUE(collector.SnapshotStats().active);
      }));
  collector.SetNotifyCallback(
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindLambdaForTesting([guard]() { (void)guard; }));
  guard.reset();
  ASSERT_FALSE(destructor_ran);

  collector.SetNotifyCallback(nullptr, base::RepeatingClosure());
  EXPECT_TRUE(destructor_ran);

  CobaltLogCollector::ReadResult result =
      collector.ReadSince(/*after_seq=*/0, /*max_entries=*/10,
                          /*max_bytes=*/64 * 1024);
  ASSERT_EQ(result.entries.size(), 1u);
  EXPECT_EQ(result.entries[0].message, "from-callback-dtor");
}

std::atomic<int> g_chained_handler_calls{0};
std::atomic<bool> g_trigger_reentrant_log{false};
constexpr char kBaseHookTestSentinel[] = "cobalt-collector-global-hook-test";

bool TestPreviousLogHandler(int severity,
                            const char* file,
                            int line,
                            size_t message_start,
                            const std::string& str) {
  g_chained_handler_calls.fetch_add(1, std::memory_order_relaxed);
  if (str.find(kBaseHookTestSentinel) != std::string::npos &&
      g_trigger_reentrant_log.exchange(false, std::memory_order_acq_rel)) {
    LOG(INFO) << "dropped-reentrant-log";
  }
  return false;
}

// Verifies that installing the global base::logging hook captures LOG() output,
// chains cleanly to any pre-existing handler without infinite recursion across
// repeated InstallHooks() calls, and drops re-entrant logs without deadlock.
TEST(CobaltLogCollectorTest, GlobalBaseLoggingHookChainingAndIdempotency) {
  logging::LogMessageHandlerFunction saved_handler =
      logging::GetLogMessageHandler();
  CobaltLogCollector* collector = CobaltLogCollector::GetInstance();
  base::ScopedClosureRunner restore_handlers(base::BindLambdaForTesting([&]() {
    g_trigger_reentrant_log.store(false, std::memory_order_release);
    collector->UninstallHooksForTesting();
    logging::SetLogMessageHandler(saved_handler);
  }));

  g_chained_handler_calls.store(0, std::memory_order_relaxed);
  logging::SetLogMessageHandler(&TestPreviousLogHandler);

  collector->UninstallHooksForTesting();
  collector->InstallHooks();
  // Repeated calls must remain idempotent and never chain to self.
  collector->InstallHooks();

  const uint64_t start_seq = collector->SnapshotStats().latest_seq;
  {
    CobaltLogCollector::ScopedConsumer consumer = collector->RegisterConsumer();
    g_trigger_reentrant_log.store(true, std::memory_order_release);
    LOG(WARNING) << kBaseHookTestSentinel;

    CobaltLogCollector::ReadResult result =
        collector->ReadSince(start_seq, /*max_entries=*/50,
                             /*max_bytes=*/64 * 1024);
    bool found_warning = false;
    bool found_reentrant = false;
    for (const CobaltLogCollector::LogEntry& entry : result.entries) {
      if (entry.message == kBaseHookTestSentinel &&
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

  EXPECT_FALSE(g_trigger_reentrant_log.load(std::memory_order_acquire));
  EXPECT_GE(g_chained_handler_calls.load(std::memory_order_relaxed), 1);

  collector->UninstallHooksForTesting();
  EXPECT_EQ(logging::GetLogMessageHandler(), &TestPreviousLogHandler);
}

std::atomic<int> g_chained_sb_handler_calls{0};
std::atomic<bool> g_trigger_reentrant_sb_log{false};
constexpr char kStarboardHookTestSentinel[] =
    "cobalt-collector-sblog-hook-test";

bool TestPreviousStarboardLogHandler(SbLogPriority priority,
                                     const char* file,
                                     int line,
                                     size_t message_start,
                                     const std::string& str) {
  g_chained_sb_handler_calls.fetch_add(1, std::memory_order_relaxed);
  if (str.find(kStarboardHookTestSentinel) != std::string::npos &&
      g_trigger_reentrant_sb_log.exchange(false, std::memory_order_acq_rel)) {
    SB_LOG(INFO) << "dropped-reentrant-sb-log";
  }
  return false;
}

// Verifies that installing hooks also captures SB_LOG() messages emitted inside
// libcobalt.so, chains cleanly to any prior Starboard handler, and drops
// same-thread re-entrant logs without deadlock.
TEST(CobaltLogCollectorTest, GlobalStarboardLogHookCapturesSbLogAndChains) {
  starboard::LogMessageHandlerFunction saved_sb_handler =
      starboard::GetLogMessageHandler();
  CobaltLogCollector* collector = CobaltLogCollector::GetInstance();
  base::ScopedClosureRunner restore_handlers(base::BindLambdaForTesting([&]() {
    g_trigger_reentrant_sb_log.store(false, std::memory_order_release);
    collector->UninstallHooksForTesting();
    starboard::SetLogMessageHandler(saved_sb_handler);
  }));

  g_chained_sb_handler_calls.store(0, std::memory_order_relaxed);
  starboard::SetLogMessageHandler(&TestPreviousStarboardLogHandler);

  collector->UninstallHooksForTesting();
  collector->InstallHooks();
  collector->InstallHooks();

  const uint64_t start_seq = collector->SnapshotStats().latest_seq;
  {
    CobaltLogCollector::ScopedConsumer consumer = collector->RegisterConsumer();
    g_trigger_reentrant_sb_log.store(true, std::memory_order_release);
    SB_LOG(ERROR) << kStarboardHookTestSentinel;

    CobaltLogCollector::ReadResult result =
        collector->ReadSince(start_seq, /*max_entries=*/50,
                             /*max_bytes=*/64 * 1024);
    bool found_error = false;
    bool found_reentrant = false;
    for (const CobaltLogCollector::LogEntry& entry : result.entries) {
      if (entry.message == kStarboardHookTestSentinel &&
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

  EXPECT_FALSE(g_trigger_reentrant_sb_log.load(std::memory_order_acquire));
  EXPECT_GE(g_chained_sb_handler_calls.load(std::memory_order_relaxed), 1);

  collector->UninstallHooksForTesting();
  EXPECT_EQ(starboard::GetLogMessageHandler(),
            &TestPreviousStarboardLogHandler);
}

}  // namespace
}  // namespace cobalt
