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

#include <algorithm>
#include <utility>

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/no_destructor.h"
#include "base/threading/platform_thread.h"

namespace cobalt {
namespace {

// Guards against recursive logging from inside the collector or chained
// handlers on the same thread.
thread_local bool g_in_cobalt_log_collector = false;

std::string ExtractBasename(const char* file) {
  std::string_view path = file ? std::string_view(file) : std::string_view();
  if (path.empty()) {
    return std::string();
  }
  size_t last_sep = path.find_last_of("/\\");
  if (last_sep != std::string_view::npos) {
    path.remove_prefix(last_sep + 1);
  }
  return std::string(path);
}

std::string_view StripTrailingNewlines(std::string_view text) {
  while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) {
    text.remove_suffix(1);
  }
  return text;
}

// Truncates |text| to at most |max_bytes| while keeping the resulting UTF-8
// prefix well-formed by trimming any incomplete trailing multi-byte sequence.
std::string TruncateUtf8(std::string_view text,
                         size_t max_bytes,
                         bool* truncated) {
  if (text.size() <= max_bytes) {
    *truncated = false;
    return std::string(text);
  }
  *truncated = true;
  if (max_bytes == 0) {
    return std::string();
  }

  size_t end = max_bytes;
  // Back up over UTF-8 continuation bytes (10xxxxxx) if the cut landed inside
  // a multi-byte sequence.
  size_t lead = end;
  while (lead > 0 &&
         (static_cast<unsigned char>(text[lead - 1]) & 0xC0) == 0x80) {
    --lead;
  }
  if (lead > 0) {
    unsigned char lead_byte = static_cast<unsigned char>(text[lead - 1]);
    size_t expected_len = 1;
    if ((lead_byte & 0xE0) == 0xC0) {
      expected_len = 2;
    } else if ((lead_byte & 0xF0) == 0xE0) {
      expected_len = 3;
    } else if ((lead_byte & 0xF8) == 0xF0) {
      expected_len = 4;
    }
    if ((end - (lead - 1)) < expected_len) {
      end = lead - 1;
    }
  }
  return std::string(text.substr(0, end));
}

size_t EstimateEntryBytes(const CobaltLogCollector::LogEntry& entry) {
  return sizeof(CobaltLogCollector::LogEntry) + entry.file.size() +
         entry.thread_name.size() + entry.message.size();
}

}  // namespace

CobaltLogCollector::ScopedConsumer::ScopedConsumer(
    CobaltLogCollector* collector)
    : collector_(collector) {}

CobaltLogCollector::ScopedConsumer::ScopedConsumer(
    ScopedConsumer&& other) noexcept
    : collector_(std::exchange(other.collector_, nullptr)) {}

CobaltLogCollector::ScopedConsumer&
CobaltLogCollector::ScopedConsumer::operator=(ScopedConsumer&& other) noexcept {
  if (this != &other) {
    Reset();
    collector_ = std::exchange(other.collector_, nullptr);
  }
  return *this;
}

CobaltLogCollector::ScopedConsumer::~ScopedConsumer() {
  Reset();
}

void CobaltLogCollector::ScopedConsumer::Reset() {
  if (collector_) {
    CobaltLogCollector* collector = std::exchange(collector_, nullptr);
    collector->UnregisterConsumer();
  }
}

// static
CobaltLogCollector* CobaltLogCollector::GetInstance() {
  static base::NoDestructor<CobaltLogCollector> instance;
  return instance.get();
}

// static
CobaltLogCollector::LogSeverity CobaltLogCollector::SeverityFromBaseLogLevel(
    int base_severity) {
  if (base_severity < logging::LOGGING_INFO) {
    return LogSeverity::kVerbose;
  }
  if (base_severity == logging::LOGGING_INFO) {
    return LogSeverity::kInfo;
  }
  if (base_severity == logging::LOGGING_WARNING) {
    return LogSeverity::kWarning;
  }
  if (base_severity == logging::LOGGING_ERROR) {
    return LogSeverity::kError;
  }
  return LogSeverity::kFatal;
}

// static
CobaltLogCollector::LogSeverity
CobaltLogCollector::SeverityFromStarboardLogPriority(SbLogPriority priority) {
  switch (priority) {
    case kSbLogPriorityWarning:
      return LogSeverity::kWarning;
    case kSbLogPriorityError:
      return LogSeverity::kError;
    case kSbLogPriorityFatal:
      return LogSeverity::kFatal;
    case kSbLogPriorityInfo:
    case kSbLogPriorityUnknown:
    default:
      return LogSeverity::kInfo;
  }
}

CobaltLogCollector::CobaltLogCollector() : CobaltLogCollector(Config()) {}

CobaltLogCollector::CobaltLogCollector(Config config)
    : config_(config), notify_state_(base::MakeRefCounted<NotifyState>()) {
  base::AutoLock notify_lock(notify_state_->lock);
  notify_state_->collector = this;
}

