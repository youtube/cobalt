/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "test/test_helper.h"

#include "perfetto/base/compiler.h"
#include "perfetto/ext/tracing/core/trace_packet.h"
#include "perfetto/ext/tracing/ipc/default_socket.h"
#include "perfetto/tracing/core/tracing_service_state.h"

#include "protos/perfetto/trace/trace_packet.pbzero.h"

namespace perfetto {

namespace {
const char* ProducerSocketForMode(TestHelper::Mode mode) {
#if !PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID)
  base::ignore_result(mode);
  return ::perfetto::GetProducerSocket();
#else
  switch (mode) {
    case TestHelper::Mode::kStartDaemons:
      return "/data/local/tmp/traced_producer";
    case TestHelper::Mode::kUseSystemService:
      return ::perfetto::GetProducerSocket();
  }
#endif
}

const char* ConsumerSocketForMode(TestHelper::Mode mode) {
#if !PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID)
  base::ignore_result(mode);
  return ::perfetto::GetConsumerSocket();
#else
  switch (mode) {
    case TestHelper::Mode::kStartDaemons:
      return "/data/local/tmp/traced_consumer";
    case TestHelper::Mode::kUseSystemService:
      return ::perfetto::GetConsumerSocket();
  }
#endif
}
}  // namespace

uint64_t TestHelper::next_instance_num_ = 0;
#if PERFETTO_BUILDFLAG(PERFETTO_START_DAEMONS)
TestHelper::Mode TestHelper::kDefaultMode = Mode::kStartDaemons;
#else
TestHelper::Mode TestHelper::kDefaultMode = Mode::kUseSystemService;
#endif

TestHelper::TestHelper(base::TestTaskRunner* task_runner, Mode mode)
    : instance_num_(next_instance_num_++),
      task_runner_(task_runner),
      mode_(mode),
      producer_socket_(ProducerSocketForMode(mode)),
      consumer_socket_(ConsumerSocketForMode(mode)),
      service_thread_(producer_socket_, consumer_socket_),
      fake_producer_thread_(producer_socket_,
                            WrapTask(CreateCheckpoint("producer.connect")),
                            WrapTask(CreateCheckpoint("producer.setup")),
                            WrapTask(CreateCheckpoint("producer.enabled"))) {}

void TestHelper::OnConnect() {
  std::move(on_connect_callback_)();
}

void TestHelper::OnDisconnect() {
  PERFETTO_FATAL("Consumer unexpectedly disconnected from the service");
}

void TestHelper::OnTracingDisabled(const std::string& /*error*/) {
  std::move(on_stop_tracing_callback_)();
  on_stop_tracing_callback_ = nullptr;
}

void TestHelper::ReadTraceData(std::vector<TracePacket> packets) {
  for (auto& encoded_packet : packets) {
    protos::gen::TracePacket packet;
    PERFETTO_CHECK(
        packet.ParseFromString(encoded_packet.GetRawBytesForTesting()));
    full_trace_.push_back(packet);
    if (packet.has_clock_snapshot() || packet.has_trace_uuid() ||
        packet.has_trace_config() || packet.has_trace_stats() ||
        !packet.synchronization_marker().empty() || packet.has_system_info() ||
        packet.has_service_event()) {
      continue;
    }
    PERFETTO_CHECK(packet.has_trusted_uid());
    trace_.push_back(std::move(packet));
  }
}

void TestHelper::OnTraceData(std::vector<TracePacket> packets, bool has_more) {
  ReadTraceData(std::move(packets));
  if (!has_more) {
    std::move(on_packets_finished_callback_)();
  }
}

void TestHelper::StartServiceIfRequired() {
  if (mode_ == Mode::kStartDaemons)
    env_cleaner_ = service_thread_.Start();
}

void TestHelper::RestartService() {
  PERFETTO_CHECK(mode_ == Mode::kStartDaemons);
  service_thread_.Stop();
  service_thread_.Start();
}

FakeProducer* TestHelper::ConnectFakeProducer() {
  fake_producer_thread_.Connect();
  // This will wait until the service has seen the RegisterDataSource() call
  // (because of the Sync() in FakeProducer::OnConnect()).
  RunUntilCheckpoint("producer.connect");
  return fake_producer_thread_.producer();
}

void TestHelper::ConnectConsumer() {
  cur_consumer_num_++;
  on_connect_callback_ = CreateCheckpoint("consumer.connected." +
                                          std::to_string(cur_consumer_num_));
  endpoint_ = ConsumerIPCClient::Connect(consumer_socket_, this, task_runner_);
}

void TestHelper::DetachConsumer(const std::string& key) {
  on_detach_callback_ = CreateCheckpoint("detach." + key);
  endpoint_->Detach(key);
  RunUntilCheckpoint("detach." + key);
  endpoint_.reset();
}

bool TestHelper::AttachConsumer(const std::string& key) {
  bool success = false;
  auto checkpoint = CreateCheckpoint("attach." + key);
  on_attach_callback_ = [&success, checkpoint](bool s) {
    success = s;
    checkpoint();
  };
  endpoint_->Attach(key);
  RunUntilCheckpoint("attach." + key);
  return success;
}

void TestHelper::CreateProducerProvidedSmb() {
  fake_producer_thread_.CreateProducerProvidedSmb();
}

bool TestHelper::IsShmemProvidedByProducer() {
  return fake_producer_thread_.producer()->IsShmemProvidedByProducer();
}

void TestHelper::ProduceStartupEventBatch(
    const protos::gen::TestConfig& config) {
  auto on_data_written = CreateCheckpoint("startup_data_written");
  fake_producer_thread_.ProduceStartupEventBatch(config,
                                                 WrapTask(on_data_written));
  RunUntilCheckpoint("startup_data_written");
}

void TestHelper::StartTracing(const TraceConfig& config,
                              base::ScopedFile file) {
  PERFETTO_CHECK(!on_stop_tracing_callback_);
  trace_.clear();
  on_stop_tracing_callback_ =
      CreateCheckpoint("stop.tracing" + std::to_string(++trace_count_));
  endpoint_->EnableTracing(config, std::move(file));
}

void TestHelper::DisableTracing() {
  endpoint_->DisableTracing();
}

void TestHelper::FlushAndWait(uint32_t timeout_ms) {
  static int flush_num = 0;
  std::string checkpoint_name = "flush." + std::to_string(flush_num++);
  auto checkpoint = CreateCheckpoint(checkpoint_name);
  endpoint_->Flush(timeout_ms, [checkpoint](bool) { checkpoint(); });
  RunUntilCheckpoint(checkpoint_name, timeout_ms + 1000);
}

void TestHelper::ReadData(uint32_t read_count) {
  on_packets_finished_callback_ =
      CreateCheckpoint("readback.complete." + std::to_string(read_count));
  endpoint_->ReadBuffers();
}

void TestHelper::FreeBuffers() {
  endpoint_->FreeBuffers();
}

void TestHelper::WaitForConsumerConnect() {
  RunUntilCheckpoint("consumer.connected." + std::to_string(cur_consumer_num_));
}

void TestHelper::WaitForProducerSetup() {
  RunUntilCheckpoint("producer.setup");
}

void TestHelper::WaitForProducerEnabled() {
  RunUntilCheckpoint("producer.enabled");
}

void TestHelper::WaitForTracingDisabled(uint32_t timeout_ms) {
  RunUntilCheckpoint(std::string("stop.tracing") + std::to_string(trace_count_),
                     timeout_ms);
}

void TestHelper::WaitForReadData(uint32_t read_count, uint32_t timeout_ms) {
  RunUntilCheckpoint("readback.complete." + std::to_string(read_count),
                     timeout_ms);
}

void TestHelper::WaitFor(std::function<bool()> predicate,
                         const std::string& error_msg,
                         uint32_t timeout_ms) {
  int64_t deadline_ms = base::GetWallTimeMs().count() + timeout_ms;
  while (base::GetWallTimeMs().count() < deadline_ms) {
    if (predicate())
      return;
    base::SleepMicroseconds(500 * 1000);  // 0.5 s.
  }
  PERFETTO_FATAL("Test timed out waiting for: %s", error_msg.c_str());
}

void TestHelper::WaitForDataSourceConnected(const std::string& ds_name) {
  auto predicate = [&] {
    auto dss = QueryServiceStateAndWait().data_sources();
    return std::any_of(dss.begin(), dss.end(),
                       [&](const TracingServiceState::DataSource& ds) {
                         return ds.ds_descriptor().name() == ds_name;
                       });
  };
  WaitFor(predicate, "connection of data source " + ds_name);
}

void TestHelper::SyncAndWaitProducer() {
  static int sync_id = 0;
  std::string checkpoint_name = "producer_sync_" + std::to_string(++sync_id);
  auto checkpoint = CreateCheckpoint(checkpoint_name);
  fake_producer_thread_.producer()->Sync(
      [this, &checkpoint] { task_runner_->PostTask(checkpoint); });
  RunUntilCheckpoint(checkpoint_name);
}

TracingServiceState TestHelper::QueryServiceStateAndWait() {
  TracingServiceState res;
  static int n = 0;
  std::string checkpoint_name = "query_svc_state_" + std::to_string(n++);
  auto checkpoint = CreateCheckpoint(checkpoint_name);
  auto callback = [&checkpoint, &res](bool, const TracingServiceState& tss) {
    res = tss;
    checkpoint();
  };
  endpoint_->QueryServiceState(callback);
  RunUntilCheckpoint(checkpoint_name);
  return res;
}

std::function<void()> TestHelper::WrapTask(
    const std::function<void()>& function) {
  return [this, function] { task_runner_->PostTask(function); };
}

void TestHelper::OnDetach(bool) {
  if (on_detach_callback_)
    std::move(on_detach_callback_)();
}

void TestHelper::OnAttach(bool success, const TraceConfig&) {
  if (on_attach_callback_)
    std::move(on_attach_callback_)(success);
}

void TestHelper::OnTraceStats(bool, const TraceStats&) {}

void TestHelper::OnObservableEvents(const ObservableEvents&) {}

void TestHelper::OnSessionCloned(bool, const std::string&) {}

// static
const char* TestHelper::GetDefaultModeConsumerSocketName() {
  return ConsumerSocketForMode(TestHelper::kDefaultMode);
}

// static
const char* TestHelper::GetDefaultModeProducerSocketName() {
  return ProducerSocketForMode(TestHelper::kDefaultMode);
}

}  // namespace perfetto
