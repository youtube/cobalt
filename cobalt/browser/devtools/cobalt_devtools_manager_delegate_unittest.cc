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

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/scoped_command_line.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "cobalt/browser/logging/cobalt_log_collector.h"
#include "cobalt/browser/switches.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_agent_host_client.h"
#include "content/public/browser/devtools_agent_host_client_channel.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/inspector_protocol/crdtp/json.h"
#include "third_party/inspector_protocol/crdtp/span.h"
#include "url/gurl.h"

namespace cobalt {
namespace {

using LogSeverity = CobaltLogCollector::LogSeverity;

// Minimal DevToolsAgentHost stub returning a configurable target type so tests
// can verify top-level page/browser vs. child worker target filtering.
class FakeDevToolsAgentHost : public content::DevToolsAgentHost {
 public:
  explicit FakeDevToolsAgentHost(std::string type) : type_(std::move(type)) {}

  std::string CreateIOStreamFromData(
      scoped_refptr<base::RefCountedMemory>) override {
    return "";
  }
  bool AttachClient(content::DevToolsAgentHostClient* client) override {
    return false;
  }
  bool DetachClient(content::DevToolsAgentHostClient* client) override {
    return false;
  }
  bool IsAttached() override { return false; }
  void DispatchProtocolMessage(content::DevToolsAgentHostClient* client,
                               base::span<const uint8_t> message) override {}
  void InspectElement(content::RenderFrameHost* frame_host,
                      int x,
                      int y) override {}
  std::string GetId() override { return "fake-id"; }
  std::string GetParentId() override { return ""; }
  std::string GetOpenerId() override { return ""; }
  bool CanAccessOpener() override { return false; }
  std::string GetOpenerFrameId() override { return ""; }
  content::WebContents* GetWebContents() override { return nullptr; }
  content::BrowserContext* GetBrowserContext() override { return nullptr; }
  void DisconnectWebContents() override {}
  void ConnectWebContents(content::WebContents* web_contents) override {}
  std::string GetType() override { return type_; }
  std::string GetTitle() override { return ""; }
  std::string GetDescription() override { return ""; }
  GURL GetURL() override { return GURL(); }
  GURL GetFaviconURL() override { return GURL(); }
  std::string GetFrontendURL() override { return ""; }
  bool Activate() override { return false; }
  void Reload() override {}
  bool Close() override { return false; }
  base::TimeTicks GetLastActivityTime() override { return base::TimeTicks(); }
  void ForceDetachAllSessions() override {}
  content::RenderProcessHost* GetProcessHost() override { return nullptr; }

 private:
  ~FakeDevToolsAgentHost() override = default;

  std::string type_;
};

// No-op DevToolsAgentHostClient backing FakeDevToolsChannel in unit tests.
class FakeDevToolsAgentHostClient : public content::DevToolsAgentHostClient {
 public:
  void DispatchProtocolMessage(content::DevToolsAgentHost* agent_host,
                               base::span<const uint8_t> message) override {}
  void AgentHostClosed(content::DevToolsAgentHost* agent_host) override {}
};

// Captures CBOR-encoded CDP messages dispatched by the delegate as parsed JSON
// dictionaries and optionally runs a callback on dispatch (e.g., to simulate a
// client detaching mid-flush).
class FakeDevToolsChannel : public content::DevToolsAgentHostClientChannel {
 public:
  explicit FakeDevToolsChannel(
      scoped_refptr<content::DevToolsAgentHost> agent_host = nullptr)
      : agent_host_(std::move(agent_host)) {}
  ~FakeDevToolsChannel() override = default;

  void DispatchProtocolMessageToClient(std::vector<uint8_t> message) override {
    std::string json;
    ASSERT_TRUE(
        crdtp::json::ConvertCBORToJSON(crdtp::SpanFrom(message), &json).ok());
    std::optional<base::Value::Dict> parsed = base::JSONReader::ReadDict(json);
    ASSERT_TRUE(parsed.has_value()) << "Failed to parse JSON: " << json;
    received_messages_.push_back(std::move(*parsed));
    if (on_dispatch_) {
      on_dispatch_.Run(received_messages_.back());
    }
  }

