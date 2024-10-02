// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_channel_manager.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include "base/test/test_trace_processor.h"
#include "base/test/trace_event_analyzer.h"
#include "base/test/trace_test_utils.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/context_result.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/ipc/common/command_buffer_id.h"
#include "gpu/ipc/common/gpu_channel.mojom.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_test_common.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/perfetto/include/perfetto/tracing/tracing.h"

namespace gpu {

class GpuChannelManagerTest : public GpuChannelTestCommon {
 public:
  static constexpr uint64_t kUInt64_T_Max =
      std::numeric_limits<uint64_t>::max();

  GpuChannelManagerTest()
      : GpuChannelTestCommon(true /* use_stub_bindings */) {}
  ~GpuChannelManagerTest() override = default;

  GpuChannelManager::GpuPeakMemoryMonitor* gpu_peak_memory_monitor() {
    return &channel_manager()->peak_memory_monitor_;
  }

  // Returns the peak memory usage from the channel_manager(). This will stop
  // tracking for |sequence_number|.
  uint64_t GetManagersPeakMemoryUsage(uint32_t sequence_num) {
    // Set default as max so that invalid cases can properly test 0u returns.
    uint64_t peak_memory = kUInt64_T_Max;
    auto allocation =
        channel_manager()->GetPeakMemoryUsage(sequence_num, &peak_memory);
    return peak_memory;
  }

  // Returns the peak memory usage currently stores in the GpuPeakMemoryMonitor.
  // Does not shut down tracking for |sequence_num|.
  uint64_t GetMonitorsPeakMemoryUsage(uint32_t sequence_num) {
    // Set default as max so that invalid cases can properly test 0u returns.
    uint64_t peak_memory = kUInt64_T_Max;
    auto allocation =
        channel_manager()->peak_memory_monitor_.GetPeakMemoryUsage(
            sequence_num, &peak_memory);
    return peak_memory;
  }

  // Helpers to call MemoryTracker::Observer methods of
  // GpuChannelManager::GpuPeakMemoryMonitor.
  void OnMemoryAllocatedChange(CommandBufferId id,
                               uint64_t old_size,
                               uint64_t new_size) {
    static_cast<MemoryTracker::Observer*>(gpu_peak_memory_monitor())
        ->OnMemoryAllocatedChange(id, old_size, new_size,
                                  GpuPeakMemoryAllocationSource::UNKNOWN);
  }

#if BUILDFLAG(IS_ANDROID)
  void TestApplicationBackgrounded(ContextType type,
                                   bool should_destroy_channel) {
    ASSERT_TRUE(channel_manager());

    int32_t kClientId = 1;
    GpuChannel* channel = CreateChannel(kClientId, true);
    EXPECT_TRUE(channel);

    int32_t kRouteId =
        static_cast<int32_t>(GpuChannelReservedRoutes::kMaxValue) + 1;
    const SurfaceHandle kFakeSurfaceHandle = 1;
    SurfaceHandle surface_handle = kFakeSurfaceHandle;
    auto init_params = mojom::CreateCommandBufferParams::New();
    init_params->surface_handle = surface_handle;
    init_params->share_group_id = MSG_ROUTING_NONE;
    init_params->stream_id = 0;
    init_params->stream_priority = SchedulingPriority::kNormal;
    init_params->attribs = ContextCreationAttribs();
    init_params->attribs.context_type = type;
    init_params->active_url = GURL();

    ContextResult result = ContextResult::kFatalFailure;
    Capabilities capabilities;
    CreateCommandBuffer(*channel, std::move(init_params), kRouteId,
                        GetSharedMemoryRegion(), &result, &capabilities);
    EXPECT_EQ(result, ContextResult::kSuccess);

    auto raster_decoder_state =
        channel_manager()->GetSharedContextState(&result);
    EXPECT_EQ(result, ContextResult::kSuccess);
    ASSERT_TRUE(raster_decoder_state);

    CommandBufferStub* stub = channel->LookupCommandBuffer(kRouteId);
    EXPECT_TRUE(stub);

    channel_manager()->OnBackgroundCleanup();

    channel = channel_manager()->LookupChannel(kClientId);
    if (should_destroy_channel) {
      EXPECT_FALSE(channel);
    } else {
      EXPECT_TRUE(channel);
    }

    // We should always clear the shared raster state on background cleanup.
    ASSERT_NE(channel_manager()->GetSharedContextState(&result).get(),
              raster_decoder_state.get());
  }
#endif

#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  // TODO(crbug.com/1420217): Simplify this with a generic helper to control the
  // test's tracing in ::base::test.
  std::unique_ptr<perfetto::TracingSession> StartNewTraceBlocking() {
    std::unique_ptr<perfetto::TracingSession> session =
        perfetto::Tracing::NewTrace();
    auto config = ::base::test::TracingEnvironment::GetDefaultTraceConfig();
    for (auto& data_source : *config.mutable_data_sources()) {
      perfetto::protos::gen::TrackEventConfig track_event_config;
      track_event_config.add_disabled_categories("*");
      track_event_config.add_enabled_categories("gpu");
      data_source.mutable_config()->set_track_event_config_raw(
          track_event_config.SerializeAsString());
    }
    session->Setup(config);
    session->StartBlocking();
    return session;
  }

