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

#include "cobalt/browser/devtools/cobalt_devtools_manager_delegate.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "cobalt/browser/switches.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_agent_host_client_channel.h"
#include "third_party/inspector_protocol/crdtp/dispatch.h"
#include "third_party/inspector_protocol/crdtp/json.h"
#include "third_party/inspector_protocol/crdtp/span.h"

namespace cobalt {

namespace {

using LogSeverity = CobaltLogCollector::LogSeverity;

constexpr char kMethodLogEnable[] = "Log.enable";
constexpr char kMethodLogDisable[] = "Log.disable";
constexpr char kMethodLogEntryAdded[] = "Log.entryAdded";
constexpr char kMethodCobaltSetNativeLogging[] = "Cobalt.setNativeLogging";
constexpr char kMethodCobaltGetNativeLoggingState[] =
    "Cobalt.getNativeLoggingState";

const char* SeverityToString(LogSeverity severity) {
  switch (severity) {
    case LogSeverity::kVerbose:
      return "verbose";
    case LogSeverity::kInfo:
      return "info";
    case LogSeverity::kWarning:
      return "warning";
    case LogSeverity::kError:
      return "error";
    case LogSeverity::kFatal:
      return "fatal";
  }
}

const char* SeverityToCdpLevel(LogSeverity severity) {
  switch (severity) {
    case LogSeverity::kVerbose:
      return "verbose";
    case LogSeverity::kInfo:
      return "info";
    case LogSeverity::kWarning:
      return "warning";
    case LogSeverity::kError:
    case LogSeverity::kFatal:
      return "error";
  }
}

std::optional<LogSeverity> ParseSeverity(std::string_view value) {
  const std::string lower = base::ToLowerASCII(value);
  if (lower == "verbose") {
    return LogSeverity::kVerbose;
  }
  if (lower == "info") {
    return LogSeverity::kInfo;
  }
  if (lower == "warning") {
    return LogSeverity::kWarning;
  }
  if (lower == "error") {
    return LogSeverity::kError;
  }
  if (lower == "fatal") {
    return LogSeverity::kFatal;
  }
  return std::nullopt;
}

std::string SanitizeUtf8(std::string value) {
  if (base::IsStringUTF8AllowingNoncharacters(value)) {
    return value;
  }
  return base::UTF16ToUTF8(base::UTF8ToUTF16(value));
}

std::string FormatLogEntryText(const CobaltLogCollector::LogEntry& entry) {
  std::string text = "[Cobalt]";
  if (entry.severity == LogSeverity::kFatal) {
    text += " [FATAL]";
  }
  if (!entry.thread_name.empty()) {
    text += " [";
    text += entry.thread_name;
    text += "]";
  }
  text += " ";
  text += entry.message;
  if (entry.message_truncated) {
    text += " [truncated]";
  }
  return SanitizeUtf8(std::move(text));
}

bool EncodeDictToCbor(const base::Value::Dict& dict,
                      std::vector<uint8_t>* out_cbor) {
  std::optional<std::string> json = base::WriteJsonWithOptions(
      dict, base::OPTIONS_OMIT_DOUBLE_TYPE_PRESERVATION);
  if (!json.has_value()) {
    return false;
  }
  return crdtp::json::ConvertJSONToCBOR(crdtp::SpanFrom(*json), out_cbor).ok();
}

bool HasEnableDevToolsNativeLogsSwitch() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  return command_line &&
         command_line->HasSwitch(switches::kEnableDevToolsNativeLogs);
}

CobaltLogCollector::ScopedConsumer& GetBootLogConsumer() {
  static base::NoDestructor<CobaltLogCollector::ScopedConsumer> consumer;
  return *consumer;
}

base::WeakPtr<CobaltDevToolsManagerDelegate>& GetActiveDelegate() {
  static base::NoDestructor<base::WeakPtr<CobaltDevToolsManagerDelegate>>
      delegate;
  return *delegate;
}

bool IsNativeLogSink(content::DevToolsAgentHostClientChannel* channel) {
  content::DevToolsAgentHost* host =
      channel ? channel->GetAgentHost() : nullptr;
  if (!host) {
    return true;
  }
  const std::string type = host->GetType();
  return type == content::DevToolsAgentHost::kTypePage ||
         type == content::DevToolsAgentHost::kTypeBrowser;
}

}  // namespace