  content::DevToolsAgentHost* GetAgentHost() override {
    return agent_host_.get();
  }
  content::DevToolsAgentHostClient* GetClient() override { return &client_; }

  void SetOnDispatch(
      base::RepeatingCallback<void(const base::Value::Dict&)> on_dispatch) {
    on_dispatch_ = std::move(on_dispatch);
  }

  const std::vector<base::Value::Dict>& received_messages() const {
    return received_messages_;
  }

  void ClearMessages() { received_messages_.clear(); }

 private:
  scoped_refptr<content::DevToolsAgentHost> agent_host_;
  FakeDevToolsAgentHostClient client_;
  std::vector<base::Value::Dict> received_messages_;
  base::RepeatingCallback<void(const base::Value::Dict&)> on_dispatch_;
};

std::vector<uint8_t> EncodeCommand(
    int id,
    const std::string& method,
    std::optional<base::Value::Dict> params = std::nullopt) {
  base::Value::Dict command;
  command.Set("id", id);
  command.Set("method", method);
  if (params.has_value()) {
    command.Set("params", std::move(*params));
  }
  std::optional<std::string> json = base::WriteJson(command);
  EXPECT_TRUE(json.has_value());
  std::vector<uint8_t> cbor;
  EXPECT_TRUE(
      crdtp::json::ConvertJSONToCBOR(crdtp::SpanFrom(*json), &cbor).ok());
  return cbor;
}

// Test fixture providing a mock-time TaskEnvironment and CBOR CDP command
// dispatch helpers for CobaltDevToolsManagerDelegate unit tests.
class CobaltDevToolsManagerDelegateTest : public ::testing::Test {
 protected:
  CobaltDevToolsManagerDelegateTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  CobaltLogCollector::Config FastConfig() {
    CobaltLogCollector::Config config;
    config.notify_delay = base::Milliseconds(10);
    return config;
  }

  bool SendCommand(CobaltDevToolsManagerDelegate* delegate,
                   FakeDevToolsChannel* channel,
                   int id,
                   const std::string& method,
                   std::optional<base::Value::Dict> params = std::nullopt) {
    std::vector<uint8_t> cbor = EncodeCommand(id, method, std::move(params));
    bool passed_through = false;
    delegate->HandleCommand(
        channel, cbor,
        base::BindOnce(
            [](bool* flag, base::span<const uint8_t>) { *flag = true; },
            &passed_through));
    return passed_through;
  }

