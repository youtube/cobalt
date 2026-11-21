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

#ifndef COBALT_BROWSER_LOGGING_COBALT_LOG_COLLECTOR_H_
#define COBALT_BROWSER_LOGGING_COBALT_LOG_COLLECTOR_H_

#include <stddef.h>
#include <stdint.h>

#include <atomic>
#include <deque>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "starboard/common/log.h"

namespace cobalt {

// Thread-safe, opt-in in-process ring buffer for native Cobalt logs.
//
// Consumers register via RegisterConsumer() to activate collection. When no
// consumer is registered, the global log hook returns immediately on an atomic
// load without taking a lock or allocating ring storage.
class CobaltLogCollector {
 public:
  enum class LogSeverity {
    kVerbose = 0,
    kInfo = 1,
    kWarning = 2,
    kError = 3,
    kFatal = 4,
  };

  struct Config {
    size_t max_entries = 2048;
    size_t max_bytes = 512 * 1024;
    size_t max_message_bytes = 2048;
    base::TimeDelta notify_delay = base::Milliseconds(50);
  };

  struct LogEntry {
    uint64_t seq = 0;
    double timestamp_ms = 0.0;
    LogSeverity severity = LogSeverity::kInfo;
    std::string file;
    int line = 0;
    std::string thread_name;
    std::string message;
    bool message_truncated = false;
  };

  struct ReadResult {
    std::vector<LogEntry> entries;
    uint64_t next_seq = 0;
    uint64_t dropped_count = 0;
    bool truncated = false;
  };

  struct Stats {
    bool active = false;
    size_t buffered_entries = 0;
    size_t buffered_bytes = 0;
    uint64_t dropped_count = 0;
    uint64_t latest_seq = 0;
  };

  // Move-only RAII token keeping log collection active while alive.
  class ScopedConsumer {
   public:
    ScopedConsumer() = default;
    ScopedConsumer(ScopedConsumer&& other) noexcept;
    ScopedConsumer& operator=(ScopedConsumer&& other) noexcept;
    ScopedConsumer(const ScopedConsumer&) = delete;
    ScopedConsumer& operator=(const ScopedConsumer&) = delete;
    ~ScopedConsumer();

    void Reset();
    explicit operator bool() const { return collector_ != nullptr; }

   private:
    friend class CobaltLogCollector;
    explicit ScopedConsumer(CobaltLogCollector* collector);

    raw_ptr<CobaltLogCollector> collector_ = nullptr;
  };

  // Returns the process-wide singleton (backed by base::NoDestructor so late
  // worker logs during shutdown never touch a destroyed collector).
  static CobaltLogCollector* GetInstance();

  static LogSeverity SeverityFromBaseLogLevel(int base_severity);
  static LogSeverity SeverityFromStarboardLogPriority(SbLogPriority priority);

  CobaltLogCollector();
  explicit CobaltLogCollector(Config config);
  CobaltLogCollector(const CobaltLogCollector&) = delete;
  CobaltLogCollector& operator=(const CobaltLogCollector&) = delete;
  ~CobaltLogCollector();

  // Installs the process-wide base::logging and starboard::logging message
  // handlers once on the singleton instance. Safe to call multiple times.
  void InstallHooks();

  // Restores the previous base::logging and starboard::logging handlers
  // installed before InstallHooks(). Intended only for tests that exercise
  // global hooks.
  void UninstallHooksForTesting();

  // Activates log collection until the returned ScopedConsumer is destroyed or
  // reset. When the last consumer unregisters, buffered entries are released.
  [[nodiscard]] ScopedConsumer RegisterConsumer();

  // Registers (or clears when |task_runner| or |callback| is null) a coalesced
  // push notification callback invoked on |task_runner| after new logs arrive.
  void SetNotifyCallback(scoped_refptr<base::SequencedTaskRunner> task_runner,
                         base::RepeatingClosure callback);

  // Records a log message from any thread. No-ops immediately when no consumer
  // is registered or when called re-entrantly on the same thread.
  void RecordLogMessage(LogSeverity severity,
                        const char* file,
                        int line,
                        std::string_view message);

  // Returns buffered entries with seq > |after_seq| and severity >=
  // |min_severity|, bounded by |max_entries| and |max_bytes|.
  ReadResult ReadSince(uint64_t after_seq,
                       size_t max_entries,
                       size_t max_bytes,
                       LogSeverity min_severity = LogSeverity::kVerbose) const;

  // Returns a snapshot of current collector state and buffer utilization.
  Stats SnapshotStats() const;

 private:
  struct NotifyState : public base::RefCountedThreadSafe<NotifyState> {
    base::Lock lock;
    raw_ptr<CobaltLogCollector> collector GUARDED_BY(lock) = nullptr;

   private:
    friend class base::RefCountedThreadSafe<NotifyState>;
    ~NotifyState() = default;
  };

  static bool BaseLogMessageHandler(int severity,
                                    const char* file,
                                    int line,
                                    size_t message_start,
                                    const std::string& str);

  static bool StarboardLogMessageHandler(SbLogPriority priority,
                                         const char* file,
                                         int line,
                                         size_t message_start,
                                         const std::string& str);

  static void DispatchNotification(scoped_refptr<NotifyState> notify_state);

  void UnregisterConsumer();
  void RecordLogMessageInternal(LogSeverity severity,
                                const char* file,
                                int line,
                                std::string_view message);
  base::RepeatingClosure TakePendingNotification();

  const Config config_;
  const scoped_refptr<NotifyState> notify_state_;

  // Checked lock-free on the hot path so idle logging has zero lock contention
  // and zero heap allocation.
  std::atomic<uint32_t> active_consumers_{0};

  // Saved once on InstallHooks() so chaining never recurses and remains
  // lock-free in the global log handlers.
  std::atomic<logging::LogMessageHandlerFunction> previous_base_handler_{
      nullptr};
  std::atomic<starboard::LogMessageHandlerFunction> previous_starboard_handler_{
      nullptr};
  std::atomic<bool> hooks_installed_{false};

  mutable base::Lock lock_;
  std::deque<LogEntry> entries_ GUARDED_BY(lock_);
  size_t buffered_bytes_ GUARDED_BY(lock_) = 0;
  uint64_t next_seq_ GUARDED_BY(lock_) = 1;
  uint64_t dropped_count_ GUARDED_BY(lock_) = 0;

  scoped_refptr<base::SequencedTaskRunner> notify_task_runner_
      GUARDED_BY(lock_);
  base::RepeatingClosure notify_callback_ GUARDED_BY(lock_);
  bool notify_pending_ GUARDED_BY(lock_) = false;
};

}  // namespace cobalt

#endif  // COBALT_BROWSER_LOGGING_COBALT_LOG_COLLECTOR_H_