// static
void CobaltDevToolsManagerDelegate::InitializeNativeLogging() {
  CobaltLogCollector* collector = CobaltLogCollector::GetInstance();
  collector->InstallHooks();
  if (HasEnableDevToolsNativeLogsSwitch()) {
    GetBootLogConsumer() = collector->RegisterConsumer();
  }
}

// static
void CobaltDevToolsManagerDelegate::ShutdownNativeLogging() {
  if (base::WeakPtr<CobaltDevToolsManagerDelegate> delegate =
          GetActiveDelegate()) {
    delegate->SetNativeLoggingEnabled(/*enabled=*/false,
                                      /*replay_backlog=*/false);
  }
  GetActiveDelegate().reset();
  CobaltLogCollector::GetInstance()->SetNotifyCallback(
      nullptr, base::RepeatingClosure());
  GetBootLogConsumer().Reset();
}

CobaltDevToolsManagerDelegate::CobaltDevToolsManagerDelegate(
    content::BrowserContext* browser_context)
    : CobaltDevToolsManagerDelegate(browser_context,
                                    CobaltLogCollector::GetInstance(),
                                    HasEnableDevToolsNativeLogsSwitch()) {}

CobaltDevToolsManagerDelegate::CobaltDevToolsManagerDelegate(
    content::BrowserContext* browser_context,
    CobaltLogCollector* collector,
    bool enable_native_logging_at_startup)
    : content::ShellDevToolsManagerDelegate(browser_context),
      collector_(collector),
      keep_enabled_when_detached_(enable_native_logging_at_startup) {
  if (collector_ == CobaltLogCollector::GetInstance()) {
    GetActiveDelegate() = weak_ptr_factory_.GetWeakPtr();
  }
  if (enable_native_logging_at_startup) {
    SetNativeLoggingEnabled(/*enabled=*/true, /*replay_backlog=*/true);
    if (collector_ == CobaltLogCollector::GetInstance()) {
      // Hand off ownership from the early-boot consumer so an explicit
      // Cobalt.setNativeLogging({enabled: false}) can release the ring buffer.
      GetBootLogConsumer().Reset();
    }
  }
}

CobaltDevToolsManagerDelegate::~CobaltDevToolsManagerDelegate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (collector_ == CobaltLogCollector::GetInstance() &&
      GetActiveDelegate().get() == this) {
    GetActiveDelegate().reset();
  }
  if (collector_) {
    collector_->SetNotifyCallback(nullptr, base::RepeatingClosure());
  }
  notify_callback_registered_ = false;
  collector_consumer_.Reset();
}

void CobaltDevToolsManagerDelegate::ClientAttached(
    content::DevToolsAgentHostClientChannel* channel) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  content::ShellDevToolsManagerDelegate::ClientAttached(channel);
  SessionState state;
  state.native_log_sink = IsNativeLogSink(channel);
  state.last_sent_seq = default_session_start_seq_;
  sessions_.insert_or_assign(channel, state);
  if (native_logging_enabled_) {
    EnsureNotifyCallbackRegistered();
  }
}

void CobaltDevToolsManagerDelegate::ClientDetached(
    content::DevToolsAgentHostClientChannel* channel) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sessions_.erase(channel);
  content::ShellDevToolsManagerDelegate::ClientDetached(channel);
  if (sessions_.empty() && native_logging_enabled_ &&
      !keep_enabled_when_detached_) {
    SetNativeLoggingEnabled(/*enabled=*/false, /*replay_backlog=*/false);
  }
}