  base::test::TaskEnvironment task_environment_;
};

// Covers the developer journey of opening stock Chrome DevTools Console:
// Log.enable and Log.disable are passively observed and always forwarded to
// Chromium's built-in handlers, and Cobalt.getNativeLoggingState reflects the
// active session count while keeping collection inactive by default.
TEST_F(CobaltDevToolsManagerDelegateTest,
       PassivelyTracksLogDomainAndReportsIdleDefaultState) {
  CobaltLogCollector collector(FastConfig());
  CobaltDevToolsManagerDelegate delegate(
      /*browser_context=*/nullptr, &collector,
      /*enable_native_logging_at_startup=*/false);
  FakeDevToolsChannel console_channel;
  delegate.ClientAttached(&console_channel);

  EXPECT_TRUE(SendCommand(&delegate, &console_channel, 1, "Log.enable"));
  EXPECT_FALSE(collector.SnapshotStats().active);

  EXPECT_FALSE(SendCommand(&delegate, &console_channel, 2,
                           "Cobalt.getNativeLoggingState"));
  ASSERT_EQ(console_channel.received_messages().size(), 1u);
  const base::Value::Dict* result =
      console_channel.received_messages()[0].FindDict("result");
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(result->FindBool("enabled"), std::optional<bool>(false));
  EXPECT_EQ(*result->FindString("minSeverity"), "info");
  EXPECT_EQ(result->FindDouble("logEnabledSessions"),
            std::optional<double>(1.0));

  EXPECT_TRUE(SendCommand(&delegate, &console_channel, 3, "Log.disable"));
  console_channel.ClearMessages();
  EXPECT_FALSE(SendCommand(&delegate, &console_channel, 4,
                           "Cobalt.getNativeLoggingState"));
  ASSERT_EQ(console_channel.received_messages().size(), 1u);
  result = console_channel.received_messages()[0].FindDict("result");
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(result->FindDouble("logEnabledSessions"),
            std::optional<double>(0.0));

  delegate.ClientDetached(&console_channel);
}

// Covers the developer journey of opening stock Chrome DevTools Console
// (Session A) alongside the unpacked Cobalt DevTools extension (Session B),
// clicking "Native Logs: On" in Session B, and seeing native Cobalt logs stream
// into Session A as standard Log.entryAdded console events with severity
// filtering, FATAL mapping, and 0-based line numbers.
TEST_F(CobaltDevToolsManagerDelegateTest,
       CrossSessionToggleFansOutFilteredLogEntriesToConsole) {
  CobaltLogCollector collector(FastConfig());
  CobaltDevToolsManagerDelegate delegate(
      /*browser_context=*/nullptr, &collector,
      /*enable_native_logging_at_startup=*/false);

  FakeDevToolsChannel console_channel;
  FakeDevToolsChannel extension_channel;
  delegate.ClientAttached(&console_channel);
  delegate.ClientAttached(&extension_channel);

  // Console enables the CDP Log domain; extension does not.
  EXPECT_TRUE(SendCommand(&delegate, &console_channel, 1, "Log.enable"));

  // Extension toggles native logging on at warning severity.
  base::Value::Dict params;
  params.Set("enabled", true);
  params.Set("minSeverity", "warning");
  params.Set("replayBacklog", false);
  EXPECT_FALSE(SendCommand(&delegate, &extension_channel, 10,
                           "Cobalt.setNativeLogging", std::move(params)));
  ASSERT_EQ(extension_channel.received_messages().size(), 1u);
  const base::Value::Dict* toggle_result =
      extension_channel.received_messages()[0].FindDict("result");
  ASSERT_NE(toggle_result, nullptr);
  EXPECT_EQ(toggle_result->FindBool("enabled"), std::optional<bool>(true));
  EXPECT_EQ(*toggle_result->FindString("minSeverity"), "warning");
  EXPECT_TRUE(collector.SnapshotStats().active);

  // Record INFO (filtered out), WARNING, and FATAL messages (advancing the
  // mock clock by 500 ms between WARNING and FATAL to verify millisecond
  // units).
  const double expected_warn_ms =
      base::Time::Now().InMillisecondsFSinceUnixEpoch();
  collector.RecordLogMessage(LogSeverity::kInfo, "cobalt/app.cc", 10,
                             "info filtered");
  collector.RecordLogMessage(LogSeverity::kWarning, "cobalt/player.cc", 42,
                             "buffer underrun");
  task_environment_.AdvanceClock(base::Milliseconds(500));
  collector.RecordLogMessage(LogSeverity::kFatal, "cobalt/fatal.cc", 99,
                             "unrecoverable fault");

  task_environment_.FastForwardBy(base::Milliseconds(20));

  // Only Session A (which sent Log.enable) receives Log.entryAdded events.
  EXPECT_EQ(extension_channel.received_messages().size(), 1u);
  ASSERT_EQ(console_channel.received_messages().size(), 2u);

  const base::Value::Dict& warn_msg = console_channel.received_messages()[0];
  EXPECT_EQ(*warn_msg.FindString("method"), "Log.entryAdded");
  const base::Value::Dict* warn_entry =
      warn_msg.FindDictByDottedPath("params.entry");
  ASSERT_NE(warn_entry, nullptr);
  EXPECT_EQ(*warn_entry->FindString("source"), "other");
  EXPECT_EQ(*warn_entry->FindString("level"), "warning");
  EXPECT_EQ(*warn_entry->FindString("url"), "player.cc");
  EXPECT_EQ(warn_entry->FindInt("lineNumber"), std::optional<int>(41));
  std::optional<double> warn_timestamp = warn_entry->FindDouble("timestamp");
  ASSERT_TRUE(warn_timestamp.has_value());
  EXPECT_DOUBLE_EQ(*warn_timestamp, expected_warn_ms);
  EXPECT_NE(warn_entry->FindString("text")->find("buffer underrun"),
            std::string::npos);

  const base::Value::Dict& fatal_msg = console_channel.received_messages()[1];
  const base::Value::Dict* fatal_entry =
      fatal_msg.FindDictByDottedPath("params.entry");
  ASSERT_NE(fatal_entry, nullptr);
  EXPECT_EQ(*fatal_entry->FindString("level"), "error");
  std::optional<double> fatal_timestamp = fatal_entry->FindDouble("timestamp");
  ASSERT_TRUE(fatal_timestamp.has_value());
  EXPECT_NEAR(*fatal_timestamp - *warn_timestamp, 500.0, 0.01);
  EXPECT_NE(fatal_entry->FindString("text")->find("[FATAL]"),
            std::string::npos);
  EXPECT_NE(fatal_entry->FindString("text")->find("unrecoverable fault"),
            std::string::npos);

  delegate.ClientDetached(&extension_channel);
  delegate.ClientDetached(&console_channel);
}

// Covers the developer journey of launching Cobalt with
// --enable-devtools-native-logs to capture early startup logs before DevTools
// connects, then attaching Chrome DevTools Console later and receiving the
// buffered boot backlog while keeping collection active across reconnects.
TEST_F(CobaltDevToolsManagerDelegateTest,
       BootFlagReplaysBacklogOnAttachAndPersistsAcrossDetach) {
  CobaltLogCollector collector(FastConfig());
  CobaltDevToolsManagerDelegate delegate(
      /*browser_context=*/nullptr, &collector,
      /*enable_native_logging_at_startup=*/true);

  EXPECT_TRUE(collector.SnapshotStats().active);
  collector.RecordLogMessage(LogSeverity::kInfo, "cobalt/boot.cc", 12,
                             "early startup log");

  FakeDevToolsChannel console_channel;
  delegate.ClientAttached(&console_channel);
  EXPECT_TRUE(SendCommand(&delegate, &console_channel, 1, "Log.enable"));
  task_environment_.RunUntilIdle();

  ASSERT_EQ(console_channel.received_messages().size(), 1u);
  const base::Value::Dict* entry =
      console_channel.received_messages()[0].FindDictByDottedPath(
          "params.entry");
  ASSERT_NE(entry, nullptr);
  EXPECT_NE(entry->FindString("text")->find("early startup log"),
            std::string::npos);

  // Detaching the last client keeps collection active when launched with the
  // boot flag.
  delegate.ClientDetached(&console_channel);
  EXPECT_TRUE(collector.SnapshotStats().active);
}

// Covers the developer journey of closing Chrome DevTools after an interactive
// debugging session without manually toggling native logs off: detaching the
// last client automatically unsubscribes from CobaltLogCollector and frees the
// ring buffer.
TEST_F(CobaltDevToolsManagerDelegateTest,
       LastClientDetachAutomaticallyDisablesInteractiveLogging) {
  CobaltLogCollector collector(FastConfig());
  CobaltDevToolsManagerDelegate delegate(
      /*browser_context=*/nullptr, &collector,
      /*enable_native_logging_at_startup=*/false);

  FakeDevToolsChannel channel_a;
  FakeDevToolsChannel channel_b;
  delegate.ClientAttached(&channel_a);
  delegate.ClientAttached(&channel_b);

  base::Value::Dict params;
  params.Set("enabled", true);
  EXPECT_FALSE(SendCommand(&delegate, &channel_b, 1, "Cobalt.setNativeLogging",
                           std::move(params)));
  EXPECT_TRUE(collector.SnapshotStats().active);

  // Detaching one client leaves collection active while the other stays open.
  delegate.ClientDetached(&channel_b);
  EXPECT_TRUE(collector.SnapshotStats().active);

  // Detaching the final client automatically releases the collector consumer.
  delegate.ClientDetached(&channel_a);
  EXPECT_FALSE(collector.SnapshotStats().active);
}

// Covers the developer journey of toggling native logging with replayBacklog
// enabled vs. disabled when another consumer has already buffered earlier logs,
// and ensures malformed CDP params return a standard InvalidParams error.
TEST_F(CobaltDevToolsManagerDelegateTest,
       ReplayBacklogFlagAndInvalidParamsHandling) {
  CobaltLogCollector collector(FastConfig());
  auto existing_consumer = collector.RegisterConsumer();
  collector.RecordLogMessage(LogSeverity::kInfo, "cobalt/pre.cc", 5,
                             "preexisting entry");

  CobaltDevToolsManagerDelegate delegate(
      /*browser_context=*/nullptr, &collector,
      /*enable_native_logging_at_startup=*/false);
  FakeDevToolsChannel channel;
  delegate.ClientAttached(&channel);
  EXPECT_TRUE(SendCommand(&delegate, &channel, 1, "Log.enable"));

  // Missing params -> InvalidParams error (-32602).
  EXPECT_FALSE(SendCommand(&delegate, &channel, 2, "Cobalt.setNativeLogging"));
  ASSERT_EQ(channel.received_messages().size(), 1u);
  EXPECT_EQ(channel.received_messages()[0].FindIntByDottedPath("error.code"),
            std::optional<int>(-32602));
  channel.ClearMessages();

  // Invalid severity -> InvalidParams error (-32602).
  base::Value::Dict bad_severity;
  bad_severity.Set("enabled", true);
  bad_severity.Set("minSeverity", "loud");
  EXPECT_FALSE(SendCommand(&delegate, &channel, 3, "Cobalt.setNativeLogging",
                           std::move(bad_severity)));
  ASSERT_EQ(channel.received_messages().size(), 1u);
  EXPECT_EQ(channel.received_messages()[0].FindIntByDottedPath("error.code"),
            std::optional<int>(-32602));
  channel.ClearMessages();

  // Enable with replayBacklog=false skips the pre-existing entry.
  base::Value::Dict no_replay;
  no_replay.Set("enabled", true);
  no_replay.Set("replayBacklog", false);
  EXPECT_FALSE(SendCommand(&delegate, &channel, 4, "Cobalt.setNativeLogging",
                           std::move(no_replay)));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(channel.received_messages().size(), 1u);
  channel.ClearMessages();

  // Calling again with replayBacklog=true replays the buffered entry once
  // because nothing has been delivered to this session yet.
  base::Value::Dict with_replay;
  with_replay.Set("enabled", true);
  with_replay.Set("replayBacklog", true);
  EXPECT_FALSE(SendCommand(&delegate, &channel, 5, "Cobalt.setNativeLogging",
                           std::move(with_replay)));
  task_environment_.RunUntilIdle();
  ASSERT_EQ(channel.received_messages().size(), 2u);
  EXPECT_EQ(*channel.received_messages()[1].FindString("method"),
            "Log.entryAdded");
  EXPECT_NE(channel.received_messages()[1]
                .FindStringByDottedPath("params.entry.text")
                ->find("preexisting entry"),
            std::string::npos);
  channel.ClearMessages();

  // A subsequent replayBacklog=true toggle while already streaming must not
  // replay duplicates into an active Console that has already received logs.
  base::Value::Dict stale_replay;
  stale_replay.Set("enabled", true);
  stale_replay.Set("replayBacklog", true);
  EXPECT_FALSE(SendCommand(&delegate, &channel, 6, "Cobalt.setNativeLogging",
                           std::move(stale_replay)));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(channel.received_messages().size(), 1u);
  channel.ClearMessages();

  // Updating severity live with replayBacklog=false applies immediately to
  // subsequent logs without replaying earlier entries.
  base::Value::Dict change_severity;
  change_severity.Set("enabled", true);
  change_severity.Set("minSeverity", "error");
  change_severity.Set("replayBacklog", false);
  EXPECT_FALSE(SendCommand(&delegate, &channel, 7, "Cobalt.setNativeLogging",
                           std::move(change_severity)));
  channel.ClearMessages();
  collector.RecordLogMessage(LogSeverity::kWarning, "cobalt/live.cc", 1,
                             "warning suppressed after severity change");
  collector.RecordLogMessage(LogSeverity::kError, "cobalt/live.cc", 2,
                             "error passes after severity change");
  task_environment_.FastForwardBy(base::Milliseconds(20));
  ASSERT_EQ(channel.received_messages().size(), 1u);
  EXPECT_NE(channel.received_messages()[0]
                .FindStringByDottedPath("params.entry.text")
                ->find("error passes after severity change"),
            std::string::npos);

  delegate.ClientDetached(&channel);
}

// Covers the developer journey of inspecting a burst of logs larger than a
// single flush batch (128 entries) and logs containing raw non-UTF-8 bytes,
// verifying continuation flushes deliver every entry without stalling the UI
// thread or failing UTF-8 JSON encoding.
TEST_F(CobaltDevToolsManagerDelegateTest,
       LargeBurstFlushesAcrossMultipleBatchesAndSanitizesUtf8) {
  CobaltLogCollector collector(FastConfig());
  CobaltDevToolsManagerDelegate delegate(
      /*browser_context=*/nullptr, &collector,
      /*enable_native_logging_at_startup=*/true);

  constexpr size_t kBurstCount =
      CobaltDevToolsManagerDelegate::kMaxEntriesPerFlush + 25;
  for (size_t i = 0; i < kBurstCount; ++i) {
    std::string msg = "burst #" + std::to_string(i);
    if (i == 0) {
      msg.push_back(static_cast<char>(0xFF));
      msg.push_back(static_cast<char>(0xFE));
    }
    collector.RecordLogMessage(LogSeverity::kInfo, "cobalt/burst.cc",
                               static_cast<int>(i + 1), msg);
  }

  FakeDevToolsChannel console_channel;
  delegate.ClientAttached(&console_channel);
  EXPECT_TRUE(SendCommand(&delegate, &console_channel, 1, "Log.enable"));
  task_environment_.RunUntilIdle();

  ASSERT_EQ(console_channel.received_messages().size(), kBurstCount);
  const std::string* first_text =
      console_channel.received_messages()[0].FindStringByDottedPath(
          "params.entry.text");
  ASSERT_NE(first_text, nullptr);
  EXPECT_NE(first_text->find("burst #0"), std::string::npos);
  // Raw 0xFF 0xFE bytes must be replaced with UTF-8 U+FFFD (\xEF\xBF\xBD).
  EXPECT_NE(first_text->find("\xEF\xBF\xBD"), std::string::npos);
  EXPECT_NE(console_channel.received_messages()[kBurstCount - 1]
                .FindStringByDottedPath("params.entry.text")
                ->find("burst #" + std::to_string(kBurstCount - 1)),
            std::string::npos);

  delegate.ClientDetached(&console_channel);
}

// Covers the developer journey of inspecting a page that emits console.log()
// while native log streaming is enabled: Chromium mirrors JS console messages
// into base::logging with file "CONSOLE", so the delegate must skip them to
// avoid duplicate Console output while leaving them in CobaltLogCollector.
TEST_F(CobaltDevToolsManagerDelegateTest, SkipsMirroredPageConsoleMessages) {
  CobaltLogCollector collector(FastConfig());
  CobaltDevToolsManagerDelegate delegate(
      /*browser_context=*/nullptr, &collector,
      /*enable_native_logging_at_startup=*/true);

  FakeDevToolsChannel console_channel;
  delegate.ClientAttached(&console_channel);
  EXPECT_TRUE(SendCommand(&delegate, &console_channel, 1, "Log.enable"));

  collector.RecordLogMessage(LogSeverity::kInfo, "CONSOLE", 12,
                             "\"js hello\", source: https://app/main.js (12)");
  collector.RecordLogMessage(LogSeverity::kInfo, "cobalt/player.cc", 44,
                             "native playback started");
  task_environment_.FastForwardBy(base::Milliseconds(20));

  // Both entries remain buffered in CobaltLogCollector, but only the native
  // entry is forwarded to DevTools Console.
  EXPECT_EQ(collector.SnapshotStats().buffered_entries, 2u);
  ASSERT_EQ(console_channel.received_messages().size(), 1u);
  const std::string* text =
      console_channel.received_messages()[0].FindStringByDottedPath(
          "params.entry.text");
  ASSERT_NE(text, nullptr);
  EXPECT_NE(text->find("native playback started"), std::string::npos);

  delegate.ClientDetached(&console_channel);
}

// Covers the developer journey of inspecting a page with auto-attached Web
// Workers in Chrome DevTools: only top-level page/browser targets receive
// Log.entryAdded events so native logs are not duplicated once per worker,
// while worker sessions still support Cobalt.* commands and detach tracking.
TEST_F(CobaltDevToolsManagerDelegateTest, SkipsWorkerChildTargetsAsLogSinks) {
  CobaltLogCollector collector(FastConfig());
  CobaltDevToolsManagerDelegate delegate(
      /*browser_context=*/nullptr, &collector,
      /*enable_native_logging_at_startup=*/false);

  auto page_host = base::MakeRefCounted<FakeDevToolsAgentHost>(
      content::DevToolsAgentHost::kTypePage);
  auto worker_host = base::MakeRefCounted<FakeDevToolsAgentHost>(
      content::DevToolsAgentHost::kTypeDedicatedWorker);
  FakeDevToolsChannel page_channel(page_host);
  FakeDevToolsChannel worker_channel(worker_host);

  delegate.ClientAttached(&page_channel);
  delegate.ClientAttached(&worker_channel);
  EXPECT_TRUE(SendCommand(&delegate, &page_channel, 1, "Log.enable"));
  EXPECT_TRUE(SendCommand(&delegate, &worker_channel, 2, "Log.enable"));

  base::Value::Dict enable_params;
  enable_params.Set("enabled", true);
  EXPECT_FALSE(SendCommand(&delegate, &worker_channel, 3,
                           "Cobalt.setNativeLogging",
                           std::move(enable_params)));
  worker_channel.ClearMessages();

  EXPECT_FALSE(SendCommand(&delegate, &worker_channel, 4,
                           "Cobalt.getNativeLoggingState"));
  ASSERT_EQ(worker_channel.received_messages().size(), 1u);
  EXPECT_EQ(worker_channel.received_messages()[0].FindDoubleByDottedPath(
                "result.logEnabledSessions"),
            std::optional<double>(1.0));
  worker_channel.ClearMessages();

  collector.RecordLogMessage(LogSeverity::kInfo, "cobalt/media.cc", 7,
                             "single copy only");
  task_environment_.FastForwardBy(base::Milliseconds(20));

  ASSERT_EQ(page_channel.received_messages().size(), 1u);
  EXPECT_TRUE(worker_channel.received_messages().empty());

  // Detaching the page while the worker session remains open keeps collection
  // active until the final session detaches.
  delegate.ClientDetached(&page_channel);
  EXPECT_TRUE(collector.SnapshotStats().active);
  delegate.ClientDetached(&worker_channel);
  EXPECT_FALSE(collector.SnapshotStats().active);
}

// Covers the developer journey of launching with --enable-devtools-native-logs
// and either explicitly turning logging off interactively or shutting down the
// browser cleanly via ShutdownNativeLogging(), ensuring ring storage is freed.
TEST_F(CobaltDevToolsManagerDelegateTest,
       BootFlagInteractiveDisableAndShutdownSingletonHandoff) {
  // 1) Injected-collector boot-flag delegate disabled interactively.
  {
    CobaltLogCollector collector(FastConfig());
    CobaltDevToolsManagerDelegate delegate(
        /*browser_context=*/nullptr, &collector,
        /*enable_native_logging_at_startup=*/true);
    FakeDevToolsChannel channel;
    delegate.ClientAttached(&channel);
    EXPECT_TRUE(collector.SnapshotStats().active);

    base::Value::Dict disable_params;
    disable_params.Set("enabled", false);
    EXPECT_FALSE(SendCommand(&delegate, &channel, 1, "Cobalt.setNativeLogging",
                             std::move(disable_params)));
    EXPECT_FALSE(collector.SnapshotStats().active);
    delegate.ClientDetached(&channel);
  }

  // 2) Singleton boot-flag handoff and ShutdownNativeLogging() release path.
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kEnableDevToolsNativeLogs);
  base::ScopedClosureRunner cleanup(base::BindLambdaForTesting([]() {
    CobaltDevToolsManagerDelegate::ShutdownNativeLogging();
    CobaltLogCollector::GetInstance()->UninstallHooksForTesting();
  }));