  std::vector<char> StopAndReadTraceBlocking(
      std::unique_ptr<perfetto::TracingSession> session) {
    base::TrackEvent::Flush();
    session->StopBlocking();
    return session->ReadTraceBlocking();
  }

 private:
  ::base::test::TracingEnvironment tracing_environment_;
#endif  // BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
};

TEST_F(GpuChannelManagerTest, EstablishChannel) {
  int32_t kClientId = 1;
  uint64_t kClientTracingId = 1;

  ASSERT_TRUE(channel_manager());
  GpuChannel* channel = channel_manager()->EstablishChannel(
      base::UnguessableToken::Create(), kClientId, kClientTracingId, false);
  EXPECT_TRUE(channel);
  EXPECT_EQ(channel_manager()->LookupChannel(kClientId), channel);
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(GpuChannelManagerTest, OnBackgroundedWithoutWebGL) {
  TestApplicationBackgrounded(CONTEXT_TYPE_OPENGLES2, true);
}

TEST_F(GpuChannelManagerTest, OnBackgroundedWithWebGL) {
  TestApplicationBackgrounded(CONTEXT_TYPE_WEBGL2, false);
}

#endif

// Tests that peak memory usage is only reported for valid sequence numbers,
// and that polling shuts down the monitoring.
TEST_F(GpuChannelManagerTest, GpuPeakMemoryOnlyReportedForValidSequence) {
#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  std::unique_ptr<perfetto::TracingSession> session = StartNewTraceBlocking();
#else
  // TODO(crbug.com/1006541): Remove trace_analyzer usage after migration to the
  // SDK.
  trace_analyzer::Start("gpu");
#endif

  GpuChannelManager* manager = channel_manager();
  const CommandBufferId buffer_id =
      CommandBufferIdFromChannelAndRoute(42, 1337);
  const uint64_t current_memory = 42;
  OnMemoryAllocatedChange(buffer_id, 0u, current_memory);

  const uint32_t sequence_num = 1;
  manager->StartPeakMemoryMonitor(sequence_num);
  EXPECT_EQ(current_memory, GetMonitorsPeakMemoryUsage(sequence_num));

  // With no request to listen to memory it should report 0.
  const uint32_t invalid_sequence_num = 1337;
  EXPECT_EQ(0u, GetMonitorsPeakMemoryUsage(invalid_sequence_num));
  EXPECT_EQ(0u, GetManagersPeakMemoryUsage(invalid_sequence_num));

  // The valid sequence should receive a report.
  EXPECT_EQ(current_memory, GetManagersPeakMemoryUsage(sequence_num));
  // However it should be shut-down and no longer report anything.
  EXPECT_EQ(0u, GetMonitorsPeakMemoryUsage(sequence_num));
  EXPECT_EQ(0u, GetManagersPeakMemoryUsage(sequence_num));

#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  std::vector<char> raw_trace = StopAndReadTraceBlocking(std::move(session));
  ASSERT_FALSE(raw_trace.empty());
  base::test::TestTraceProcessor trace_processor;
  auto status = trace_processor.ParseTrace(raw_trace);
  ASSERT_TRUE(status.ok()) << status.message();
  std::string query =
      R"(
      SELECT
        EXTRACT_ARG(arg_set_id, 'debug.start') AS start,
        (
          SELECT COUNT(*)
          FROM args
          WHERE args.arg_set_id = slice.arg_set_id
                AND args.key GLOB 'debug.start_sources*'
        ) > 0 AS has_start_sources,
        EXTRACT_ARG(arg_set_id, 'debug.peak') AS peak,
        (
          SELECT COUNT(*)
          FROM args
          WHERE args.arg_set_id = slice.arg_set_id
                AND args.key GLOB 'debug.end_sources*'
        ) > 0 AS has_end_sources
      FROM slice
      where name = 'PeakMemoryTracking'
      ORDER BY ts ASC
      )";
  EXPECT_THAT(trace_processor.ExecuteQuery(query),
              ::testing::ElementsAre(
                  std::vector<std::string>{"start", "has_start_sources", "peak",
                                           "has_end_sources"},
                  std::vector<std::string>{
                      base::StringPrintf("%" PRIu64, current_memory), "1",
                      base::StringPrintf("%" PRIu64, current_memory), "1"}));
#else   // BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  auto analyzer = trace_analyzer::Stop();
  trace_analyzer::TraceEventVector events;
  analyzer->FindEvents(trace_analyzer::Query::EventNameIs("PeakMemoryTracking"),
                       &events);

  EXPECT_EQ(2u, events.size());

  ASSERT_TRUE(events[0]->HasNumberArg("start"));
  EXPECT_EQ(current_memory,
            static_cast<uint64_t>(events[0]->GetKnownArgAsDouble("start")));
  ASSERT_TRUE(events[0]->HasDictArg("start_sources"));
  EXPECT_FALSE(events[0]->GetKnownArgAsDict("start_sources").empty());

  const int kEndArgumentsSliceIndex = 1;
  ASSERT_TRUE(events[kEndArgumentsSliceIndex]->HasNumberArg("peak"));
  EXPECT_EQ(current_memory,
            static_cast<uint64_t>(
                events[kEndArgumentsSliceIndex]->GetKnownArgAsDouble("peak")));
  ASSERT_TRUE(events[kEndArgumentsSliceIndex]->HasDictArg("end_sources"));
  EXPECT_FALSE(events[kEndArgumentsSliceIndex]
                   ->GetKnownArgAsDict("end_sources")
                   .empty());
#endif  // BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
}

// Tests that while a channel may exist for longer than a request to monitor,
// that only peaks seen are reported.
TEST_F(GpuChannelManagerTest,
       GpuPeakMemoryOnlyReportsPeaksFromObservationTime) {
  GpuChannelManager* manager = channel_manager();

  const CommandBufferId buffer_id =
      CommandBufferIdFromChannelAndRoute(42, 1337);
  const uint64_t initial_memory = 42;
  OnMemoryAllocatedChange(buffer_id, 0u, initial_memory);
  const uint64_t reduced_memory = 2;
  OnMemoryAllocatedChange(buffer_id, initial_memory, reduced_memory);

  const uint32_t sequence_num = 1;
  manager->StartPeakMemoryMonitor(sequence_num);
  EXPECT_EQ(reduced_memory, GetMonitorsPeakMemoryUsage(sequence_num));

  // While not the peak memory for the lifetime of |buffer_id| this should be
  // the peak seen during the observation of |sequence_num|.
  const uint64_t localized_peak_memory = 24;
  OnMemoryAllocatedChange(buffer_id, reduced_memory, localized_peak_memory);
  EXPECT_EQ(localized_peak_memory, GetManagersPeakMemoryUsage(sequence_num));
}

// Checks that when there are more than one sequence, that each has a separately
// calulcated peak.
TEST_F(GpuChannelManagerTest, GetPeakMemoryUsageCalculatedPerSequence) {
  GpuChannelManager* manager = channel_manager();

  const CommandBufferId buffer_id =
      CommandBufferIdFromChannelAndRoute(42, 1337);
  const uint64_t initial_memory = 42;
  OnMemoryAllocatedChange(buffer_id, 0u, initial_memory);

  // Start the first sequence so it is the only one to see the peak of
  // |initial_memory|.
  const uint32_t sequence_num_1 = 1;
  manager->StartPeakMemoryMonitor(sequence_num_1);

  // Reduce the memory before the second sequence starts.
  const uint64_t reduced_memory = 2;
  OnMemoryAllocatedChange(buffer_id, initial_memory, reduced_memory);

  const uint32_t sequence_num_2 = 2;
  manager->StartPeakMemoryMonitor(sequence_num_2);
  const uint64_t localized_peak_memory = 24;
  OnMemoryAllocatedChange(buffer_id, reduced_memory, localized_peak_memory);

  EXPECT_EQ(initial_memory, GetManagersPeakMemoryUsage(sequence_num_1));
  EXPECT_EQ(localized_peak_memory, GetManagersPeakMemoryUsage(sequence_num_2));
}

}  // namespace gpu