void CobaltDevToolsManagerDelegate::HandleCommand(
    content::DevToolsAgentHostClientChannel* channel,
    base::span<const uint8_t> message,
    NotHandledCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  crdtp::Dispatchable dispatchable(crdtp::SpanFrom(message));
  if (!dispatchable.ok()) {
    std::move(callback).Run(message);
    return;
  }

  const crdtp::span<uint8_t> method = dispatchable.Method();
  if (crdtp::SpanEquals(method, crdtp::SpanFrom(kMethodLogEnable))) {
    auto it = sessions_.find(channel);
    if (it != sessions_.end() && it->second.native_log_sink) {
      it->second.log_domain_enabled = true;
      if (native_logging_enabled_) {
        EnsureNotifyCallbackRegistered();
        ScheduleFlush();
      }
    }
    std::move(callback).Run(message);
    return;
  }

  if (crdtp::SpanEquals(method, crdtp::SpanFrom(kMethodLogDisable))) {
    auto it = sessions_.find(channel);
    if (it != sessions_.end()) {
      it->second.log_domain_enabled = false;
    }
    std::move(callback).Run(message);
    return;
  }

  if (crdtp::SpanEquals(method,
                        crdtp::SpanFrom(kMethodCobaltSetNativeLogging))) {
    HandleSetNativeLogging(channel, dispatchable);
    return;
  }

  if (crdtp::SpanEquals(method,
                        crdtp::SpanFrom(kMethodCobaltGetNativeLoggingState))) {
    HandleGetNativeLoggingState(channel, dispatchable);
    return;
  }

  std::move(callback).Run(message);
}

void CobaltDevToolsManagerDelegate::HandleSetNativeLogging(
    content::DevToolsAgentHostClientChannel* channel,
    const crdtp::Dispatchable& dispatchable) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (dispatchable.Params().empty()) {
    SendErrorResponse(channel, dispatchable.CallId(),
                      crdtp::DispatchResponse::InvalidParams(
                          "Missing required 'enabled' parameter"));
    return;
  }

  std::string params_json;
  crdtp::Status decode_status =
      crdtp::json::ConvertCBORToJSON(dispatchable.Params(), &params_json);
  if (!decode_status.ok()) {
    SendErrorResponse(
        channel, dispatchable.CallId(),
        crdtp::DispatchResponse::InvalidParams("Failed to decode params"));
    return;
  }

  std::optional<base::Value::Dict> params =
      base::JSONReader::ReadDict(params_json);
  if (!params.has_value()) {
    SendErrorResponse(
        channel, dispatchable.CallId(),
        crdtp::DispatchResponse::InvalidParams("Params must be a JSON object"));
    return;
  }

  std::optional<bool> enabled = params->FindBool("enabled");
  if (!enabled.has_value()) {
    SendErrorResponse(channel, dispatchable.CallId(),
                      crdtp::DispatchResponse::InvalidParams(
                          "Missing or invalid boolean 'enabled' parameter"));
    return;
  }

  LogSeverity requested_severity = min_severity_;
  if (const base::Value* severity_value = params->Find("minSeverity")) {
    if (!severity_value->is_string()) {
      SendErrorResponse(
          channel, dispatchable.CallId(),
          crdtp::DispatchResponse::InvalidParams(
              "'minSeverity' must be one of verbose, info, warning, error, "
              "fatal"));
      return;
    }
    std::optional<LogSeverity> parsed =
        ParseSeverity(severity_value->GetString());
    if (!parsed.has_value()) {
      SendErrorResponse(
          channel, dispatchable.CallId(),
          crdtp::DispatchResponse::InvalidParams(
              "Invalid 'minSeverity': expected verbose, info, warning, error, "
              "or fatal"));
      return;
    }
    requested_severity = *parsed;
  }

  bool replay_backlog = false;
  if (const base::Value* replay_value = params->Find("replayBacklog")) {
    if (!replay_value->is_bool()) {
      SendErrorResponse(channel, dispatchable.CallId(),
                        crdtp::DispatchResponse::InvalidParams(
                            "'replayBacklog' must be a boolean"));
      return;
    }
    replay_backlog = replay_value->GetBool();
  }

  min_severity_ = requested_severity;
  SetNativeLoggingEnabled(*enabled, replay_backlog);

  CobaltLogCollector::Stats stats;
  if (collector_) {
    stats = collector_->SnapshotStats();
  }

  base::Value::Dict result;
  result.Set("enabled", native_logging_enabled_);
  result.Set("minSeverity", SeverityToString(min_severity_));
  result.Set("droppedCount", static_cast<double>(stats.dropped_count));
  result.Set("latestSeq", static_cast<double>(stats.latest_seq));
  SendJsonResponse(channel, dispatchable.CallId(), std::move(result));
}