  CobaltDevToolsManagerDelegate::InitializeNativeLogging();
  EXPECT_TRUE(CobaltLogCollector::GetInstance()->SnapshotStats().active);

  CobaltDevToolsManagerDelegate singleton_delegate(/*browser_context=*/nullptr);
  EXPECT_TRUE(CobaltLogCollector::GetInstance()->SnapshotStats().active);

  // Interactive disable must free the ring buffer immediately, proving
  // constructor handoff released GetBootLogConsumer().
  FakeDevToolsChannel singleton_channel;
  singleton_delegate.ClientAttached(&singleton_channel);
  base::Value::Dict singleton_disable;
  singleton_disable.Set("enabled", false);
  EXPECT_FALSE(SendCommand(&singleton_delegate, &singleton_channel, 1,
                           "Cobalt.setNativeLogging",
                           std::move(singleton_disable)));
  EXPECT_FALSE(CobaltLogCollector::GetInstance()->SnapshotStats().active);

  // Re-enable interactively, then verify ShutdownNativeLogging() releases the
  // live delegate's consumer during browser shutdown.
  base::Value::Dict singleton_enable;
  singleton_enable.Set("enabled", true);
  EXPECT_FALSE(SendCommand(&singleton_delegate, &singleton_channel, 2,
                           "Cobalt.setNativeLogging",
                           std::move(singleton_enable)));
  EXPECT_TRUE(CobaltLogCollector::GetInstance()->SnapshotStats().active);