CobaltLogCollector::~CobaltLogCollector() {
  base::AutoLock notify_lock(notify_state_->lock);
  notify_state_->collector = nullptr;
}

void CobaltLogCollector::InstallHooks() {
  base::AutoLock lock(lock_);
  if (hooks_installed_.load(std::memory_order_relaxed)) {
    return;
  }
  logging::LogMessageHandlerFunction existing_base =
      logging::GetLogMessageHandler();
  if (existing_base != &CobaltLogCollector::BaseLogMessageHandler) {
    previous_base_handler_.store(existing_base, std::memory_order_release);
    logging::SetLogMessageHandler(&CobaltLogCollector::BaseLogMessageHandler);
  }
  starboard::LogMessageHandlerFunction existing_starboard =
      starboard::GetLogMessageHandler();
  if (existing_starboard != &CobaltLogCollector::StarboardLogMessageHandler) {
    previous_starboard_handler_.store(existing_starboard,
                                      std::memory_order_release);
    starboard::SetLogMessageHandler(
        &CobaltLogCollector::StarboardLogMessageHandler);
  }
  hooks_installed_.store(true, std::memory_order_release);
}

void CobaltLogCollector::UninstallHooksForTesting() {
  base::AutoLock lock(lock_);
  if (!hooks_installed_.load(std::memory_order_relaxed)) {
    return;
  }
  logging::LogMessageHandlerFunction previous_base =
      previous_base_handler_.exchange(nullptr, std::memory_order_acq_rel);
  if (logging::GetLogMessageHandler() ==
      &CobaltLogCollector::BaseLogMessageHandler) {
    logging::SetLogMessageHandler(previous_base);
  }
  starboard::LogMessageHandlerFunction previous_starboard =
      previous_starboard_handler_.exchange(nullptr, std::memory_order_acq_rel);
  if (starboard::GetLogMessageHandler() ==
      &CobaltLogCollector::StarboardLogMessageHandler) {
    starboard::SetLogMessageHandler(previous_starboard);
  }
  hooks_installed_.store(false, std::memory_order_release);
}

CobaltLogCollector::ScopedConsumer CobaltLogCollector::RegisterConsumer() {
  base::AutoLock lock(lock_);
  active_consumers_.fetch_add(1, std::memory_order_acq_rel);
  return ScopedConsumer(this);
}

void CobaltLogCollector::UnregisterConsumer() {
  base::AutoLock lock(lock_);
  uint32_t previous = active_consumers_.fetch_sub(1, std::memory_order_acq_rel);
  if (previous <= 1) {
    // Release ring memory immediately once the last consumer stops.
    std::deque<LogEntry>().swap(entries_);
    buffered_bytes_ = 0;
    notify_pending_ = false;
  }
}

void CobaltLogCollector::SetNotifyCallback(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::RepeatingClosure callback) {
  base::AutoLock lock(lock_);
  if (!task_runner || !callback) {
    notify_task_runner_.reset();
    notify_callback_.Reset();
    notify_pending_ = false;
    return;
  }
  notify_task_runner_ = std::move(task_runner);
  notify_callback_ = std::move(callback);
  notify_pending_ = false;
}

void CobaltLogCollector::RecordLogMessage(LogSeverity severity,
                                          const char* file,
                                          int line,
                                          std::string_view message) {
  if (active_consumers_.load(std::memory_order_relaxed) == 0) {
    return;
  }
  if (g_in_cobalt_log_collector) {
    return;
  }
  base::AutoReset<bool> reentrancy_guard(&g_in_cobalt_log_collector, true);
  RecordLogMessageInternal(severity, file, line, message);
}

void CobaltLogCollector::RecordLogMessageInternal(LogSeverity severity,
                                                  const char* file,
                                                  int line,
                                                  std::string_view message) {
  LogEntry entry;
  entry.timestamp_ms = base::Time::Now().InMillisecondsFSinceUnixEpoch();
  entry.severity = severity;
  entry.file = ExtractBasename(file);
  entry.line = line;
  const char* thread_name = base::PlatformThread::GetName();
  std::string_view thread_view =
      thread_name ? std::string_view(thread_name) : std::string_view();
  if (!thread_view.empty()) {
    entry.thread_name = std::string(thread_view);
  }
  entry.message =
      TruncateUtf8(StripTrailingNewlines(message), config_.max_message_bytes,
                   &entry.message_truncated);
  const size_t entry_bytes = EstimateEntryBytes(entry);

  scoped_refptr<base::SequencedTaskRunner> task_runner_to_notify;
  {
    base::AutoLock lock(lock_);
    // Re-check under lock so entries are never allocated after the final
    // consumer unregisters.
    if (active_consumers_.load(std::memory_order_relaxed) == 0 ||
        config_.max_entries == 0) {
      return;
    }

    entry.seq = next_seq_++;
    while (!entries_.empty() &&
           (entries_.size() >= config_.max_entries ||
            buffered_bytes_ + entry_bytes > config_.max_bytes)) {
      buffered_bytes_ -= EstimateEntryBytes(entries_.front());
      entries_.pop_front();
      ++dropped_count_;
    }

    entries_.push_back(std::move(entry));
    buffered_bytes_ += entry_bytes;

    if (!notify_pending_ && notify_task_runner_ && notify_callback_) {
      notify_pending_ = true;
      task_runner_to_notify = notify_task_runner_;
    }
  }

  if (task_runner_to_notify) {
    task_runner_to_notify->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&CobaltLogCollector::DispatchNotification,
                       notify_state_),
        config_.notify_delay);
  }
}