void CobaltDevToolsManagerDelegate::HandleGetNativeLoggingState(
    content::DevToolsAgentHostClientChannel* channel,
    const crdtp::Dispatchable& dispatchable) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CobaltLogCollector::Stats stats;
  if (collector_) {
    stats = collector_->SnapshotStats();
  }

  base::Value::Dict result;
  result.Set("enabled", native_logging_enabled_);
  result.Set("minSeverity", SeverityToString(min_severity_));
  result.Set("bufferedEntries", static_cast<double>(stats.buffered_entries));
  result.Set("bufferedBytes", static_cast<double>(stats.buffered_bytes));
  result.Set("droppedCount", static_cast<double>(stats.dropped_count));
  result.Set("latestSeq", static_cast<double>(stats.latest_seq));
  result.Set("logEnabledSessions",
             static_cast<double>(CountLogEnabledSessions()));
  SendJsonResponse(channel, dispatchable.CallId(), std::move(result));
}

void CobaltDevToolsManagerDelegate::SetNativeLoggingEnabled(
    bool enabled,
    bool replay_backlog) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!collector_) {
    native_logging_enabled_ = false;
    return;
  }

  if (!enabled) {
    native_logging_enabled_ = false;
    flush_scheduled_ = false;
    for (auto& [channel, state] : sessions_) {
      state.delivered_any = false;
    }
    collector_->SetNotifyCallback(nullptr, base::RepeatingClosure());
    notify_callback_registered_ = false;
    collector_consumer_.Reset();
    return;
  }

  const bool was_enabled = native_logging_enabled_;
  if (!collector_consumer_) {
    collector_consumer_ = collector_->RegisterConsumer();
  }
  native_logging_enabled_ = true;
  EnsureNotifyCallbackRegistered();

  if (replay_backlog) {
    default_session_start_seq_ = 0;
    for (auto& [channel, state] : sessions_) {
      if (!state.delivered_any) {
        state.last_sent_seq = 0;
      }
    }
    ScheduleFlush();
  } else if (!was_enabled) {
    const uint64_t latest_seq = collector_->SnapshotStats().latest_seq;
    default_session_start_seq_ = latest_seq;
    for (auto& [channel, state] : sessions_) {
      state.last_sent_seq = latest_seq;
    }
  }
}

void CobaltDevToolsManagerDelegate::EnsureNotifyCallbackRegistered() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (notify_callback_registered_ || !collector_ ||
      !base::SequencedTaskRunner::HasCurrentDefault()) {
    return;
  }
  collector_->SetNotifyCallback(
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindRepeating(&CobaltDevToolsManagerDelegate::OnCollectorHasLogs,
                          weak_ptr_factory_.GetWeakPtr()));
  notify_callback_registered_ = true;
}

void CobaltDevToolsManagerDelegate::OnCollectorHasLogs() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!native_logging_enabled_ || flush_scheduled_) {
    return;
  }
  FlushPendingLogs();
}

