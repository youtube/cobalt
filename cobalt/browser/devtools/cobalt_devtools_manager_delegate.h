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

#ifndef COBALT_BROWSER_DEVTOOLS_COBALT_DEVTOOLS_MANAGER_DELEGATE_H_
#define COBALT_BROWSER_DEVTOOLS_COBALT_DEVTOOLS_MANAGER_DELEGATE_H_

#include <stddef.h>
#include <stdint.h>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/values.h"
#include "cobalt/browser/logging/cobalt_log_collector.h"
#include "cobalt/shell/browser/shell_devtools_manager_delegate.h"

namespace crdtp {
class Dispatchable;
class DispatchResponse;
}  // namespace crdtp

namespace cobalt {

// Cobalt-specific DevToolsManagerDelegate that bridges CobaltLogCollector into
// stock Chrome DevTools Console sessions via standard Log.entryAdded events and
// handles Cobalt.setNativeLogging / Cobalt.getNativeLoggingState CDP commands.
//
// Owned by content::DevToolsManager (a process-wide base::Singleton): lazily
// constructed on the browser UI thread when DevTools initializes and destroyed
// at process exit (not tied to BrowserContext lifetime). All methods are
// invoked on the browser UI sequence; |collector_| must outlive the delegate
// (the process-wide base::NoDestructor singleton in production).
class CobaltDevToolsManagerDelegate
    : public content::ShellDevToolsManagerDelegate {
 public:
  static constexpr size_t kMaxEntriesPerFlush = 128;
  static constexpr size_t kMaxBytesPerFlush = 128 * 1024;

  // Installs process-wide CobaltLogCollector hooks and, when
  // --enable-devtools-native-logs is set, starts buffering boot logs.
  static void InitializeNativeLogging();

  // Clears any active DevTools notify callback and releases the boot log
  // consumer during browser shutdown.
  static void ShutdownNativeLogging();

  explicit CobaltDevToolsManagerDelegate(
      content::BrowserContext* browser_context);
  CobaltDevToolsManagerDelegate(content::BrowserContext* browser_context,
                                CobaltLogCollector* collector,
                                bool enable_native_logging_at_startup);

  CobaltDevToolsManagerDelegate(const CobaltDevToolsManagerDelegate&) = delete;
  CobaltDevToolsManagerDelegate& operator=(
      const CobaltDevToolsManagerDelegate&) = delete;

  ~CobaltDevToolsManagerDelegate() override;

  // content::DevToolsManagerDelegate overrides.
  void ClientAttached(
      content::DevToolsAgentHostClientChannel* channel) override;
  void ClientDetached(
      content::DevToolsAgentHostClientChannel* channel) override;
  void HandleCommand(content::DevToolsAgentHostClientChannel* channel,
                     base::span<const uint8_t> message,
                     NotHandledCallback callback) override;

 private:
  struct SessionState {
    bool native_log_sink = true;
    bool log_domain_enabled = false;
    bool delivered_any = false;
    uint64_t last_sent_seq = 0;
  };

  void HandleSetNativeLogging(content::DevToolsAgentHostClientChannel* channel,
                              const crdtp::Dispatchable& dispatchable);
  void HandleGetNativeLoggingState(
      content::DevToolsAgentHostClientChannel* channel,
      const crdtp::Dispatchable& dispatchable);

  void SetNativeLoggingEnabled(bool enabled, bool replay_backlog);
  void EnsureNotifyCallbackRegistered();
  void OnCollectorHasLogs();
  void ScheduleFlush();
  void FlushPendingLogs();

  void SendJsonResponse(content::DevToolsAgentHostClientChannel* channel,
                        int call_id,
                        base::Value::Dict result);
  void SendErrorResponse(content::DevToolsAgentHostClientChannel* channel,
                         int call_id,
                         crdtp::DispatchResponse error);
  bool SendLogEntry(content::DevToolsAgentHostClientChannel* channel,
                    const CobaltLogCollector::LogEntry& entry);
  size_t CountLogEnabledSessions() const;

  raw_ptr<CobaltLogCollector> collector_;
  const bool keep_enabled_when_detached_;
  bool native_logging_enabled_ = false;
  CobaltLogCollector::LogSeverity min_severity_ =
      CobaltLogCollector::LogSeverity::kInfo;
  uint64_t default_session_start_seq_ = 0;
  bool notify_callback_registered_ = false;
  bool flush_scheduled_ = false;
  CobaltLogCollector::ScopedConsumer collector_consumer_;
  base::flat_map<raw_ptr<content::DevToolsAgentHostClientChannel>, SessionState>
      sessions_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<CobaltDevToolsManagerDelegate> weak_ptr_factory_{this};
};

}  // namespace cobalt

#endif  // COBALT_BROWSER_DEVTOOLS_COBALT_DEVTOOLS_MANAGER_DELEGATE_H_
