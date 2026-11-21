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
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "cobalt/browser/logging/cobalt_log_collector.h"
#include "content/public/browser/devtools_agent_host_client.h"
#include "content/public/browser/devtools_agent_host_client_channel.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/inspector_protocol/crdtp/json.h"
#include "third_party/inspector_protocol/crdtp/span.h"

namespace cobalt {
namespace {

using LogSeverity = CobaltLogCollector::LogSeverity;

class FakeDevToolsAgentHostClient : public content::DevToolsAgentHostClient {
 public:
  void DispatchProtocolMessage(content::DevToolsAgentHost* agent_host,
                               base::span<const uint8_t> message) override {}
  void AgentHostClosed(content::DevToolsAgentHost* agent_host) override {}
};

class FakeDevToolsChannel : public content::DevToolsAgentHostClientChannel {
 public:
  FakeDevToolsChannel() = default;
  ~FakeDevToolsChannel() override = default;

  void DispatchProtocolMessageToClient(std::vector<uint8_t> message) override {
    std::string json;
    ASSERT_TRUE(
        crdtp::json::ConvertCBORToJSON(crdtp::SpanFrom(message), &json).ok());
    std::optional<base::Value::Dict> parsed = base::JSONReader::ReadDict(json);
    ASSERT_TRUE(parsed.has_value()) << "Failed to parse JSON: " << json;
    received_messages_.push_back(std::move(*parsed));
  }

  content::DevToolsAgentHost* GetAgentHost() override { return nullptr; }
  content::DevToolsAgentHostClient* GetClient() override { return &client_; }

  const std::vector<base::Value::Dict>& received_messages() const {
    return received_messages_;
  }

  void ClearMessages() { received_messages_.clear(); }

 private:
  FakeDevToolsAgentHostClient client_;
  std::vector<base::Value::Dict> received_messages_;
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

  // Record INFO (filtered out), WARNING, and FATAL messages.
  collector.RecordLogMessage(LogSeverity::kInfo, "cobalt/app.cc", 10,
                             "info filtered");
  collector.RecordLogMessage(LogSeverity::kWarning, "cobalt/player.cc", 42,
                             "buffer underrun");
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
  EXPECT_NE(warn_entry->FindString("text")->find("buffer underrun"),
            std::string::npos);

  const base::Value::Dict& fatal_msg = console_channel.received_messages()[1];
  const base::Value::Dict* fatal_entry =
      fatal_msg.FindDictByDottedPath("params.entry");
  ASSERT_NE(fatal_entry, nullptr);
  EXPECT_EQ(*fatal_entry->FindString("level"), "error");
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

  // Calling again with replayBacklog=true replays the buffered entry.
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
  EXPECT_NE(console_channel.received_messages()[0]
                .FindStringByDottedPath("params.entry.text")
                ->find("burst #0"),
            std::string::npos);
  EXPECT_NE(console_channel.received_messages()[kBurstCount - 1]
                .FindStringByDottedPath("params.entry.text")
                ->find("burst #" + std::to_string(kBurstCount - 1)),
            std::string::npos);

  delegate.ClientDetached(&console_channel);
}

}  // namespace
}  // namespace cobalt