void CobaltDevToolsManagerDelegate::ScheduleFlush() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (flush_scheduled_ || !native_logging_enabled_ ||
      !base::SequencedTaskRunner::HasCurrentDefault()) {
    return;
  }
  flush_scheduled_ = true;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&CobaltDevToolsManagerDelegate::FlushPendingLogs,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CobaltDevToolsManagerDelegate::FlushPendingLogs() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  flush_scheduled_ = false;
  if (!native_logging_enabled_ || !collector_) {
    return;
  }

  std::vector<content::DevToolsAgentHostClientChannel*> target_channels;
  target_channels.reserve(sessions_.size());
  for (const auto& [channel, state] : sessions_) {
    if (state.native_log_sink && state.log_domain_enabled) {
      target_channels.push_back(channel);
    }
  }
  if (target_channels.empty()) {
    return;
  }

  bool any_truncated = false;
  for (content::DevToolsAgentHostClientChannel* channel : target_channels) {
    if (!native_logging_enabled_) {
      break;
    }
    auto it = sessions_.find(channel);
    if (it == sessions_.end() || !it->second.native_log_sink ||
        !it->second.log_domain_enabled) {
      continue;
    }

    CobaltLogCollector::ReadResult batch =
        collector_->ReadSince(it->second.last_sent_seq, kMaxEntriesPerFlush,
                              kMaxBytesPerFlush, min_severity_);
    it->second.last_sent_seq = batch.next_seq;
    if (batch.truncated) {
      any_truncated = true;
    }

    for (const CobaltLogCollector::LogEntry& entry : batch.entries) {
      // Page console.* calls are already delivered natively to DevTools
      // Console and mirrored into base::logging with file "CONSOLE" by
      // content::LogConsoleMessage(). Skip them here to prevent duplicates
      // while keeping CobaltLogCollector general for non-DevTools consumers.
      if (entry.file == "CONSOLE") {
        continue;
      }
      if (!SendLogEntry(channel, entry)) {
        continue;
      }
      // Re-check session presence in case dispatching detached the client.
      it = sessions_.find(channel);
      if (it == sessions_.end() || !it->second.native_log_sink ||
          !it->second.log_domain_enabled || !native_logging_enabled_) {
        break;
      }
      it->second.delivered_any = true;
    }
  }

  if (any_truncated && native_logging_enabled_) {
    ScheduleFlush();
  }
}

void CobaltDevToolsManagerDelegate::SendJsonResponse(
    content::DevToolsAgentHostClientChannel* channel,
    int call_id,
    base::Value::Dict result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::Value::Dict envelope;
  envelope.Set("id", call_id);
  envelope.Set("result", std::move(result));

  std::vector<uint8_t> cbor;
  if (EncodeDictToCbor(envelope, &cbor)) {
    channel->DispatchProtocolMessageToClient(std::move(cbor));
  }
}

void CobaltDevToolsManagerDelegate::SendErrorResponse(
    content::DevToolsAgentHostClientChannel* channel,
    int call_id,
    crdtp::DispatchResponse error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto response = crdtp::CreateErrorResponse(call_id, std::move(error));
  channel->DispatchProtocolMessageToClient(response->Serialize());
}

bool CobaltDevToolsManagerDelegate::SendLogEntry(
    content::DevToolsAgentHostClientChannel* channel,
    const CobaltLogCollector::LogEntry& entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::Value::Dict log_entry;
  log_entry.Set("source", "other");
  log_entry.Set("level", SeverityToCdpLevel(entry.severity));
  log_entry.Set("text", FormatLogEntryText(entry));
  log_entry.Set("timestamp", entry.timestamp_ms);
  if (!entry.file.empty()) {
    log_entry.Set("url", SanitizeUtf8(entry.file));
    if (entry.line > 0) {
      // CDP Log.LogEntry lineNumber is 0-based; DevTools displays +1.
      log_entry.Set("lineNumber", entry.line - 1);
    }
  }

  base::Value::Dict params;
  params.Set("entry", std::move(log_entry));

  base::Value::Dict notification;
  notification.Set("method", kMethodLogEntryAdded);
  notification.Set("params", std::move(params));

  std::vector<uint8_t> cbor;
  if (!EncodeDictToCbor(notification, &cbor)) {
    return false;
  }
  channel->DispatchProtocolMessageToClient(std::move(cbor));
  return true;
}

size_t CobaltDevToolsManagerDelegate::CountLogEnabledSessions() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  size_t count = 0;
  for (const auto& [channel, state] : sessions_) {
    if (state.native_log_sink && state.log_domain_enabled) {
      ++count;
    }
  }
  return count;
}

}  // namespace cobalt