  CobaltDevToolsManagerDelegate::ShutdownNativeLogging();
  EXPECT_FALSE(CobaltLogCollector::GetInstance()->SnapshotStats().active);
  singleton_delegate.ClientDetached(&singleton_channel);
}

// Covers the edge case where a DevTools client disconnects synchronously while
// FlushPendingLogs() is delivering a batch: remaining entries for the detached
// client stop cleanly without UAF and sibling Console sessions still receive
// the full batch.
TEST_F(CobaltDevToolsManagerDelegateTest,
       SelfDetachingChannelDuringFlushDoesNotCrash) {
  CobaltLogCollector collector(FastConfig());
  CobaltDevToolsManagerDelegate delegate(
      /*browser_context=*/nullptr, &collector,
      /*enable_native_logging_at_startup=*/true);

  FakeDevToolsChannel detaching_channel;
  FakeDevToolsChannel healthy_channel;
  delegate.ClientAttached(&detaching_channel);
  delegate.ClientAttached(&healthy_channel);
  EXPECT_TRUE(SendCommand(&delegate, &detaching_channel, 1, "Log.enable"));
  EXPECT_TRUE(SendCommand(&delegate, &healthy_channel, 2, "Log.enable"));

  bool detached = false;
  detaching_channel.SetOnDispatch(
      base::BindLambdaForTesting([&](const base::Value::Dict&) {
        if (!detached) {
          detached = true;
          delegate.ClientDetached(&detaching_channel);
        }
      }));

  collector.RecordLogMessage(LogSeverity::kInfo, "cobalt/flush.cc", 1, "msg 1");
  collector.RecordLogMessage(LogSeverity::kInfo, "cobalt/flush.cc", 2, "msg 2");
  collector.RecordLogMessage(LogSeverity::kInfo, "cobalt/flush.cc", 3, "msg 3");
  task_environment_.FastForwardBy(base::Milliseconds(20));

  EXPECT_TRUE(detached);
  EXPECT_EQ(detaching_channel.received_messages().size(), 1u);
  EXPECT_EQ(healthy_channel.received_messages().size(), 3u);

  delegate.ClientDetached(&healthy_channel);
}

}  // namespace
}  // namespace cobalt