CobaltLogCollector::ReadResult CobaltLogCollector::ReadSince(
    uint64_t after_seq,
    size_t max_entries,
    size_t max_bytes,
    LogSeverity min_severity) const {
  ReadResult result;
  base::AutoLock lock(lock_);
  result.dropped_count = dropped_count_;
  result.next_seq = after_seq;

  if (entries_.empty()) {
    const uint64_t latest_seq = next_seq_ > 1 ? next_seq_ - 1 : 0;
    result.next_seq = std::max(after_seq, latest_seq);
    return result;
  }

  size_t accumulated_bytes = 0;
  for (const LogEntry& entry : entries_) {
    if (entry.seq <= after_seq) {
      continue;
    }
    if (static_cast<int>(entry.severity) < static_cast<int>(min_severity)) {
      result.next_seq = entry.seq;
      continue;
    }
    const size_t entry_bytes = EstimateEntryBytes(entry);
    if ((max_entries > 0 && result.entries.size() >= max_entries) ||
        (max_bytes > 0 && !result.entries.empty() &&
         accumulated_bytes + entry_bytes > max_bytes)) {
      result.truncated = true;
      break;
    }
    result.entries.push_back(entry);
    accumulated_bytes += entry_bytes;
    result.next_seq = entry.seq;
  }

  return result;
}

CobaltLogCollector::Stats CobaltLogCollector::SnapshotStats() const {
  base::AutoLock lock(lock_);
  Stats stats;
  stats.active = active_consumers_.load(std::memory_order_relaxed) > 0;
  stats.buffered_entries = entries_.size();
  stats.buffered_bytes = buffered_bytes_;
  stats.dropped_count = dropped_count_;
  stats.latest_seq = next_seq_ > 1 ? next_seq_ - 1 : 0;
  return stats;
}

// static
bool CobaltLogCollector::BaseLogMessageHandler(int severity,
                                               const char* file,
                                               int line,
                                               size_t message_start,
                                               const std::string& str) {
  if (g_in_cobalt_log_collector) {
    return false;
  }
  base::AutoReset<bool> reentrancy_guard(&g_in_cobalt_log_collector, true);

  CobaltLogCollector* collector = GetInstance();
  if (collector->active_consumers_.load(std::memory_order_relaxed) > 0) {
    std::string_view message(str);
    if (message_start <= message.size()) {
      message.remove_prefix(message_start);
    }
    collector->RecordLogMessageInternal(SeverityFromBaseLogLevel(severity),
                                        file, line, message);
  }

  logging::LogMessageHandlerFunction previous =
      collector->previous_base_handler_.load(std::memory_order_acquire);
  if (previous) {
    return previous(severity, file, line, message_start, str);
  }
  return false;
}

// static
bool CobaltLogCollector::StarboardLogMessageHandler(SbLogPriority priority,
                                                    const char* file,
                                                    int line,
                                                    size_t message_start,
                                                    const std::string& str) {
  if (g_in_cobalt_log_collector) {
    return false;
  }
  base::AutoReset<bool> reentrancy_guard(&g_in_cobalt_log_collector, true);

  CobaltLogCollector* collector = GetInstance();
  if (collector->active_consumers_.load(std::memory_order_relaxed) > 0) {
    std::string_view message(str);
    if (message_start <= message.size()) {
      message.remove_prefix(message_start);
    }
    collector->RecordLogMessageInternal(
        SeverityFromStarboardLogPriority(priority), file, line, message);
  }

  starboard::LogMessageHandlerFunction previous =
      collector->previous_starboard_handler_.load(std::memory_order_acquire);
  if (previous) {
    return previous(priority, file, line, message_start, str);
  }
  return false;
}

// static
void CobaltLogCollector::DispatchNotification(
    scoped_refptr<NotifyState> notify_state) {
  base::RepeatingClosure callback;
  {
    base::AutoLock notify_lock(notify_state->lock);
    if (!notify_state->collector) {
      return;
    }
    callback = notify_state->collector->TakePendingNotification();
  }
  if (callback) {
    callback.Run();
  }
}

base::RepeatingClosure CobaltLogCollector::TakePendingNotification() {
  base::AutoLock lock(lock_);
  notify_pending_ = false;
  return notify_callback_;
}

}  // namespace cobalt
